#include "vehicle_hook.h"
#include "plugin.h"
#include "logging.h"
#include "sources.h"
#include "config.h"
#include "wheel.h"
#include "vehicle_physics_ext.h"
#include "rtti_offsets.h"

#include <RED4ext/Relocation.hpp>
#include <RED4ext/Api/v1/Sdk.hpp>
#include <RED4ext/Scripting/Natives/vehicleBaseObject.hpp>
#include <RED4ext/RTTISystem.hpp>
#include <RED4ext/RTTITypes.hpp>
#include <RED4ext/CName.hpp>
#include <RED4ext/CNamePool.hpp>
#include <RED4ext/Handle.hpp>
#include <RED4ext/ISerializable.hpp>
#include <RED4ext/Scripting/CProperty.hpp>
#include <RED4ext/Scripting/Utils.hpp>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace gwheel::vehicle_hook
{
    namespace
    {
        // vehicle::BaseObject::UpdateVehicleCameraInput(self)
        // Hash from Let There Be Flight (MIT licensed). Resolved per-patch
        // by RED4ext.dll's address database. Fires per-vehicle per-tick;
        // used here to write wheel axis values into the player vehicle's
        // input struct each frame. Non-player vehicles are skipped so
        // traffic AI keeps driving normally.
        using UpdateVehicleCameraInputFn = void (*)(void*);
        constexpr uint32_t kUpdateVehicleCameraInputHash = 501486464u;

        // Input fields in vehicle::BaseObject for game build 5294808 (CP2077
        // v2.31). The RED4ext SDK struct at vehicleBaseObject.hpp labels
        // +0x264 as `acceleration` and +0x268 as `deceleration`; we overwrite
        // them after vanilla g_original(self) runs so the values land as
        // processed drive commands. The +0x278 steer field sits inside the
        // SDK's unk26C[0x2A3-0x26C] gap (no label), so we keep that one as
        // an empirical constant — found via field-probe sweep 2026-04-21.
        // Re-probe steer if CDPR patches the vehicleBaseObject struct.
        namespace off
        {
            constexpr std::ptrdiff_t kInputSteer = 0x278; // float, [-1..1], += right
        }

        UpdateVehicleCameraInputFn g_original = nullptr;
        void* g_target = nullptr;

        std::atomic<uint64_t> g_fireCount{0};
        std::atomic<uint64_t> g_injectCount{0};
        std::atomic<bool>     g_attached{false};

        // Cached pointer of the player's currently-mounted vehicle. The
        // detour fires for many vehicles each tick (parked cars, visible
        // traffic with active camera updates, etc.); without this filter,
        // our input injection writes to every one of them and remote-drives
        // them all. Set by the redscript mount/unmount event wrappers via
        // GWheel_Set/ClearPlayerVehicle natives. nullptr = no injection.
        std::atomic<void*>    g_playerVehicle{nullptr};

        // Cached RTTI handle for vehicleBaseObject::GetCurrentSpeed, looked
        // up once on first use. Calling it via ExecuteFunction(self, fn, &out)
        // returns forward speed in m/s with sign preserved (negative when
        // reversing). Null means lookup failed and we've logged it once.
        std::atomic<RED4ext::CClassFunction*> g_getCurrentSpeedFn{nullptr};
        std::atomic<bool>                     g_getCurrentSpeedLookupTried{false};

        // Cached RTTI handle for vehicleBaseObject::GetVehiclePS(), which
        // returns a handle to the vehicle's persistent-state component
        // (native type vehiclePersistentDataPS). The PS stores a
        // StaticArray<WheelRuntimePSData, 4> at offset 0x78; each entry
        // begins with a CName for the material currently under that
        // wheel (previousTouchedMaterial). That's the game-authored
        // surface signal (asphalt / dirt / gravel / metal / etc.) that
        // CP2077 drives its controller rumble off.
        std::atomic<RED4ext::CClassFunction*> g_getVehiclePSFn{nullptr};
        std::atomic<bool>                     g_getVehiclePSLookupTried{false};

        // Per-wheel material hashes from the last tick (CName hashes are
        // uint64_t). Initialised to 0; any non-zero change is logged.
        // Index 0-3 = wheel 0-3 (typically FL / FR / RL / RR).
        std::atomic<uint64_t> g_lastWheelMat[4]{};

        // wheelRuntimeData offset on the handle returned by GetVehiclePS().
        // GetVehiclePS() actually returns a `VehicleComponentPS` instance,
        // which is a SIBLING class of the native `vehiclePersistentDataPS`
        // (both extend `gameComponentPS`) — they are NOT the same type,
        // despite sharing the "PS" conceptual role. VehicleComponentPS
        // does not expose wheelRuntimeData on this build, so once we
        // discover that fact we latch g_surfaceLookupFailed and skip the
        // probe + read entirely. A future pass could walk to the real
        // vehiclePersistentDataPS via the persistency system (lookup by
        // persistentID) or by probing for a direct pointer on the vehicle
        // struct.
        std::atomic<uint32_t> g_psWheelDataOffset{0};
        std::atomic<bool>     g_surfaceLookupFailed{false};
        constexpr std::ptrdiff_t kPS_wheelEntryStride = 0x18;

        // Last vehicle pointer for which we've dumped per-car physics values
        // to the log. Cleared in SetPlayerVehicle when a new vehicle mounts,
        // so every fresh ride prints its turnRate / wheelbase / slip-curve
        // etc. once to the log.
        std::atomic<void*> g_lastPhysicsLoggedVehicle{nullptr};

        // Cached per-tick yaw reference (rad/s) derived from the current
        // vehicle's WheeledPhysics::turnRate. Zero means "fall back to the
        // hardcoded default" (e.g. tanks, vehicles without accessible
        // physics, first-tick race before the read succeeds). Updated by
        // the detour each tick.
        std::atomic<float> g_perCarYawRef{0.f};

        // Previous tick's angular velocity, used to compute the suspension-
        // activity signal that drives the road-surface FFB. Only the roll
        // (X) and pitch (Y) axes matter — yaw (Z) tracks steering input
        // rather than the road. Initialised to zero; first-tick delta is
        // effectively the current ω, which settles within a tick.
        std::atomic<float> g_prevAngVelX{0.f};
        std::atomic<float> g_prevAngVelY{0.f};

        // Resolve vehicleBaseObject::GetVehiclePS once, return null on
        // failure (logged once). Pattern mirrors ReadVehicleSpeed below.
        RED4ext::CClassFunction* GetVehiclePSFn()
        {
            auto* fn = g_getVehiclePSFn.load(std::memory_order_acquire);
            if (fn) return fn;
            if (g_getVehiclePSLookupTried.exchange(true, std::memory_order_acq_rel))
                return nullptr;
            auto* rtti = RED4ext::CRTTISystem::Get();
            if (!rtti)
            {
                log::Warn("[gwheel:hook] RTTI unavailable — GetVehiclePS lookup skipped");
                return nullptr;
            }
            auto* cls = rtti->GetClass(RED4ext::CName("vehicleBaseObject"));
            if (!cls)
            {
                log::Warn("[gwheel:hook] RTTI class 'vehicleBaseObject' not found (GetVehiclePS)");
                return nullptr;
            }
            auto* resolved = cls->GetFunction(RED4ext::CName("GetVehiclePS"));
            if (!resolved)
            {
                log::Warn("[gwheel:hook] RTTI method 'vehicleBaseObject::GetVehiclePS' not found — wheel-surface detection disabled");
                return nullptr;
            }
            g_getVehiclePSFn.store(resolved, std::memory_order_release);
            log::InfoF("[gwheel:hook] RTTI resolved: vehicleBaseObject::GetVehiclePS -> %p", resolved);
            return resolved;
        }

        // One-shot diagnostic to identify the PS type we get back and
        // reveal where CNames actually live inside it on this build. The
        // hardcoded offset (0x78) is from SDK headers which are known to
        // drift across game patches. We log the first time a PS is read
        // so we can correlate the SDK layout against live memory.
        std::atomic<bool> g_psDiagnosticLogged{false};

        void LogPSDiagnostic(RED4ext::ISerializable* ps)
        {
            if (!ps) return;
            if (g_psDiagnosticLogged.exchange(true, std::memory_order_acq_rel)) return;

            // RTTI class name of the actual runtime type.
            const char* typeName = "?";
            if (auto* t = ps->GetType())
                typeName = t->GetName().ToString();
            log::InfoF("[gwheel:surface] PS handle type=%s at %p", typeName ? typeName : "?", ps);

            // Dump the first 0x200 bytes as 64-bit words. Rows that look
            // like small hash-distribution numbers (not pointer-range
            // 0x00007FF...) are CName-candidate offsets.
            const auto* bytes = reinterpret_cast<const uint64_t*>(ps);
            for (int off = 0; off < 0x200; off += 0x40)
            {
                log::InfoF("[gwheel:surface]   PS+0x%03X: %016llX %016llX %016llX %016llX %016llX %016llX %016llX %016llX",
                           off,
                           (unsigned long long)bytes[(off + 0x00) / 8],
                           (unsigned long long)bytes[(off + 0x08) / 8],
                           (unsigned long long)bytes[(off + 0x10) / 8],
                           (unsigned long long)bytes[(off + 0x18) / 8],
                           (unsigned long long)bytes[(off + 0x20) / 8],
                           (unsigned long long)bytes[(off + 0x28) / 8],
                           (unsigned long long)bytes[(off + 0x30) / 8],
                           (unsigned long long)bytes[(off + 0x38) / 8]);
            }
        }

        // Walk the given runtime class's property table looking for
        // `wheelRuntimeData`. Returns the valueOffset or 0 if absent.
        // The class hierarchy is walked upward (parent chain) so a
        // field declared on a base class resolves through a subclass.
        uint32_t ResolveWheelDataOffset(RED4ext::CClass* cls)
        {
            RED4ext::CName key("wheelRuntimeData");
            while (cls)
            {
                if (auto* p = cls->GetProperty(key))
                    return p->valueOffset;
                cls = cls->parent;
            }
            return 0;
        }

        // Read the 4 per-wheel material CName hashes for this vehicle.
        // Returns true on success; `out` indices 0-3 hold the hashes
        // (0 = no material / read failed for that wheel).
        bool ReadWheelMaterials(void* self, uint64_t out[4])
        {
            out[0] = out[1] = out[2] = out[3] = 0;
            // Permanently disabled once we've confirmed the current route
            // can't reach the data. Re-enable by clearing this latch if a
            // new resolution path is wired up.
            if (g_surfaceLookupFailed.load(std::memory_order_acquire)) return false;

            auto* fn = GetVehiclePSFn();
            if (!fn) return false;

            RED4ext::Handle<RED4ext::ISerializable> psHandle;
            RED4ext::ExecuteFunction(self, fn, &psHandle);
            if (!psHandle.instance) return false;

            LogPSDiagnostic(psHandle.instance);

            uint32_t off = g_psWheelDataOffset.load(std::memory_order_acquire);
            if (off == 0)
            {
                auto* type = psHandle.instance->GetType();
                if (!type) return false;
                const uint32_t resolved = ResolveWheelDataOffset(type);
                if (resolved > 0)
                {
                    g_psWheelDataOffset.store(resolved, std::memory_order_release);
                    off = resolved;
                    log::InfoF("[gwheel:surface] wheelRuntimeData resolved: offset=0x%X on %s",
                               resolved, type->GetName().ToString());
                }
                else
                {
                    g_surfaceLookupFailed.store(true, std::memory_order_release);
                    log::WarnF("[gwheel:surface] wheelRuntimeData not found on %s — surface reads disabled "
                               "(VehicleComponentPS and vehiclePersistentDataPS are sibling classes; needs a different route)",
                               type->GetName().ToString());
                    return false;
                }
            }

            auto* psBytes = reinterpret_cast<const char*>(psHandle.instance);
            for (int i = 0; i < 4; ++i)
            {
                out[i] = *reinterpret_cast<const uint64_t*>(psBytes + off + i * kPS_wheelEntryStride);
            }
            return true;
        }

        float ReadVehicleSpeed(void* self)
        {
            auto* fn = g_getCurrentSpeedFn.load(std::memory_order_acquire);
            if (!fn)
            {
                if (g_getCurrentSpeedLookupTried.exchange(true, std::memory_order_acq_rel))
                    return 0.f; // lookup already attempted and failed
                auto* rtti = RED4ext::CRTTISystem::Get();
                if (!rtti)
                {
                    log::Warn("[gwheel:hook] RTTI unavailable — GetCurrentSpeed lookup skipped");
                    return 0.f;
                }
                auto* cls = rtti->GetClass(RED4ext::CName("vehicleBaseObject"));
                if (!cls)
                {
                    log::Warn("[gwheel:hook] RTTI class 'vehicleBaseObject' not found");
                    return 0.f;
                }
                auto* resolved = cls->GetFunction(RED4ext::CName("GetCurrentSpeed"));
                if (!resolved)
                {
                    log::Warn("[gwheel:hook] RTTI method 'vehicleBaseObject::GetCurrentSpeed' not found — centering spring will stay disabled");
                    return 0.f;
                }
                g_getCurrentSpeedFn.store(resolved, std::memory_order_release);
                fn = resolved;
                log::InfoF("[gwheel:hook] RTTI resolved: vehicleBaseObject::GetCurrentSpeed -> %p", resolved);
            }

            float speed = 0.f;
            RED4ext::ExecuteFunction(self, fn, &speed);
            return speed;
        }

        inline float* FloatFieldAt(void* base, std::ptrdiff_t off)
        {
            return reinterpret_cast<float*>(static_cast<char*>(base) + off);
        }

        inline float Clamp(float v, float lo, float hi)
        {
            return v < lo ? lo : (v > hi ? hi : v);
        }

        void DetourUpdateVehicleCameraInput(void* self)
        {
            if (g_original) g_original(self);

            const auto n = g_fireCount.fetch_add(1, std::memory_order_relaxed) + 1;
            if (n == 1)
                log::InfoF("[gwheel:hook] UpdateVehicleCameraInput fired for the first time (self=%p)", self);
            if (!self) return;

            // Gate: only write into the player's currently-mounted vehicle.
            // Redscript mount/unmount event wrappers cache the pointer via
            // GWheel_SetPlayerVehicle. If no vehicle is cached, inject
            // nothing — the redscript hook is the authoritative source.
            void* pv = g_playerVehicle.load(std::memory_order_acquire);
            if (pv == nullptr || self != pv) return;

            const auto frame = sources::Current();
            if (!frame.connected) return;
            const auto cfg = config::Current();

            // Cast once to read labeled fields from the RED4ext SDK struct.
            // acceleration / deceleration are the processed drive commands
            // (what we've historically called throttle/brake in this mod).
            auto* veh = static_cast<RED4ext::vehicle::BaseObject*>(self);

            if (cfg.input.enabled)
            {
                // Compute the wheel's contribution to each axis.
                float wheelSteer = frame.axes.steer;
                if (cfg.wheel.steeringSensitivity != 1.0f)
                    wheelSteer = wheelSteer * cfg.wheel.steeringSensitivity;
                wheelSteer = Clamp(wheelSteer, -1.0f, 1.0f);
                const float wheelThrottle = Clamp(frame.axes.throttle, 0.0f, 1.0f);
                const float wheelBrake    = Clamp(frame.axes.brake,    0.0f, 1.0f);

                // Merge with whatever the vanilla input pipeline (keyboard /
                // gamepad) already wrote into the struct. g_original(self)
                // above has already processed WASD / analog stick input into
                // these fields; we take the max-magnitude so wheel and
                // keyboard coexist — whichever source asks for more steer /
                // throttle / brake wins, neither clobbers the other.
                float* pSteer    = FloatFieldAt(self, off::kInputSteer);
                float* pThrottle = &veh->acceleration;
                float* pBrake    = &veh->deceleration;

                if (std::fabs(wheelSteer) > std::fabs(*pSteer))
                    *pSteer = wheelSteer;
                if (wheelThrottle > *pThrottle)
                    *pThrottle = wheelThrottle;
                if (wheelBrake > *pBrake)
                    *pBrake = wheelBrake;

                const float steer    = *pSteer;
                const float throttle = *pThrottle;
                const float brake    = *pBrake;

                const auto m = g_injectCount.fetch_add(1, std::memory_order_relaxed) + 1;
                if (m == 1)
                    log::InfoF("[gwheel:hook] first injection: steer=%.3f throttle=%.3f brake=%.3f",
                               steer, throttle, brake);
                else if (m == 5000 || m == 50000 || m == 500000)
                    log::InfoF("[gwheel:hook] inject count = %llu (steer=%.3f throttle=%.3f brake=%.3f)",
                               static_cast<unsigned long long>(m), steer, throttle, brake);
            }

            // Physics-model FFB. Runs independently of cfg.input.enabled so
            // FFB still behaves correctly when the user disables input
            // injection for testing. cfg.ffb.enabled is the single master
            // toggle; there is no separate "centering only" gate.
            if (cfg.ffb.enabled)
            {
                const float speed = std::fabs(ReadVehicleSpeed(self));

                // Angular velocity magnitude from the vehicle's PhysicsData,
                // read via the RTTI-resolved offsets. Dominant component is
                // yaw (Z in REDengine), but we take the full vector
                // magnitude so we don't need an axis-convention assumption.
                float angVelMag = 0.f;
                float angVel[3] = {};
                if (vehicle_ext::ReadAngularVelocity(self, angVel))
                {
                    angVelMag = std::sqrt(angVel[0] * angVel[0]
                                        + angVel[1] * angVel[1]
                                        + angVel[2] * angVel[2]);
                }

                // Suspension activity proxy. Per-tick change in roll+pitch
                // angular velocity. Yaw is excluded — that comes from
                // steering input, not suspension. Units are rad per tick
                // (not per second); the downstream road-surface envelope
                // is tuned against this convention. First tick after a
                // vehicle swap will briefly read a big delta (prev=0);
                // the envelope attack is fast enough that the spurious
                // peak dies within a few frames.
                const float prevX = g_prevAngVelX.load(std::memory_order_acquire);
                const float prevY = g_prevAngVelY.load(std::memory_order_acquire);
                const float dX = std::fabs(angVel[0] - prevX);
                const float dY = std::fabs(angVel[1] - prevY);
                const float suspensionActivity = dX + dY;
                g_prevAngVelX.store(angVel[0], std::memory_order_release);
                g_prevAngVelY.store(angVel[1], std::memory_order_release);

                // Per-wheel road surface material read. Game authors this
                // as a CName on WheelRuntimePSData.previousTouchedMaterial;
                // controller rumble uses the same source. Log transitions
                // so we can see the raw CName set for asphalt / dirt /
                // gravel / etc. — first pass is diagnostic-only; we'll map
                // names to FFB categories in a follow-up once the log
                // shows what values actually fire.
                uint64_t mats[4] = {};
                if (ReadWheelMaterials(self, mats))
                {
                    for (int i = 0; i < 4; ++i)
                    {
                        const uint64_t prev = g_lastWheelMat[i].load(std::memory_order_acquire);
                        if (mats[i] != prev)
                        {
                            g_lastWheelMat[i].store(mats[i], std::memory_order_release);
                            RED4ext::CName cname; cname.hash = mats[i];
                            const char* name = RED4ext::CNamePool::Get(cname);
                            log::InfoF("[gwheel:surface] wheel[%d] material: %s (hash=0x%016llX)",
                                       i,
                                       name ? name : "(null)",
                                       static_cast<unsigned long long>(mats[i]));
                        }
                    }
                }

                // Per-car physics snapshot drives yaw reference, cruise
                // speed, and centering-baseline derivation. Hardcoded
                // fallbacks kick in for vehicles whose physics we can't
                // read (tanks, air, first-tick race before the read lands).
                constexpr float kFallbackYawRef      = 1.5f;  // rad/s — generic sports car
                constexpr float kFallbackCruiseMps   = 18.f;  // ~65 km/h
                constexpr float kFallbackCenteringB  = 0.85f; // mid-weight, matches new formula
                constexpr float kWheelbaseMinM       = 0.8f;
                constexpr float kWheelbaseMaxM       = 6.0f;

                const auto wp = vehicle_ext::ReadWheeledPhysics(self);
                float perCarYawRef = 0.f;
                if (wp.valid && wp.turnRate > 0.f)
                    perCarYawRef = std::clamp(wp.turnRate, 0.3f, 10.f);
                g_perCarYawRef.store(perCarYawRef, std::memory_order_relaxed);
                const float yawRef = (perCarYawRef > 0.f) ? perCarYawRef : kFallbackYawRef;

                // Cruise speed derived from wheelbase: longer chassis cruise
                // at higher speeds. cruise = 5 + 5×wheelbase, clamped. Maps
                // a 2.5m wheelbase (sports car) to ~17.5 m/s (63 km/h), a
                // 3.5m truck to 22.5 m/s, a 1.4m bike to 12 m/s.
                //
                // Centering baseline: stiffer car = more SAT per m/s. Use
                // wheelbase as a proxy for chassis stability — heavier
                // longer cars load the wheel harder at cruise. Formula
                // tuned so a 2.5m sports car hits 0.875 at cruise (heavy
                // but not locked), a 3.5m truck hits 1.0 (locked), a 1.4m
                // bike hits 0.71 (loose but present).
                float cruiseMps        = kFallbackCruiseMps;
                float centeringBaseline = kFallbackCenteringB;
                if (wp.valid
                    && wp.frontBackWheelDistance >= kWheelbaseMinM
                    && wp.frontBackWheelDistance <= kWheelbaseMaxM)
                {
                    const float wb = wp.frontBackWheelDistance;
                    cruiseMps        = std::clamp(5.f + 5.f * wb, 10.f, 40.f);
                    centeringBaseline = std::clamp(0.5f + 0.15f * wb, 0.5f, 1.0f);
                }

                // Run the offset probe once we have a live player vehicle.
                // Self-discovering: scans for back-pointer correlations to
                // find where physicsData / physics live in this build.
                if (!rtti_offsets::StructProbeDone())
                    rtti_offsets::ProbeStructOffsets(self);

                // Inner WheeledPhysics field-probe is defined but no longer
                // called per-mount — we've triangulated the 2.31 offsets
                // and hardcoded them in vehicle_physics_ext.h fallbacks.
                // Re-enable this block if a future patch breaks the reads.
                // (call site: rtti_offsets::ProbeInnerWheeledFields(wp))

                // First-tick per-vehicle dump: once per unique `self`, print
                // the physics values we pulled so users / devs can see what
                // the game reports for each ride and calibrate yaw scaling.
                void* lastLogged = g_lastPhysicsLoggedVehicle.load(std::memory_order_acquire);
                if (lastLogged != self)
                {
                    if (g_lastPhysicsLoggedVehicle.compare_exchange_strong(
                            lastLogged, self, std::memory_order_acq_rel))
                    {
                        log::InfoF("[gwheel:ffb] per-car physics for vehicle=%p:", self);
                        if (wp.valid)
                        {
                            log::InfoF("[gwheel:ffb]   turnRate=%.3f maxWheelTurnDeg=%.2f slipAngleCurveScale=%.3f "
                                       "numDriveWheels=%u wheelbase=%.3f",
                                       wp.turnRate, wp.maxWheelTurnDeg, wp.slipAngleCurveScale,
                                       wp.numDriveWheels, wp.frontBackWheelDistance);
                        }
                        else
                        {
                            log::Info("[gwheel:ffb]   WheeledPhysics unavailable (tank / air / mount race?)");
                        }
                        float vel[3] = {};
                        if (vehicle_ext::ReadVelocity(self, vel))
                        {
                            log::InfoF("[gwheel:ffb]   velocity=(%.2f, %.2f, %.2f) angVel=(%.2f, %.2f, %.2f)",
                                       vel[0], vel[1], vel[2],
                                       angVel[0], angVel[1], angVel[2]);
                        }
                        log::InfoF("[gwheel:ffb]   yawRef in use: %.3f rad/s (%s)",
                                   yawRef, perCarYawRef > 0.f ? "from turnRate" : "hardcoded fallback");
                        log::InfoF("[gwheel:ffb]   derived cruise=%.2f m/s, centeringBaseline=%.3f (%s)",
                                   cruiseMps, centeringBaseline,
                                   (wp.valid
                                    && wp.frontBackWheelDistance >= kWheelbaseMinM
                                    && wp.frontBackWheelDistance <= kWheelbaseMaxM)
                                       ? "from wheelbase" : "hardcoded fallback");
                    }
                }

                // Raw wheel position for active-torque direction. We use
                // the DirectInput-normalised wheelSteer (physical wheel
                // position in its operating range), not the post-merge
                // game input field — which can be tiny if G HUB has a
                // wide operating range (e.g. 30° rotation out of 900°
                // reports ~0.07, which × activeTorque rides below the
                // wheel motor's friction threshold and produces no felt
                // push-back). wheelSteer is always normalised -1..+1
                // against the *current* operating range, so a moderate
                // hand position always reads as a moderate value.
                float torqueSteer = frame.axes.steer;
                if (cfg.wheel.steeringSensitivity != 1.0f)
                    torqueSteer *= cfg.wheel.steeringSensitivity;
                torqueSteer = Clamp(torqueSteer, -1.0f, 1.0f);

                wheel::UpdateCenteringSpring(
                    speed,
                    angVelMag,
                    suspensionActivity,
                    torqueSteer,
                    veh->acceleration,  // post-merge throttle (0..1)
                    veh->deceleration,  // post-merge brake (0..1)
                    veh->isReversing,
                    veh->isOnGround,
                    /*enabled*/       true,
                    cfg.ffb.stationaryThresholdMps,
                    cruiseMps,
                    centeringBaseline,
                    cfg.ffb.yawFeedbackPct,
                    yawRef,
                    cfg.ffb.activeTorqueStrengthPct,
                    cfg.ffb.debugLogging);
            }
            else
            {
                // FFB master toggle off — release every effect. Calling
                // Update with enabled=false performs the edge teardown
                // (spring, active, damper, road surface, airborne).
                wheel::UpdateCenteringSpring(0.f, 0.f, 0.f, 0.f, 0.f, 0.f, false, true, /*enabled*/ false,
                                             0.f, 1.f, 1.f, 0, 1.f, 0, cfg.ffb.debugLogging);
            }
        }
    }

    bool Init()
    {
        auto& ctx = Ctx();
        if (!ctx.sdk || !ctx.sdk->hooking || !ctx.sdk->hooking->Attach)
        {
            log::Error("[gwheel:hook] RED4ext hooking API unavailable");
            return false;
        }

        // Resolving the hash terminates the game with a RED4ext MessageBox
        // if the address db doesn't know it for the current build. That's
        // the ecosystem convention - users get a clear "which mod is
        // broken" dialog rather than a silent crash later.
        const auto addr = RED4ext::UniversalRelocBase::Resolve(kUpdateVehicleCameraInputHash);
        g_target = reinterpret_cast<void*>(addr);
        log::InfoF("[gwheel:hook] UpdateVehicleCameraInput resolved: hash=%u addr=%p",
                   kUpdateVehicleCameraInputHash, g_target);

        const bool ok = ctx.sdk->hooking->Attach(
            ctx.handle,
            g_target,
            reinterpret_cast<void*>(&DetourUpdateVehicleCameraInput),
            reinterpret_cast<void**>(&g_original));

        if (!ok)
        {
            log::Error("[gwheel:hook] Attach returned false for UpdateVehicleCameraInput");
            g_target = nullptr;
            return false;
        }

        g_attached.store(true, std::memory_order_release);
        log::Info("[gwheel:hook] UpdateVehicleCameraInput detour installed "
                  "(player vehicle input override: steer/throttle/brake)");
        return true;
    }

    void Shutdown()
    {
        auto& ctx = Ctx();
        if (!g_attached.exchange(false)) return;
        if (ctx.sdk && ctx.sdk->hooking && ctx.sdk->hooking->Detach && g_target)
        {
            ctx.sdk->hooking->Detach(ctx.handle, g_target);
            log::Info("[gwheel:hook] UpdateVehicleCameraInput detour detached");
        }
        g_target = nullptr;
        g_original = nullptr;
    }

    bool IsInstalled() { return g_attached.load(std::memory_order_acquire); }

    uint64_t FireCount() { return g_fireCount.load(std::memory_order_relaxed); }

    void SetPlayerVehicle(void* p)
    {
        void* prev = g_playerVehicle.exchange(p, std::memory_order_acq_rel);
        if (prev != p)
        {
            log::InfoF("[gwheel:hook] player vehicle changed: %p -> %p", prev, p);
            // Reset the per-vehicle physics-logging latch so the next
            // vehicle's turnRate / wheelbase / etc. get dumped on first
            // tick. Also clear the per-car yaw reference and angular-
            // velocity history so stale values from the previous ride
            // don't bleed across the transition.
            g_lastPhysicsLoggedVehicle.store(nullptr, std::memory_order_release);
            g_perCarYawRef.store(0.f, std::memory_order_release);
            g_prevAngVelX.store(0.f, std::memory_order_release);
            g_prevAngVelY.store(0.f, std::memory_order_release);
            for (int i = 0; i < 4; ++i)
                g_lastWheelMat[i].store(0, std::memory_order_release);
        }
        // In-vehicle context flag tracks presence/absence of a mounted
        // vehicle pointer. input_bindings uses it to suppress vehicle-
        // centric dispatches on-foot.
        sources::SetInVehicle(p != nullptr);
    }

    bool IsPlayerVehicle(void* p)
    {
        return p != nullptr && g_playerVehicle.load(std::memory_order_acquire) == p;
    }
}
