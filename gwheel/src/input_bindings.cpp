#include "input_bindings.h"
#include "config.h"
#include "logging.h"
#include "sources.h"
#include "kbd_hook.h"

#include <windows.h>

#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <mutex>

namespace gwheel::input_bindings
{
    namespace
    {
        // How a PhysicalInput is derived from a wheel::Snapshot. Unmapped
        // = this physical control doesn't exist on the current wheel model;
        // the slot is held in the UI for uniformity but edges never fire.
        enum class Source { Unmapped, Button, PovDirection };
        struct InputSourceMap
        {
            Source   source = Source::Unmapped;
            uint32_t value  = 0; // button index (Button) or POV raw value (PovDirection)
        };

        constexpr InputSourceMap UM()          { return { Source::Unmapped, 0 }; }
        constexpr InputSourceMap BT(uint32_t v){ return { Source::Button, v }; }
        constexpr InputSourceMap PV(uint32_t v){ return { Source::PovDirection, v }; }

        using DeviceLayout = std::array<InputSourceMap, kCount>;

        // ------------------------------------------------------------------
        // G923 Xbox — VERIFIED empirically 2026-04-21 via tools/input_probe
        // on Grant's hardware. Ground truth.
        // ------------------------------------------------------------------
        constexpr DeviceLayout kG923XboxLayout = {{
            BT(5),         // PaddleLeft
            BT(4),         // PaddleRight
            PV(0),         // DpadUp
            PV(18000),     // DpadDown
            PV(27000),     // DpadLeft
            PV(9000),      // DpadRight
            BT(0),         // A
            BT(1),         // B
            BT(2),         // X
            BT(3),         // Y
            BT(6),         // Start
            BT(7),         // Select
            BT(9),         // LSB
            BT(8),         // RSB
            BT(18),        // Plus
            BT(19),        // Minus
            BT(22),        // ScrollClick
            BT(20),        // ScrollCW
            BT(21),        // ScrollCCW
            BT(10),        // Xbox
        }};

        // ------------------------------------------------------------------
        // G923 PS / G920 — UNVERIFIED. Physically identical button count
        // to G923 Xbox; most likely same DInput indices. If bindings feel
        // wrong on these wheels, re-run tools/input_probe.exe and submit
        // corrected indices. "Xbox"/"PS" button labels differ physically
        // but map to the same DInput slots.
        // ------------------------------------------------------------------
        constexpr DeviceLayout kG923PSLayout  = kG923XboxLayout;
        constexpr DeviceLayout kG920Layout    = kG923XboxLayout;

        // ------------------------------------------------------------------
        // G29 — UNVERIFIED. Same physical control count as G923 (the G923
        // is effectively a G29 with Trueforce). DInput layout is almost
        // certainly identical. Labels differ (Cross/Circle/Square/Triangle
        // for the PS variant) but slots are the same.
        // ------------------------------------------------------------------
        constexpr DeviceLayout kG29Layout = kG923XboxLayout;

        // ------------------------------------------------------------------
        // G27 — UNVERIFIED. Missing many controls the G923 has: no scroll
        // wheel, no +/- buttons, no Xbox button. The G27 has a 6-speed
        // shifter + reverse as discrete buttons, but we don't expose those
        // as PhysicalInputs yet (would require expanding PhysicalInput and
        // the UI). Face buttons are labeled 1-4 (no ABXY letters).
        // Paddle indices are known from community DIEM configs.
        // ------------------------------------------------------------------
        constexpr DeviceLayout kG27Layout = {{
            BT(5),         // PaddleLeft
            BT(4),         // PaddleRight
            PV(0),         // DpadUp
            PV(18000),     // DpadDown
            PV(27000),     // DpadLeft
            PV(9000),      // DpadRight
            BT(0),         // "A" (G27 button 1)
            BT(1),         // "B" (G27 button 2)
            BT(2),         // "X" (G27 button 3)
            BT(3),         // "Y" (G27 button 4)
            BT(6),         // Start
            BT(7),         // Select
            UM(),          // LSB (not present on G27)
            UM(),          // RSB (not present on G27)
            UM(),          // Plus
            UM(),          // Minus
            UM(),          // ScrollClick
            UM(),          // ScrollCW
            UM(),          // ScrollCCW
            UM(),          // Xbox
        }};

        // ------------------------------------------------------------------
        // G25 — UNVERIFIED. Very similar to G27 but the D-pad might be
        // buttons instead of a POV hat. Minimal face buttons. Probably
        // works basically like G27 for our purposes; users of this wheel
        // are rare enough that we'll correct if anyone reports issues.
        // ------------------------------------------------------------------
        constexpr DeviceLayout kG25Layout = kG27Layout;

        // ------------------------------------------------------------------
        // Driving Force GT / MOMO / Wingman — UNVERIFIED. Older wheels
        // with minimal controls. Fall back to G27 layout (paddles + basic
        // face buttons). Most slots will be Unmapped.
        // ------------------------------------------------------------------
        constexpr DeviceLayout kDrivingForceLayout = kG27Layout;

        // ------------------------------------------------------------------
        // Device registry. Order matters: first friendly-name substring
        // match wins, so list more-specific names before less-specific.
        // ------------------------------------------------------------------
        struct DeviceEntry
        {
            const char*         nameSubstring; // case-insensitive substring for LogiGetFriendlyProductName
            const char*         label;         // for logs
            const DeviceLayout* layout;
            bool                verified;
        };

        constexpr DeviceEntry kDeviceRegistry[] = {
            { "G923 Racing Wheel for Xbox",        "G923 Xbox",       &kG923XboxLayout,      true  },
            { "G923 Racing Wheel for PlayStation", "G923 PS",         &kG923PSLayout,        false },
            { "G923",                               "G923 (unknown variant)", &kG923XboxLayout, false },
            { "G920",                               "G920",            &kG920Layout,          false },
            { "G29",                                "G29",             &kG29Layout,           false },
            { "G27",                                "G27",             &kG27Layout,           false },
            { "G25",                                "G25",             &kG25Layout,           false },
            { "Driving Force GT",                   "Driving Force GT",&kDrivingForceLayout,  false },
            { "Driving Force",                      "Driving Force",   &kDrivingForceLayout,  false },
            { "MOMO",                               "MOMO",            &kDrivingForceLayout,  false },
            { "Wingman",                            "Wingman",         &kDrivingForceLayout,  false },
        };

        bool ContainsCaseInsensitive(const char* haystack, const char* needle)
        {
            if (!haystack || !needle) return false;
            for (const char* p = haystack; *p; ++p)
            {
                size_t i = 0;
                while (needle[i] && p[i] &&
                       std::tolower(static_cast<unsigned char>(p[i])) ==
                       std::tolower(static_cast<unsigned char>(needle[i])))
                    ++i;
                if (!needle[i]) return true;
            }
            return false;
        }

        // How each Action translates to a Windows input event. Ordered to
        // match the Action enum — array index = Action value.
        enum class DispatchKind : uint8_t { None, Keyboard, MouseButton, MouseWheel };

        // Tap = fire DOWN+UP together on rising edge (a quick pulse regardless
        //       of how long the user physically holds). Use for toggles and
        //       one-shot actions — e.g. "press M to open map", "tap MMB for
        //       rear-view camera". Prevents held physical buttons from
        //       triggering the game's long-press behavior (weapon wheel on
        //       held MMB, etc.).
        // Hold = fire DOWN on rise, UP on fall. Use for sustained actions —
        //        handbrake, horn, gunfire, weapon-wheel hold. The virtual
        //        key tracks the wheel button's physical state.
        enum class DispatchMode : uint8_t { Tap, Hold };

        // AnyContext  = dispatch regardless of whether V is on foot or
        //               driving. Reserved for menu-opening actions (Pause,
        //               OpenMap/Journal/Inventory/Phone/Perks/Crafting,
        //               QuickSave), menu-nav fallback actions (Menu*), and
        //               CallVehicle (summon-from-foot is the point).
        // VehicleOnly = suppress on-foot. The bound keyboard keys mean
        //               different things when V is walking (Space=jump,
        //               V=summon, F=interact, G=grenade, etc.), so firing
        //               them off a wheel button produces unintended side-
        //               effects. Default for every action that isn't
        //               explicitly about opening a menu. The in-vehicle
        //               flag is set from the mount event wrappers; see
        //               sources::SetInVehicle.
        enum class ActionScope : uint8_t { AnyContext, VehicleOnly };

        struct DispatchEntry
        {
            DispatchKind kind          = DispatchKind::None;
            DispatchMode mode          = DispatchMode::Tap;
            ActionScope  scope         = ActionScope::VehicleOnly;
            WORD         vk            = 0;    // Keyboard: virtual-key code
            DWORD        mouseDownFlag = 0;    // MouseButton
            DWORD        mouseUpFlag   = 0;
            DWORD        wheelDelta    = 0;    // MouseWheel (signed in low word)
            const char*  label         = "";   // for logging
        };

        // Default scope for all constructor helpers is VehicleOnly — on-foot
        // suppression is the rule, AnyContext is the exception. Menu-opening
        // actions (the A-variants) explicitly opt in.
        //
        // Tap-mode constructors (toggles, cycles).
        constexpr DispatchEntry K(WORD vk, const char* lbl)
        {
            return { DispatchKind::Keyboard, DispatchMode::Tap, ActionScope::VehicleOnly, vk, 0, 0, 0, lbl };
        }
        constexpr DispatchEntry M(DWORD down, DWORD up, const char* lbl)
        {
            return { DispatchKind::MouseButton, DispatchMode::Tap, ActionScope::VehicleOnly, 0, down, up, 0, lbl };
        }
        constexpr DispatchEntry W(int delta, const char* lbl)
        {
            return { DispatchKind::MouseWheel, DispatchMode::Tap, ActionScope::VehicleOnly, 0, 0, 0, static_cast<DWORD>(delta), lbl };
        }
        // Hold-mode constructors — sustain while the wheel button is held.
        constexpr DispatchEntry KH(WORD vk, const char* lbl)
        {
            return { DispatchKind::Keyboard, DispatchMode::Hold, ActionScope::VehicleOnly, vk, 0, 0, 0, lbl };
        }
        constexpr DispatchEntry MH(DWORD down, DWORD up, const char* lbl)
        {
            return { DispatchKind::MouseButton, DispatchMode::Hold, ActionScope::VehicleOnly, 0, down, up, 0, lbl };
        }
        // AnyContext variants (fire on foot and in-vehicle). Menu/pause/
        // quicksave/CallVehicle only.
        constexpr DispatchEntry KA(WORD vk, const char* lbl)
        {
            return { DispatchKind::Keyboard, DispatchMode::Tap, ActionScope::AnyContext, vk, 0, 0, 0, lbl };
        }

        constexpr std::array<DispatchEntry, kActionCount> kDispatch = {{
            /* None                */ { DispatchKind::None, DispatchMode::Tap, ActionScope::AnyContext, 0, 0, 0, 0, "None" },
            /* Horn                */ KH('Z',        "Horn (Z, hold)"),
            /* Headlights          */ K('V',         "Headlights (V)"),
            /* Handbrake           */ KH(VK_SPACE,   "Handbrake (Space, hold)"),
            /* Autodrive           */ K('G',         "Autodrive (G)"),
            /* ExitVehicle         */ K('F',         "ExitVehicle (F)"),
            /* CameraCycleForward  */ K('Q',         "CameraCycleFwd (Q)"),
            /* CameraCycleBackward */ MH(MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP, "Rear-view camera (MMB, hold)"),
            /* CameraReset         */ K('C',         "CameraReset (C)"),
            /* ZoomIn              */ W(+WHEEL_DELTA,"ZoomIn (MWheelUp)"),
            /* ZoomOut             */ W(-WHEEL_DELTA,"ZoomOut (MWheelDown)"),
            /* ShootPrimary        */ MH(MOUSEEVENTF_LEFTDOWN,  MOUSEEVENTF_LEFTUP,  "ShootPrimary (LMB, hold)"),
            /* ShootSecondary      */ MH(MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP, "ShootSecondary (RMB, hold)"),
            /* ShootTertiary       */ KH(VK_LCONTROL,"ShootTertiary (LCtrl, hold)"),
            /* NextWeapon          */ W(+WHEEL_DELTA,"NextWeapon (MWheelUp)"),
            /* PrevWeapon          */ W(-WHEEL_DELTA,"PrevWeapon (MWheelDown)"),
            /* WeaponSlot1         */ K('1',         "WeaponSlot1 (1)"),
            /* WeaponSlot2         */ K('2',         "WeaponSlot2 (2)"),
            /* SwitchWeapons       */ KH(VK_MENU,    "SwitchWeapons (Alt, hold for wheel)"),
            /* HolsterWeapon       */ K('B',         "HolsterWeapon (B)"),
            /* OpenMap             */ KA('M',        "OpenMap (M)"),
            /* OpenJournal         */ KA('J',        "OpenJournal (J)"),
            /* OpenInventory       */ KA('I',        "OpenInventory (I)"),
            /* OpenPhone           */ KA('T',        "OpenPhone (T)"),
            /* OpenPerks           */ KA('P',        "OpenPerks (P)"),
            /* OpenCrafting        */ KA('K',        "OpenCrafting (K)"),
            /* QuickSave           */ KA(VK_F5,      "QuickSave (F5)"),
            /* RadioMenu           */ K('R',         "RadioMenu (R)"),
            /* UseConsumable       */ K('X',         "UseConsumable (X)"),
            /* IconicCyberware     */ K('E',         "IconicCyberware (E)"),
            /* Pause               */ KA(VK_ESCAPE,  "Pause (Esc)"),
            /* Tag                 */ M(MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP, "Tag (MMB, tap)"),
            /* CallVehicle         */ KA('V',        "CallVehicle (V)"),
            /* MenuConfirm         */ KA(VK_RETURN,  "MenuConfirm (Enter)"),
            /* MenuCancel          */ KA(VK_ESCAPE,  "MenuCancel (Esc)"),
            /* MenuUp              */ KA(VK_UP,      "MenuUp (Up arrow)"),
            /* MenuDown            */ KA(VK_DOWN,    "MenuDown (Down arrow)"),
            /* MenuLeft            */ KA(VK_LEFT,    "MenuLeft (Left arrow)"),
            /* MenuRight           */ KA(VK_RIGHT,   "MenuRight (Right arrow)"),
        }};

        struct State
        {
            std::mutex              mtx;
            BindingArray            bindings{}; // all-zero = all None
            const DeviceLayout*     layout = &kG923XboxLayout; // default until SetDeviceLayout

            // Edge-detection state, owned by the pump thread.
            uint32_t                prevButtons = 0;
            DWORD                   prevPov     = 0xFFFFFFFF;
            bool                    havePrev    = false;
        };

        State& S() { static State s; return s; }

        void FireInput(const INPUT& in, const DispatchEntry& e, bool down)
        {
            INPUT local = in;
            // Tag every event with kExtraInfoTag so the LL keyboard hook
            // can distinguish our own injections from G HUB's and pass
            // ours through unmodified.
            if (local.type == INPUT_KEYBOARD)
                local.ki.dwExtraInfo = kbd_hook::kExtraInfoTag;
            else if (local.type == INPUT_MOUSE)
                local.mi.dwExtraInfo = kbd_hook::kExtraInfoTag;
            const UINT n = SendInput(1, &local, sizeof(local));
            if (n != 1)
            {
                log::WarnF("[gwheel:bind] SendInput(%s, %s) returned %u (expected 1)",
                           e.label, down ? "DOWN" : "UP", n);
            }
        }

        // Fire a single DOWN or UP event for an action, given the entry.
        void FireOne(const DispatchEntry& e, bool down)
        {
            INPUT in{};
            switch (e.kind)
            {
            case DispatchKind::Keyboard:
                in.type = INPUT_KEYBOARD;
                in.ki.wVk = e.vk;
                in.ki.wScan = static_cast<WORD>(MapVirtualKeyW(e.vk, MAPVK_VK_TO_VSC));
                in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
                FireInput(in, e, down);
                break;
            case DispatchKind::MouseButton:
                in.type = INPUT_MOUSE;
                in.mi.dwFlags = down ? e.mouseDownFlag : e.mouseUpFlag;
                FireInput(in, e, down);
                break;
            case DispatchKind::MouseWheel:
                // Wheel is a pulse event regardless of mode; fire only on DOWN.
                if (down)
                {
                    in.type = INPUT_MOUSE;
                    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
                    in.mi.mouseData = e.wheelDelta;
                    FireInput(in, e, true);
                }
                break;
            case DispatchKind::None:
            default:
                break;
            }
        }

        void Dispatch(int32_t action, bool rising)
        {
            if (action <= 0 || action >= kActionCount) return;
            const auto& e = kDispatch[static_cast<size_t>(action)];
            if (e.kind == DispatchKind::None) return;

            // On-foot suppression for vehicle-centric actions. CP2077 assigns
            // the same keyboard keys different meanings on foot (V=summon,
            // F=interact, Space=jump, etc.), so dispatching a wheel-bound
            // Headlights / Handbrake / Horn while V is walking produces
            // unintended side-effects. Log only on the rising edge so the
            // suppression is visible but not spammy.
            if (e.scope == ActionScope::VehicleOnly && !sources::InVehicle())
            {
                if (rising)
                    log::DebugF("[gwheel:bind] on-foot: %s suppressed", e.label);
                return;
            }

            if (e.mode == DispatchMode::Tap)
            {
                // Tap: fire DOWN+UP together on rising edge only. Ignore
                // falling edge entirely — the tap is already complete.
                if (!rising) return;
                FireOne(e, true);
                FireOne(e, false);
                log::InfoF("[gwheel:bind] dispatch action=%d %s TAP", action, e.label);
            }
            else
            {
                // Hold: mirror the physical state.
                FireOne(e, rising);
                log::InfoF("[gwheel:bind] dispatch action=%d %s %s",
                           action, e.label, rising ? "DOWN" : "UP");
            }
        }

        bool IsPhysicallyPressed(const DeviceLayout& layout, PhysicalInput p,
                                 uint32_t buttons, DWORD pov)
        {
            const auto& m = layout[p];
            switch (m.source)
            {
            case Source::Button:
                return (buttons & (1u << m.value)) != 0;
            case Source::PovDirection:
                return pov == m.value;
            case Source::Unmapped:
            default:
                return false;
            }
        }
    }

    void SetDeviceLayout(const char* friendlyProductName)
    {
        auto& st = S();
        const DeviceEntry* match = nullptr;
        for (const auto& entry : kDeviceRegistry)
        {
            if (ContainsCaseInsensitive(friendlyProductName, entry.nameSubstring))
            {
                match = &entry;
                break;
            }
        }

        std::lock_guard lk(st.mtx);
        if (match)
        {
            st.layout = match->layout;
            if (match->verified)
            {
                log::InfoF("[gwheel:bind] device layout = %s (verified)", match->label);
            }
            else
            {
                log::WarnF("[gwheel:bind] device layout = %s (UNVERIFIED - bindings may be "
                           "wrong; run tools/input_probe.exe to confirm indices)", match->label);
            }
        }
        else
        {
            st.layout = &kG923XboxLayout;
            log::WarnF("[gwheel:bind] unknown wheel \"%s\"; falling back to G923 Xbox "
                       "layout. Run tools/input_probe.exe to confirm indices.",
                       friendlyProductName ? friendlyProductName : "(null)");
        }
    }

    void ReplaceAll(const BindingArray& bindings)
    {
        auto& st = S();
        std::lock_guard lk(st.mtx);
        st.bindings = bindings;
    }

    void Set(int32_t inputId, int32_t action)
    {
        if (inputId < 0 || inputId >= kCount) return;
        if (action  < 0 || action  >= kActionCount) action = 0;
        auto& st = S();
        std::lock_guard lk(st.mtx);
        st.bindings[static_cast<size_t>(inputId)] = action;
    }

    int32_t Get(int32_t inputId)
    {
        if (inputId < 0 || inputId >= kCount) return 0;
        auto& st = S();
        std::lock_guard lk(st.mtx);
        return st.bindings[static_cast<size_t>(inputId)];
    }


    void OnTick(const sources::Frame& frame)
    {
        auto& st = S();
        if (!config::Current().input.enabled) return;

        const uint32_t buttons = frame.digital.buttons;
        // digital.pov is the DInput POV raw in the low 16 bits. 0xFFFF = center.
        // We compare directly against direction values (0, 9000, 18000, 27000)
        // which never collide with 0xFFFF.
        const DWORD    pov     = static_cast<DWORD>(frame.digital.pov);

        if (!st.havePrev)
        {
            st.prevButtons = buttons;
            st.prevPov     = pov;
            st.havePrev    = true;
            return;
        }

        // D-pad and A are hard-overridden to menu-nav actions at all
        // times — they send arrow keys / Enter, which are silent in
        // CP2077 gameplay and navigate menus elsewhere. This gives
        // every Logitech wheel working menu nav without any context
        // detection (which turned out to be expensive, unreliable, and
        // not exposed by the game's public APIs).
        //
        // B / X / Y fall through to the user's Mod Settings binding.
        // That lets the user explicitly bind B=Pause for gamepad-style
        // back-button behavior, or leave it None if they don't want the
        // Esc-opens-pause-menu-in-gameplay side-effect.
        auto fixedAction = [](int32_t i) -> int32_t {
            switch (i)
            {
            case DpadUp:    return MenuUp;
            case DpadDown:  return MenuDown;
            case DpadLeft:  return MenuLeft;
            case DpadRight: return MenuRight;
            case ButtonA:   return MenuConfirm;
            default:        return -1;
            }
        };

        // Snapshot the bindings and layout once per tick to avoid holding
        // the lock across SendInput calls.
        BindingArray bindings;
        const DeviceLayout* layout;
        {
            std::lock_guard lk(st.mtx);
            bindings = st.bindings;
            layout   = st.layout;
        }

        for (int32_t i = 0; i < kCount; ++i)
        {
            const PhysicalInput p = static_cast<PhysicalInput>(i);
            const bool pressed    = IsPhysicallyPressed(*layout, p, buttons, pov);
            const bool wasPressed = IsPhysicallyPressed(*layout, p, st.prevButtons, st.prevPov);
            if (pressed != wasPressed)
            {
                const int32_t fx = fixedAction(i);
                const int32_t action = (fx >= 0) ? fx : bindings[static_cast<size_t>(i)];
                Dispatch(action, pressed);
            }
        }

        st.prevButtons = buttons;
        st.prevPov     = pov;
    }
}
