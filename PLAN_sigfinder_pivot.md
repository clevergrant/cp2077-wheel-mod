# Plan — pivot to Python sigfinder + hard-coded signatures

## TL;DR for the new agent

You are taking over a stalled C++ RED4ext plugin that drives Cyberpunk 2077's vehicle input from a Logitech G-series wheel. Several routing architectures were tried and rejected. The final, user-approved architecture is:

1. **C++ plugin** reads the wheel via DirectInput, plays FFB via DirectInput, and detours one (or more) functions inside `Cyberpunk2077.exe` to inject wheel values. The function addresses are found via byte-pattern AOB scanning of the loaded module's `.text` section. The patterns are **hard-coded** in `gwheel/src/sigs.h`.
2. **Python tool** (`tools/sigfinder/`) is run by the maintainer (us) against a live `Cyberpunk2077.exe` process whenever CDPR ships a patch. It uses Frida to differentially scan memory for the input fields, capture the writing instruction via `MemoryAccessMonitor`, walk back to the function prologue, and emit a paste-ready `sigs.h` snippet to stdout. The tool **never ships to end users** — it's a dev/release-time tool only.
3. **End-user install is just `gwheel.dll` + `.reds`.** No drivers, no virtual pads, no Python, no extra MSIs.

## Hard requirements (from the user)

- **Zero external installs** for end users. No ViGEmBus, no x360ce, no Python.
- **No CET dependency.**
- **DirectInput layer stays.** `gwheel/src/wheel_dinput.{h,cpp}` already works (deferred-acquire path lands cleanly when the game window appears) — do not touch it except where instructed.
- **Honest versioning** — still v0.1.0. Do not bump.
- **One giant commit** at the end of the work session, not granular commits along the way. The user does the commit; you do not.
- **Verbose logging** — every meaningful step gets a `log::Info`/`log::Debug` line.
- **No invented byte patterns** — `sigs.h` ships with the pattern slot empty and clearly marked TBD until the sigfinder tool produces a real one. The plugin must detect the empty pattern and log "hook inactive — run tools/sigfinder and update sigs.h" without crashing.

## What was tried and rejected

| # | Approach | Outcome | Why rejected |
|---|---|---|---|
| 1 | Logitech Steering Wheel SDK (`LogitechSteeringWheelEnginesWrapper.dll`) for both wheel I/O and game integration | `LogiSteeringInitialize` returns `false` reliably; G HUB virtual driver hides the wheel from CP2077's XInput query | Doesn't reach the game even when the SDK works |
| 2 | RED4ext RTTI hooks on `VehicleComponent::GetInputValueFloat` etc. | RTTI dump (in `gwheel-2026-04-20-23-42-43.log`) proved those methods don't exist in the current game patch — only state effects (`ForceBrakesFor`, `ToggleHorn`) are RTTI-exposed | Target functions aren't reachable through RTTI |
| 3 | ViGEmBus virtual Xbox 360 controller | Built and deployed; pad plugged in successfully; DirectInput acquire kept failing with `ERROR_INVALID_WINDOW_HANDLE` because plugin loads before game window exists. Eventually fixed by deferring acquire — but user rejected the whole architecture because end users would have to install the ViGEmBus MSI driver | Driver install is unacceptable |
| 4 | In-process `XInputGetState` API hook (no driver needed) | Proposed; user rejected | Not what they want |
| 5 | Memory writeback (auto-discover field addresses at runtime, write wheel values directly into them every tick) | Proposed; user rejected | Not what they want |
| 6 | Static AOB sigscan with manually-discovered patterns from Ghidra/IDA | Proposed; user rejected the manual RE workflow but accepted the sigscan **runtime** part — coupled with an automated discovery tool | Adopted in modified form; see below |

The chosen architecture is essentially #6 + an automated discovery tool that removes the manual Ghidra step.

## Architecture (the chosen path)

### Runtime (in `gwheel.dll`)

```
plugin.cpp  step 3/4  ->  wheel::Init()           (DirectInput; deferred Acquire — already works)
                          vehicle_hook::Init()    (sigscan sigs.h patterns; install detour if matched)
                          start pump thread (250 Hz wheel poll + FFB upkeep)
```

`vehicle_hook::Init()`:
1. Resolve `Cyberpunk2077.exe` module base via `GetModuleHandleW(nullptr)` (we're loaded into it).
2. Log base address and `.text` section bounds.
3. For each pattern in `sigs.h`:
   - If empty: log `[gwheel:hook] <slot> pattern not configured — hook inactive. Run tools/sigfinder.py and update sigs.h.`
   - Else: `sigscan::ScanAll(...)`. Log every hit address. If exactly one match → install detour via `RED4ext::v1::Hooking::Attach(target, &Detour, &g_originalFn)`. If zero hits → log "game version drift — sigs.h was tested against game version `kGameVersionTested` but no match in the current binary; please re-run tools/sigfinder.py and re-release." If multiple hits → log all addresses + refuse to install.

The detour itself: call original first, then overwrite the relevant input float(s) at known offsets within the context object using values from `wheel::CurrentSnapshot()`. Field offsets are also part of `sigs.h` — they're an output of the sigfinder tool too.

### Discovery tool (in `tools/sigfinder/`)

Python 3.10+, single entry point: `python tools/sigfinder/sigfinder.py`.

Dependencies (in `tools/sigfinder/requirements.txt`):
- `frida` — dynamic instrumentation
- `capstone` — x64 disassembly for prologue walk-back and pattern stabilization
- `pefile` — parse `Cyberpunk2077.exe` on disk for `.text` section bounds and verifying RVAs

Workflow when run:
1. Connect to running `Cyberpunk2077.exe` via Frida (`frida.attach("Cyberpunk2077.exe")`).
2. Print module base, `.text` bounds.
3. Prompt: "Load into a vehicle, sit idle, then press Enter." User obliges.
4. Snapshot all writable+readable pages within the game's memory.
5. For each input axis (throttle, brake, steer in turn):
   - Prompt: "Hold throttle for 2 seconds, release, then press Enter."
   - Snapshot again. Diff for floats whose value swung in the expected direction (`throttle: ~0 → ~1 → ~0`; `brake: same`; `steer: ~0 → ~+1 → ~-1 → ~0`).
   - Narrow to candidate addresses (typically 1–10 each).
6. For each surviving candidate address, set a Frida `MemoryAccessMonitor` to capture the next write. Prompt user to nudge the relevant input once. Capture writer instruction RIP.
7. Walk back from RIP in the game's `.text` to find the function prologue (`48 89 5C 24 ?? 57`, `40 53 48 83 EC ??`, `48 83 EC ??`, etc. — try several patterns, pick the nearest match upstream).
8. Read N bytes (start with 32) from the prologue. Use Capstone to disassemble. Generate a stable AOB pattern by wildcarding obvious relocatable operands (call/jmp displacements `?? ?? ?? ??` after `E8`/`E9`, RIP-relative LEAs/MOVs).
9. Verify the generated pattern is unique by scanning `.text` — if it matches multiple sites, expand the captured byte window and retry.
10. Compute the field offset within the context object (the offset between the writer's destination address and the `this` register / first arg).
11. Print to stdout a paste-ready C++ block:

```cpp
// GENERATED by tools/sigfinder.py against Cyberpunk2077.exe build 2.21
// Captured at 2026-04-20 23:55 UTC
constexpr const char* kGameVersionTested = "2.21";
namespace gwheel::sigs {
    inline constexpr const char* kVehicleInputFnPattern =
        "48 89 5C 24 ?? 57 48 83 EC 20 ?? ?? ?? ?? ?? 8B ...";
    inline constexpr size_t kVehicleInputCtxThrottleOffset = 0x148;
    inline constexpr size_t kVehicleInputCtxBrakeOffset    = 0x14C;
    inline constexpr size_t kVehicleInputCtxSteerOffset    = 0x150;
}
```

12. Maintainer pastes this block into `gwheel/src/sigs.h`, rebuilds, releases.

The tool does NOT modify `sigs.h` directly — it prints to stdout for the human to paste. Less surface area for tool bugs to silently break the released plugin.

## Concrete file changes

### Delete

- `gwheel/src/vigem_pad.h`
- `gwheel/src/vigem_pad.cpp`
- The `gwheel/vendor/ViGEmClient/` submodule (run `git submodule deinit -f gwheel/vendor/ViGEmClient`, then `git rm -f gwheel/vendor/ViGEmClient`, then clean `.git/modules/gwheel/vendor/ViGEmClient`, then verify `.gitmodules` no longer references it)

### Create

- `gwheel/src/sigs.h` — table of patterns + offsets. Initially empty values, with a top-of-file comment block explaining the workflow (run sigfinder, paste output, rebuild). Also declare a `kGameVersionTested` string for diagnostic logging.
- `gwheel/src/sigscan.h` / `gwheel/src/sigscan.cpp` — AOB pattern matcher walking `.text`. API:
  ```cpp
  namespace gwheel::sigscan {
      uintptr_t Scan(const wchar_t* moduleName, std::string_view pattern);
      void ScanAll(const wchar_t* moduleName, std::string_view pattern,
                   std::vector<uintptr_t>& outMatches, size_t maxResults = 16);
      bool GetTextSection(const wchar_t* moduleName, uintptr_t& outBegin, uintptr_t& outEnd);
  }
  ```
  Pattern syntax: `"48 89 5C 24 ? 57"` — hex bytes separated by spaces, `?` or `??` for wildcard. Implementation: `GetModuleHandleW`, walk `IMAGE_DOS_HEADER` → `IMAGE_NT_HEADERS64` → `IMAGE_SECTION_HEADER`s, find `.text`, naive byte scan. Verbose debug logging. Reject malformed patterns with an error log + return 0.
- `gwheel/src/vehicle_hook.h` / `gwheel/src/vehicle_hook.cpp` — see runtime architecture above. Use `RED4ext::v1::Hooking::Attach` (look in `gwheel/vendor/RED4ext.SDK/include/RED4ext/Hooking/` to confirm the exact API surface — that's the correct hook attach API; do NOT add MinHook).
- `tools/sigfinder/sigfinder.py` — Python entry point implementing the workflow above.
- `tools/sigfinder/requirements.txt` — `frida-tools>=12`, `capstone>=5`, `pefile>=2023`.
- `tools/sigfinder/README.md` — short usage doc: "install requirements; launch CP2077 with a vehicle; run `python sigfinder.py`; follow prompts; paste output into `gwheel/src/sigs.h`; rebuild plugin."

### Modify

- `gwheel/CMakeLists.txt`:
  - Remove `add_subdirectory(vendor/ViGEmClient ...)` and `ViGEmClient` from the link list.
  - Remove `setupapi` from the link list (only ViGEm needed it).
  - Add `src/sigscan.cpp`, `src/vehicle_hook.cpp` to `GWHEEL_SOURCES`. Add headers to `GWHEEL_HEADERS`.
  - Keep `dinput8`, `dxguid` (still used by `wheel_dinput`).
- `gwheel/src/plugin.cpp`:
  - Remove `#include "vigem_pad.h"` and all `vigem::` calls.
  - Add `#include "vehicle_hook.h"`. Call `vehicle_hook::Init()` during step 3, `vehicle_hook::Shutdown()` during unload.
  - Pump thread is now just `wheel::Pump()` — no more ViGEm submit.
- `gwheel/src/rtti.cpp`:
  - `GetDeviceInfo` should report the hook status: `"G923 (PC/USB, 900 deg) -> vehicle hook (active)"` or `"... -> vehicle hook (inactive: pattern TBD)"` based on `vehicle_hook::IsInstalled()`.
- `deploy.ps1`:
  - Remove the `Get-Service -Name ViGEmBus` block.
  - Update post-deploy "Next steps": replace ViGEm references with the `[gwheel:hook]` log line to grep for.
- `fomod/ModuleConfig.xml` — remove the ViGEmBus dependency group.
- `NEXUS_README.md` and `README.md` — remove all mention of ViGEmBus driver. Add a "Compatibility" note: this mod hooks `Cyberpunk2077.exe` directly via byte-pattern signatures hard-coded in the source. When CDPR patches the game, the maintainer regenerates the patterns with `tools/sigfinder/` and re-publishes. Track game-version tested in CHANGELOG.
- `CHANGELOG.md` — under v0.1.0 "Unreleased adjustments": "Removed ViGEmBus dependency in favor of hard-coded byte-pattern signatures for an in-process `Cyberpunk2077.exe` function detour. Signature discovery automated via `tools/sigfinder/`. Hook is currently inactive pending first sigfinder run against current game patch."

### Do NOT touch

- `gwheel/src/wheel_dinput.{h,cpp}` — the DirectInput layer works (deferred-acquire path lands when the game window appears). Leave it alone except for any cosmetic changes needed by `rtti.cpp` referencing it.
- `gwheel/src/config.{h,cpp}`, `gwheel/src/rtti.cpp` (other than `GetDeviceInfo`), `gwheel/src/logging.{h,cpp}`, `gwheel/src/device_table.{h,cpp}`, `gwheel/src/dllmain.cpp`, `gwheel/src/plugin.h`, `gwheel/src/rtti_dump.{h,cpp}`.
- `gwheel/vendor/RED4ext.SDK/` — submodule, pinned.
- `gwheel/vendor/LogitechSDK_unpacked/` — user keeps this on disk even though no source references it.
- `mod_info.json`, `ARCHITECTURE.md`, `gwheel_reds/*.reds`, `fomod/info.xml`, `gwheel/include/gwheel_abi.h`.

## Constraints (repeating because they matter)

- **NO PLACEHOLDER PATTERNS** that pretend to work. `sigs.h` ships with empty pattern strings and a comment explaining the workflow. The plugin detects empty and logs an inactive message, not a success.
- **DO NOT BUMP THE VERSION.** Still v0.1.0.
- **DO NOT COMMIT.** Stage everything; the user commits at the end.
- **NO TODO STUBS, NO HALF-FINISHED FUNCTIONS.** Either it works or it logs honestly that it's inactive.
- **STATIC CRT, MSVC, x64, C++20.** Existing repo settings.
- **NO EMOJIS** in code or docs.
- **One-line comments only**, except the multi-paragraph block at the top of `sigs.h` (which IS justified — it's the workflow doc for whoever updates it next).
- **Verbose logging** at every step.

## Verification steps

After all the changes:

1. Build:
   ```
   powershell -ExecutionPolicy Bypass -File build.ps1 -Config Release
   ```
   Must succeed. Warnings from `wheel_dinput`/`vehicle_hook`/`sigscan` should be zero. Vendored library warnings (RED4ext.SDK) are fine to ignore.

2. Deploy:
   ```
   powershell -ExecutionPolicy Bypass -File deploy.ps1 -Game "S:\SteamLibrary\steamapps\common\Cyberpunk 2077"
   ```
   Must succeed. No ViGEmBus check.

3. Launch CP2077. Read the latest `red4ext\logs\gwheel-*.log`. Expect to see:
   - `[gwheel] loaded v0.1.0`
   - `[gwheel] wheel::Init device ready; deferring Acquire until the game window appears`
   - (after main menu loads) `[gwheel] device acquired (DirectInput)`
   - `[gwheel] firing hello pulse` (wheel kicks left then right)
   - `[gwheel:hook] <slot> pattern not configured — hook inactive. Run tools/sigfinder.py and update sigs.h.`
   The hello-pulse confirmation is the gate: if it fires, DirectInput + FFB are live. The hook-inactive message is expected — that's why we built the sigfinder.

4. Smoke-test the sigfinder tool (best-effort — no end-to-end path until the user runs it against a live CP2077):
   ```
   cd tools/sigfinder
   pip install -r requirements.txt
   python sigfinder.py --help
   ```
   Should print usage without import errors. End-to-end discovery validation is the user's manual test step after handoff.

## Suggested commit message (when the user is ready)

```
Pivot: Python sigfinder + hard-coded signatures, drop ViGEmBus

- Remove ViGEmClient submodule and vigem_pad.{h,cpp}; no driver dependency.
- Add gwheel/src/sigs.h (signature table; initially empty pending discovery).
- Add gwheel/src/sigscan.{h,cpp} (AOB scanner over Cyberpunk2077.exe .text).
- Add gwheel/src/vehicle_hook.{h,cpp} (sigscan + RED4ext detour).
- Add tools/sigfinder/ (Python+Frida discovery tool; not shipped).
- DirectInput wheel I/O + FFB unchanged; deferred-acquire path retained.
- Plugin remains v0.1.0; hook logs inactive until sigs.h is populated.
```
