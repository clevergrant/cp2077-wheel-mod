# sigfinder

Developer-side tool for producing the byte-pattern signatures in
[`gwheel/src/sigs.h`](../../gwheel/src/sigs.h). **Not shipped to end users.**
Run this each time CDPR patches `Cyberpunk2077.exe` and the vehicle hook
stops landing.

## Install

```pwsh
cd tools\sigfinder
pip install -r requirements.txt
```

## Usage

1. Launch Cyberpunk 2077. Load a save where V is sitting in a drivable
   vehicle, engine on, on flat ground, not moving. Leave the game focused;
   do not tab away during the capture.
2. In a second terminal:
   ```pwsh
   python sigfinder.py
   ```
3. Follow the prompts. The script walks you through three axes (throttle,
   brake, steer). For each axis:
   - It snapshots the game's writable memory.
   - It asks you to hold the axis for ~2 seconds, then release.
   - It diffs for floats whose value swings in the expected direction.
   - It arms `MemoryAccessMonitor` on the surviving candidates and asks you
     to nudge the axis once more to capture the writing instruction.
4. It walks back from the writer RIP to the function prologue, disassembles
   32 bytes, and generates a stable AOB pattern by wildcarding relocatable
   operands.
5. It verifies the pattern is unique across `.text`. If not, it widens the
   capture window and retries.
6. It computes the input-field offsets relative to the context object (the
   `this` register at function entry).
7. It prints a paste-ready block marked between `--- BEGIN sigfinder output ---`
   and `--- END sigfinder output ---` comments. Paste that block over the
   matching block in `gwheel/src/sigs.h`, commit, and rebuild `gwheel.dll`.

## Notes

- The tool attaches via Frida. Anti-cheat tools will flag this; run against
  an unmodified CP2077 (no online features) or accept the risk.
- The tool **never writes** `sigs.h` directly. It only prints to stdout so
  a human reviews the output before it gets committed.
- If a pass fails (no candidates, or pattern stays ambiguous), widen the
  differential window or capture more samples by pressing Ctrl+R at the
  prompt.

## Flags

```
python sigfinder.py --help
```

```
--process NAME    Target process name (default: Cyberpunk2077.exe).
--dry-run         Attach, print module layout, detach. No capture.
--no-fft          Skip action-dispatch discovery (just produce the
                  vehicle-input block).
--log-level L     Python logging level (default: INFO).
```
