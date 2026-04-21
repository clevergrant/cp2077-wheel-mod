#!/usr/bin/env python3
"""
sigfinder.py

Developer-side tool that produces the hard-coded byte-pattern signatures and
field offsets consumed by gwheel/src/sigs.h. Attaches to a running
Cyberpunk2077.exe via Frida, differentially scans memory for the active
input floats, uses MemoryAccessMonitor to capture the writer instruction,
walks back to the function prologue, and emits a paste-ready C++ block on
stdout.

This tool is NOT shipped to end users. Only the output of a successful run
(pasted into gwheel/src/sigs.h) ends up in the shipped plugin.
"""

from __future__ import annotations

import argparse
import logging
import struct
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable

try:
    import frida
except ImportError:
    print("ERROR: frida is not installed. Run: pip install -r requirements.txt", file=sys.stderr)
    sys.exit(2)

try:
    import capstone
except ImportError:
    print("ERROR: capstone is not installed. Run: pip install -r requirements.txt", file=sys.stderr)
    sys.exit(2)

try:
    import pefile
except ImportError:
    print("ERROR: pefile is not installed. Run: pip install -r requirements.txt", file=sys.stderr)
    sys.exit(2)


# --- Shared Frida script ---------------------------------------------------
#
# Exposes a small RPC surface we can call from Python: enumerate_ranges,
# snapshot, diff, arm_access_monitor, read_bytes.
#
# Kept in Python as a string to avoid shipping multiple files for the tool.

AGENT_SRC = r"""
'use strict';

function enumModule(name) {
    const m = Process.findModuleByName(name);
    if (!m) throw new Error('module not found: ' + name);
    return {
        name: m.name,
        base: m.base.toString(),
        size: m.size,
        path: m.path,
    };
}

function enumTextSection(name) {
    const m = Process.findModuleByName(name);
    if (!m) throw new Error('module not found: ' + name);
    const dosE_lfanew = m.base.add(0x3C).readU32();
    const nt = m.base.add(dosE_lfanew);
    const fileHeader = nt.add(4);
    const numberOfSections = fileHeader.add(2).readU16();
    const optionalHeaderSize = fileHeader.add(16).readU16();
    const sections = fileHeader.add(20).add(optionalHeaderSize);
    for (let i = 0; i < numberOfSections; i++) {
        const s = sections.add(i * 40);
        const raw = s.readByteArray(8);
        const bytes = new Uint8Array(raw);
        let name2 = '';
        for (let j = 0; j < 8 && bytes[j] !== 0; j++) name2 += String.fromCharCode(bytes[j]);
        if (name2 === '.text') {
            const vsize = s.add(8).readU32();
            const vaddr = s.add(12).readU32();
            return { begin: m.base.add(vaddr).toString(), size: vsize };
        }
    }
    throw new Error('.text not found in ' + name);
}

function listWritableFloatRanges() {
    // Committed, writable, non-guard pages. Skip huge allocations (shader
    // caches, texture pools) and cap the total so the scan stays sane.
    const out = [];
    const ranges = Process.enumerateRanges({ protection: 'rw-', coalesce: true });
    let total = 0;
    for (const r of ranges) {
        if (r.size > 4 * 1024 * 1024) continue;
        total += r.size;
        out.push({ base: r.base.toString(), size: r.size });
        if (total > 256 * 1024 * 1024) break;
    }
    return out;
}

// Read the full contents of the requested ranges and stream them back to
// Python as a sequence of chunks (<= 32 MiB each to stay well under Frida's
// 128 MiB per-message ceiling). Each chunk carries a seq number; the final
// message includes the full index so Python knows (base, offset, size) for
// each range within the concatenated stream.
const BULK_CHUNK_BYTES = 32 * 1024 * 1024;

function bulkReadRanges(ranges) {
    let totalBytes = 0;
    const index = [];
    const chunks = [];
    for (let i = 0; i < ranges.length; i++) {
        const r = ranges[i];
        let buf = null;
        try {
            buf = ptr(r.base).readByteArray(r.size);
        } catch (e) {
            index.push({ base: r.base, offset: totalBytes, size: 0 });
            continue;
        }
        const size = buf.byteLength;
        index.push({ base: r.base, offset: totalBytes, size: size });
        chunks.push(new Uint8Array(buf));
        totalBytes += size;
        if ((i & 63) === 0) {
            send({ type: 'progress', stage: 'read', done: i + 1, total: ranges.length });
        }
    }
    send({ type: 'progress', stage: 'read', done: ranges.length, total: ranges.length });

    // Stream out in BULK_CHUNK_BYTES-sized frames. We build a linear view
    // across all the small per-range ArrayBuffers and cut it into chunks.
    const combined = new Uint8Array(totalBytes);
    let cursor = 0;
    for (const c of chunks) {
        combined.set(c, cursor);
        cursor += c.byteLength;
    }

    const totalChunks = Math.max(1, Math.ceil(totalBytes / BULK_CHUNK_BYTES));
    send({ type: 'bulk_begin', totalBytes: totalBytes, totalChunks: totalChunks, index: index });
    for (let seq = 0; seq < totalChunks; seq++) {
        const off = seq * BULK_CHUNK_BYTES;
        const end = Math.min(off + BULK_CHUNK_BYTES, totalBytes);
        const frame = combined.slice(off, end);
        send({ type: 'bulk_chunk', seq: seq, totalChunks: totalChunks, size: end - off }, frame.buffer);
        send({ type: 'progress', stage: 'ship', done: seq + 1, total: totalChunks });
    }
    send({ type: 'bulk_end' });
    return true;
}

function readFloat(addr) {
    return ptr(addr).readFloat();
}

// Scan all writable memory on the agent side and ship back ONLY the
// addresses whose float value falls inside [lo, hi]. Used for the seed
// sample - avoids shipping 200+ MB of floats that don't match the band.
// Caps the match set at `maxMatches` (returns as many as it found if
// scanning aborts early because we hit the cap).
function agentScanFiltered(lo, hi, maxMatches) {
    const matches = {};
    let matchCount = 0;
    const ranges = Process.enumerateRanges({ protection: 'rw-', coalesce: true });
    let totalBudget = 0;
    const eligible = [];
    for (const r of ranges) {
        if (r.size > 4 * 1024 * 1024) continue;
        totalBudget += r.size;
        eligible.push(r);
        if (totalBudget > 256 * 1024 * 1024) break;
    }

    let bytesProcessed = 0;
    let capped = false;
    for (let i = 0; i < eligible.length && !capped; i++) {
        const r = eligible[i];
        let buf;
        try {
            buf = r.base.readByteArray(r.size);
        } catch (e) {
            bytesProcessed += r.size;
            continue;
        }
        const view = new Float32Array(buf);
        const baseAddr = r.base;
        for (let k = 0; k < view.length; k++) {
            const v = view[k];
            if (v !== v) continue;            // NaN
            if (v >  1e30) continue;
            if (v < -1e30) continue;
            if (v < lo || v > hi) continue;
            matches[baseAddr.add(k * 4).toString()] = v;
            matchCount += 1;
            if (matchCount >= maxMatches) {
                capped = true;
                break;
            }
        }
        bytesProcessed += r.size;
        if ((i & 31) === 0) {
            send({ type: 'progress', stage: 'agent-scan',
                   done: bytesProcessed, total: totalBudget });
        }
    }
    send({ type: 'progress', stage: 'agent-scan',
           done: totalBudget, total: totalBudget });
    return { values: matches, matchCount: matchCount, capped: capped };
}

// Read float values at a set of addresses and ship back ONLY those whose
// value falls inside [lo, hi]. Used for sample 2+ in the narrowing pass.
function readFloatsAtFiltered(addrsHex, lo, hi) {
    const out = {};
    const errors = {};
    const n = addrsHex.length;
    for (let i = 0; i < n; i++) {
        const a = ptr(addrsHex[i]);
        let v;
        try {
            v = a.readFloat();
        } catch (e) {
            errors[addrsHex[i]] = true;
            continue;
        }
        if (v !== v) continue;           // NaN
        if (v >  1e30 || v < -1e30) continue;
        if (v < lo || v > hi) continue;
        out[addrsHex[i]] = v;
        if ((i & 0xFFFF) === 0) {
            send({ type: 'progress', stage: 'reread', done: i + 1, total: n });
        }
    }
    send({ type: 'progress', stage: 'reread', done: n, total: n });
    return { values: out, unreadable: Object.keys(errors).length };
}

function readBytes(addr, n) {
    const buf = ptr(addr).readByteArray(n);
    const u = new Uint8Array(buf);
    let s = '';
    for (let i = 0; i < u.length; i++) {
        const h = u[i].toString(16);
        s += (h.length === 1 ? '0' : '') + h;
    }
    return s;
}

// Ship a large .text window back as raw bytes (for verify_unique).
function readBytesBulk(addr, n) {
    const buf = ptr(addr).readByteArray(n);
    send({ type: 'bulk_bytes', size: buf.byteLength }, buf);
    return buf.byteLength;
}

// Legacy PAGE_GUARD-based access monitor. Kept for reference but we don't
// rely on it - Frida's implementation on Windows fires zero events on the
// kinds of heap pages game-input floats live on.
let _hits = {};
let _armedRanges = [];
let _totalAccesses = 0;
let _writeCount = 0;
let _readCount = 0;

function _onAccess(details) {
    _totalAccesses += 1;
    if (details.operation === 'write') _writeCount += 1;
    else if (details.operation === 'read') _readCount += 1;
    const key = details.from.toString() + ':' + details.address.toString() + ':' + details.operation;
    if (!_hits[key]) {
        _hits[key] = {
            from: details.from.toString(),
            address: details.address.toString(),
            operation: details.operation,
            rangeIndex: details.rangeIndex,
        };
    }
    try {
        const r = _armedRanges[details.rangeIndex];
        if (r) MemoryAccessMonitor.enable([r], { onAccess: _onAccess });
    } catch (e) {}
}

function armAccessMonitor(ranges) {
    _hits = {};
    _totalAccesses = 0;
    _writeCount = 0;
    _readCount = 0;
    _armedRanges = ranges.map(r => ({ base: ptr(r.base), size: r.size }));
    MemoryAccessMonitor.enable(_armedRanges, { onAccess: _onAccess });
    return { armed: _armedRanges.length };
}

function collectAccessHits() {
    const out = [];
    for (const k in _hits) out.push(_hits[k]);
    try { MemoryAccessMonitor.disable(); } catch (e) {}
    const summary = { hits: out, totalAccesses: _totalAccesses,
                      writes: _writeCount, reads: _readCount };
    _hits = {};
    _armedRanges = [];
    _totalAccesses = 0;
    _writeCount = 0;
    _readCount = 0;
    return summary;
}

// ---------------------------------------------------------------------------
// Hardware watchpoint capture (the primary writer-capture path).
//
// Uses Frida >= 16.5's thread.setHardwareWatchpoint() which wraps the
// Windows DR0-DR3 debug registers under the hood. Each thread has its own
// debug registers, so we arm DR0 on every game thread. The first thread to
// fire wins; on hit we record RIP and unset DR0 on every thread we touched.
//
// Slot 0 on every thread. Size 4. Conditions 'w' (write) or 'rw'.
// ---------------------------------------------------------------------------

let _hwArmed = false;
let _hwAddrHex = null;
let _hwSize = 4;
let _hwConds = 'w';
let _hwThreads = [];    // array of frida Thread objects we armed
let _hwHits = [];
let _hwHandlerSet = false;

function _hwExceptionHandler(details) {
    // Only care about watchpoint-derived exceptions. On Windows x64 these
    // surface as type 'single-step' (per Intel, write watchpoints are trap
    // after), though some Frida builds classify them as 'breakpoint'.
    if (!_hwArmed) return false;
    if (details.type !== 'single-step' && details.type !== 'breakpoint') {
        return false;
    }
    try {
        const rip = details.context && details.context.rip
            ? details.context.rip.toString() : '0x0';
        _hwHits.push({
            rip: rip,
            tid: Process.getCurrentThreadId(),
            type: details.type,
        });
        // Disarm on every thread we armed - the first hit is enough.
        for (const t of _hwThreads) {
            try { t.unsetHardwareWatchpoint(0); } catch (e) {}
        }
        _hwThreads = [];
        _hwArmed = false;
        return true;   // signal handled, resume execution
    } catch (e) {
        return false;
    }
}

function armHwWriteWatch(addrHex, size, conds) {
    if (!_hwHandlerSet) {
        Process.setExceptionHandler(_hwExceptionHandler);
        _hwHandlerSet = true;
    }
    _hwAddrHex = addrHex;
    _hwSize = size || 4;
    _hwConds = conds || 'w';
    _hwHits = [];
    _hwThreads = [];
    const myTid = Process.getCurrentThreadId();
    const threads = Process.enumerateThreads();
    const addr = ptr(addrHex);
    let armed = 0;
    let errors = [];
    for (const t of threads) {
        if (t.id === myTid) continue;
        try {
            t.setHardwareWatchpoint(0, addr, _hwSize, _hwConds);
            _hwThreads.push(t);
            armed += 1;
        } catch (e) {
            errors.push({ tid: t.id, error: String(e) });
        }
    }
    _hwArmed = armed > 0;
    return { armed: armed, totalThreads: threads.length, errors: errors.slice(0, 5) };
}

function disarmHwWriteWatch() {
    for (const t of _hwThreads) {
        try { t.unsetHardwareWatchpoint(0); } catch (e) {}
    }
    _hwThreads = [];
    _hwArmed = false;
    return true;
}

function collectHwHits() {
    const out = _hwHits.slice();
    _hwHits = [];
    return { hits: out };
}

// ---------------------------------------------------------------------------
// Runtime caller tracing via Interceptor.attach.
//
// Attach at an arbitrary address; every time execution reaches it, we read
// this.returnAddress (which is RSP[0] at the hook - the caller's resume
// address). We bucket-count and keep only addresses inside [textBegin, textEnd).
//
// NOTE: if our plugin's own PolyHook trampoline has patched the same address,
// Frida's Interceptor will try to install a second trampoline there. In
// practice this either (a) works, because Frida's trampoline is placed over
// the PolyHook jmp, or (b) errors out on attach. We catch (b) and report it.
// ---------------------------------------------------------------------------

let _traceAttached = null;       // Interceptor listener
let _traceCounts = {};           // returnAddr hex -> count
let _traceTotalHits = 0;
let _traceTextBegin = 0;
let _traceTextEnd = 0;
let _traceFirstBytes = null;

function armCallerTrace(targetHex, textBeginHex, textEndHex) {
    _traceCounts = {};
    _traceTotalHits = 0;
    _traceTextBegin = parseInt(textBeginHex, 16);
    _traceTextEnd = parseInt(textEndHex, 16);
    const target = ptr(targetHex);

    // Snapshot the first 16 bytes so Python can detect whether the target
    // has already been hooked (jmp trampoline pattern).
    try {
        const buf = target.readByteArray(16);
        const u = new Uint8Array(buf);
        let s = '';
        for (let i = 0; i < u.length; i++) {
            const h = u[i].toString(16);
            s += (h.length === 1 ? '0' : '') + h;
        }
        _traceFirstBytes = s;
    } catch (e) {
        _traceFirstBytes = null;
    }

    try {
        _traceAttached = Interceptor.attach(target, {
            onEnter(args) {
                try {
                    const ra = this.returnAddress;
                    const rn = ra.toUInt32 ? ra.toUInt32() : 0;
                    // We actually need the full 64-bit value - use string.
                    const raStr = ra.toString();
                    _traceTotalHits += 1;
                    _traceCounts[raStr] = (_traceCounts[raStr] || 0) + 1;
                } catch (e) {}
            },
        });
        Interceptor.flush();
        return { ok: true, firstBytes: _traceFirstBytes };
    } catch (e) {
        return { ok: false, error: String(e), firstBytes: _traceFirstBytes };
    }
}

function disarmCallerTrace() {
    try {
        if (_traceAttached) _traceAttached.detach();
        Interceptor.flush();
    } catch (e) {}
    _traceAttached = null;
    const out = {
        totalHits: _traceTotalHits,
        counts: _traceCounts,
        firstBytes: _traceFirstBytes,
    };
    _traceCounts = {};
    _traceTotalHits = 0;
    return out;
}

function peekCallerTrace() {
    // Return a live snapshot of counts without detaching, so Python can show
    // progress during the capture window.
    return { totalHits: _traceTotalHits, countsSize: Object.keys(_traceCounts).length };
}

rpc.exports = {
    enumModule: enumModule,
    enumTextSection: enumTextSection,
    listWritableFloatRanges: listWritableFloatRanges,
    bulkReadRanges: bulkReadRanges,
    readFloat: readFloat,
    readFloatsAtFiltered: readFloatsAtFiltered,
    agentScanFiltered: agentScanFiltered,
    readBytes: readBytes,
    readBytesBulk: readBytesBulk,
    armAccessMonitor: armAccessMonitor,
    collectAccessHits: collectAccessHits,
    armHwWriteWatch: armHwWriteWatch,
    disarmHwWriteWatch: disarmHwWriteWatch,
    collectHwHits: collectHwHits,
    armCallerTrace: armCallerTrace,
    disarmCallerTrace: disarmCallerTrace,
    peekCallerTrace: peekCallerTrace,
};
"""


# --- Axis capture spec -----------------------------------------------------

@dataclass
class SampleSpec:
    prompt: str
    lo: float
    hi: float
    min_swing_from_prev: float = 0.0   # require |v_i - v_{i-1}| > this
    swing_dir: int = 0                  # +1 must increase, -1 must decrease, 0 either
    held: bool = False                  # True = "hold a key through the snapshot"
    held_key: str = ""                  # e.g. "W", "D", "A"


@dataclass
class AxisSpec:
    name: str
    friendly: str
    samples: list[SampleSpec] = field(default_factory=list)


# IMPORTANT: drive the game's input via KEYBOARD (or an already-working
# controller), NOT the steering wheel. The wheel is only wired up to the
# plugin's own memory; until the sigfinder-generated signatures are installed,
# CP2077 has no idea the wheel exists and its throttle/brake/steer floats do
# NOT change when the wheel is moved. Using keyboard guarantees the game's
# input pipeline actually writes to the memory we're trying to find.
# IMPORTANT: each axis starts with the HELD state. The held-state band is
# narrow (e.g. [0.70, 1.05]) and matches few addresses - the seed set stays
# small. Starting with the off-state would match every near-zero float in
# memory (tens of millions), overloading the agent-RPC pipeline.
#
# Prompts are input-device-agnostic. "accelerate forward" / "brake" / "steer
# right" applies equally to keyboard, controller triggers, steering wheel,
# or any other input the game accepts. The important thing is that the
# game's input pipeline sees the axis at max or idle on each step.
VEHICLE_AXES = [
    AxisSpec(
        name="throttle",
        friendly="throttle (accelerate forward)",
        samples=[
            SampleSpec(
                prompt="STEP 1/3 THROTTLE: hold ACCELERATE at full (input all the way on)",
                lo=0.70, hi=1.05,
                held=True, held_key="accelerate",
            ),
            SampleSpec(
                prompt="STEP 2/3 THROTTLE: release ACCELERATE completely (input off)",
                lo=-0.02, hi=0.02,
                min_swing_from_prev=0.5, swing_dir=-1,
            ),
            SampleSpec(
                prompt="STEP 3/3 THROTTLE: hold ACCELERATE at full again",
                lo=0.70, hi=1.05,
                min_swing_from_prev=0.5, swing_dir=+1,
                held=True, held_key="accelerate",
            ),
        ],
    ),
    AxisSpec(
        name="brake",
        friendly="brake (slow / reverse)",
        samples=[
            SampleSpec(
                prompt="STEP 1/3 BRAKE: hold BRAKE at full (input all the way on)",
                lo=0.70, hi=1.05,
                held=True, held_key="brake",
            ),
            SampleSpec(
                prompt="STEP 2/3 BRAKE: release BRAKE completely (input off)",
                lo=-0.02, hi=0.02,
                min_swing_from_prev=0.5, swing_dir=-1,
            ),
            SampleSpec(
                prompt="STEP 3/3 BRAKE: hold BRAKE at full again",
                lo=0.70, hi=1.05,
                min_swing_from_prev=0.5, swing_dir=+1,
                held=True, held_key="brake",
            ),
        ],
    ),
    AxisSpec(
        name="steer",
        friendly="steer (left / right)",
        samples=[
            SampleSpec(
                prompt="STEP 1/5 STEER: hold STEER RIGHT at full",
                lo=0.40, hi=1.05,
                held=True, held_key="steer-right",
            ),
            SampleSpec(
                prompt="STEP 2/5 STEER: release all steering input (centered)",
                lo=-0.05, hi=0.05,
                min_swing_from_prev=0.3, swing_dir=-1,
            ),
            SampleSpec(
                prompt="STEP 3/5 STEER: hold STEER LEFT at full",
                lo=-1.05, hi=-0.40,
                min_swing_from_prev=0.3, swing_dir=-1,
                held=True, held_key="steer-left",
            ),
            SampleSpec(
                prompt="STEP 4/5 STEER: release all steering input (centered)",
                lo=-0.05, hi=0.05,
                min_swing_from_prev=0.3, swing_dir=+1,
            ),
            SampleSpec(
                prompt="STEP 5/5 STEER: hold STEER RIGHT at full again",
                lo=0.40, hi=1.05,
                min_swing_from_prev=0.3, swing_dir=+1,
                held=True, held_key="steer-right",
            ),
        ],
    ),
]

AXES_BY_NAME = {a.name: a for a in VEHICLE_AXES}


# --- Core workflow ---------------------------------------------------------

class BulkCollector:
    """Catches side-channel messages from the Frida agent."""

    def __init__(self) -> None:
        self.bulk_ranges_index: list[dict] | None = None
        self.bulk_ranges_payload: bytes | None = None
        self.bulk_bytes_payload: bytes | None = None
        self.last_progress: tuple[str, int, int] | None = None

        # Chunked reassembly state.
        self._bulk_total_bytes: int = 0
        self._bulk_total_chunks: int = 0
        self._bulk_received_chunks: dict[int, bytes] = {}
        self._bulk_pending_index: list[dict] | None = None
        self._bulk_ended: bool = False

    def on_message(self, message: dict, data: bytes | None) -> None:
        if message.get("type") != "send":
            if message.get("type") == "error":
                print(f"\n[agent error] {message.get('description', '')}", flush=True)
            return
        payload = message.get("payload") or {}
        kind = payload.get("type")
        if kind == "progress":
            stage = payload.get("stage", "?")
            done = payload.get("done", 0)
            total = payload.get("total", 0)
            self.last_progress = (stage, done, total)
            pct = (100.0 * done / total) if total else 0.0
            print(f"\r  [{stage}] {done}/{total} ({pct:5.1f}%)     ", end="", flush=True)
            if total and done >= total:
                print()  # newline once complete
        elif kind == "bulk_begin":
            self._bulk_total_bytes = int(payload.get("totalBytes") or 0)
            self._bulk_total_chunks = int(payload.get("totalChunks") or 0)
            self._bulk_received_chunks = {}
            self._bulk_pending_index = payload.get("index") or []
            self._bulk_ended = False
        elif kind == "bulk_chunk":
            seq = int(payload.get("seq", -1))
            if seq >= 0 and data is not None:
                self._bulk_received_chunks[seq] = data
        elif kind == "bulk_end":
            # Stitch.
            assembled = bytearray(self._bulk_total_bytes)
            cursor = 0
            for seq in range(self._bulk_total_chunks):
                chunk = self._bulk_received_chunks.get(seq)
                if chunk is None:
                    raise RuntimeError(f"missing chunk {seq}/{self._bulk_total_chunks}")
                assembled[cursor:cursor + len(chunk)] = chunk
                cursor += len(chunk)
            self.bulk_ranges_index = self._bulk_pending_index
            self.bulk_ranges_payload = bytes(assembled)
            self._bulk_received_chunks = {}
            self._bulk_ended = True
        elif kind == "bulk_bytes":
            self.bulk_bytes_payload = data or b""


def attach(process_name: str) -> tuple[frida.core.Session, object, BulkCollector]:
    print(f"Attaching to {process_name} via Frida...", flush=True)
    session = frida.attach(process_name)
    script = session.create_script(AGENT_SRC)
    collector = BulkCollector()
    script.on("message", collector.on_message)
    script.load()
    return session, script.exports_sync, collector


def prompt(msg: str) -> None:
    print()
    print("  " + "-" * 76)
    try:
        input(f"  > {msg}\n  > Press Enter when ready: ")
    except (EOFError, KeyboardInterrupt):
        print("\nAborted.")
        sys.exit(130)


def countdown(seconds: float, leading: str) -> None:
    """Simple line-rewriting countdown printed to stdout."""
    t_end = time.time() + seconds
    while True:
        remaining = t_end - time.time()
        if remaining <= 0:
            break
        print(f"\r  {leading} {remaining:4.1f}s  ", end="", flush=True)
        time.sleep(0.1)
    print(f"\r  {leading} done.            ")


def snapshot_floats_bulk(rpc, collector: "BulkCollector", ranges: list[dict]) -> dict[int, float]:
    """Pull all bytes for `ranges` in a single bulk send and parse floats in Python.

    Returns a dict of absolute-address -> float value. Filters NaN/inf and
    |v| > 1e6 to cut noise.
    """
    collector.bulk_ranges_index = None
    collector.bulk_ranges_payload = None
    rpc.bulk_read_ranges(ranges)
    # Wait for the side-channel message to arrive.
    deadline = time.time() + 120.0
    while collector.bulk_ranges_payload is None and time.time() < deadline:
        time.sleep(0.01)
    if collector.bulk_ranges_payload is None:
        raise RuntimeError("bulk_read_ranges: timed out waiting for agent payload")

    buf = collector.bulk_ranges_payload
    out: dict[int, float] = {}
    for rec in collector.bulk_ranges_index or []:
        base = int(rec["base"], 16)
        off = int(rec["offset"])
        size = int(rec["size"])
        if size < 4:
            continue
        chunk = buf[off:off + size]
        # struct.iter_unpack is the fast path here.
        aligned = (size // 4) * 4
        values = struct.unpack_from(f"<{aligned // 4}f", chunk, 0)
        for i, v in enumerate(values):
            # Filter NaN/inf/huge.
            if v != v:  # NaN
                continue
            if v > 1e30 or v < -1e30:
                continue
            av = v if v >= 0 else -v
            if av > 1e6:
                continue
            out[base + i * 4] = v
    return out


def narrow_candidates(rpc, collector: "BulkCollector", axis: AxisSpec,
                      max_survivors: int = 256) -> list[int]:
    """Multi-sample narrowing. Keeps only addresses whose float values match
    every sample's expected range AND whose inter-sample swings match the
    expected direction/magnitude. This prunes aggressively - addresses
    "accidentally near 0" when the throttle is off do NOT survive because
    their values don't swing when the throttle is pressed.

    Returns up to `max_survivors` addresses, sorted by swing amplitude
    (descending). A hard cap matters because MemoryAccessMonitor uses
    PAGE_GUARD tricks that crash the target if armed on thousands of pages.
    """
    print()
    print("=" * 80)
    print(f"  AXIS: {axis.friendly}  ({len(axis.samples)} steps)")
    print("=" * 80)
    print(f"  Each step: read prompt -> do the wheel/pedal action in the game ->")
    print(f"  alt-tab back here -> press Enter. Then wait a few seconds for the scan.")

    # addr -> list of float values, one per sample so far.
    tracked: dict[int, list[float]] = {}
    for step_idx, sample in enumerate(axis.samples):
        print()
        print("  " + "-" * 76)
        print(f"  > {sample.prompt}")
        if sample.held:
            print(f"  > HOLD step. The flow:")
            print(f"  >   1. Press Enter in this window.")
            print(f"  >   2. You have 4 seconds to alt-tab to CP2077 and start the input.")
            print(f"  >   3. KEEP THE INPUT HELD for the entire scan.")
            print(f"  >   4. When the line says 'SCAN DONE - release now', release.")
            print(f"  >   5. Alt-tab back here for the next step.")
        else:
            print(f"  > RELEASE step. All vehicle inputs should be fully off.")
            print(f"  >   1. Press Enter in this window.")
            print(f"  >   2. You have 2 seconds to alt-tab to CP2077 (no inputs held).")
            print(f"  >   3. The snapshot runs for a few seconds.")
            print(f"  >   4. When the line says 'SCAN DONE', alt-tab back.")

        try:
            input("  > Press Enter to arm: ")
        except (EOFError, KeyboardInterrupt):
            print("\nAborted.")
            sys.exit(130)

        grace = 4.0 if sample.held else 2.0
        leading = ("alt-tab to game and START INPUT now; snapshot starts in"
                   if sample.held else
                   "alt-tab to game now; snapshot starts in")
        countdown(grace, leading)

        t0 = time.time()
        if step_idx == 0:
            # Seed: agent-side scan returns ONLY matching addresses. Bound
            # at 1,000,000 matches to keep transport sane.
            MAX_SEED = 1_000_000
            print(f"  seed scan (agent-side filter [{sample.lo:+.3f}, {sample.hi:+.3f}], "
                  f"cap {MAX_SEED})...", flush=True)
            result = rpc.agent_scan_filtered(sample.lo, sample.hi, MAX_SEED)
            values = result.get("values") or {}
            capped = bool(result.get("capped", False))
            snap = {}
            for k, v in values.items():
                if v is None:
                    continue
                try:
                    snap[int(k, 16)] = float(v)
                except (TypeError, ValueError):
                    continue
            print(f"  seed returned {len(snap)} addresses{' (capped)' if capped else ''}")
        else:
            # Re-read only the already-tracked addresses in batches (a single
            # RPC call with tens of millions of address strings exceeds Frida's
            # 128 MiB per-message cap).
            BATCH = 500_000
            addrs_hex = [hex(a) for a in tracked.keys()]
            total = len(addrs_hex)
            print(f"  re-reading {total} tracked addresses "
                  f"(agent filter [{sample.lo:+.3f}, {sample.hi:+.3f}]) in batches of {BATCH}...",
                  flush=True)
            all_values: dict[int, float] = {}
            unreadable_total = 0
            for i in range(0, total, BATCH):
                batch = addrs_hex[i:i + BATCH]
                result = rpc.read_floats_at_filtered(batch, sample.lo, sample.hi)
                values = result.get("values") or {}
                unreadable_total += int(result.get("unreadable", 0))
                for k, v in values.items():
                    if v is None:
                        continue
                    try:
                        all_values[int(k, 16)] = float(v)
                    except (TypeError, ValueError):
                        continue
                done = min(i + BATCH, total)
                print(f"\r  batch {done}/{total}  matches so far: {len(all_values)}   ",
                      end="", flush=True)
            print()
            snap = all_values
            print(f"  {len(snap)} addresses in range (unreadable total: {unreadable_total})")
        dt = time.time() - t0
        release_msg = "SCAN DONE - release input now" if sample.held else "SCAN DONE"
        print(f"\n  *** {release_msg} ***  ({dt:.1f}s)")

        if step_idx == 0:
            # Seed: every addr whose value is in range.
            for addr, v in snap.items():
                if sample.lo <= v <= sample.hi:
                    tracked[addr] = [v]
            print(f"  seeded {len(tracked)} candidates in [{sample.lo:+.3f}, {sample.hi:+.3f}]")
            continue

        # Filter the existing tracked set against this sample.
        kept: dict[int, list[float]] = {}
        range_hits = 0
        swing_hits = 0
        dir_hits = 0
        for addr, history in tracked.items():
            v = snap.get(addr)
            if v is None:
                continue
            if not (sample.lo <= v <= sample.hi):
                continue
            range_hits += 1
            prev = history[-1]
            delta = v - prev
            if abs(delta) < sample.min_swing_from_prev:
                continue
            swing_hits += 1
            if sample.swing_dir != 0:
                if sample.swing_dir > 0 and delta <= 0:
                    continue
                if sample.swing_dir < 0 and delta >= 0:
                    continue
            dir_hits += 1
            kept[addr] = history + [v]

        tracked = kept
        print(f"  sample {step_idx + 1}: range={range_hits} swing={swing_hits} "
              f"dir-ok={dir_hits}  surviving={len(tracked)}")
        if not tracked:
            print("  ! No candidates survived. Re-run sigfinder (try holding "
                  "the pedal/wheel more firmly and longer).")
            return []

    # Score by total swing amplitude across the sequence; largest first.
    def score(history: list[float]) -> float:
        total = 0.0
        for i in range(1, len(history)):
            total += abs(history[i] - history[i - 1])
        return total

    ranked = sorted(tracked.items(), key=lambda kv: score(kv[1]), reverse=True)
    if len(ranked) > max_survivors:
        print(f"  capping from {len(ranked)} to top {max_survivors} by swing score")
        ranked = ranked[:max_survivors]

    for addr, hist in ranked[:5]:
        preview = " -> ".join(f"{v:+.3f}" for v in hist)
        print(f"    top: 0x{addr:X}  {preview}  (score={score(hist):.2f})")

    # Dump top candidates to a file so the Cheat Engine / manual workflow can
    # pick them up if the writer-capture phase fails.
    try:
        runs_dir = Path(__file__).resolve().parent / "runs"
        runs_dir.mkdir(parents=True, exist_ok=True)
        import json
        out_path = runs_dir / f"candidates-{axis.name}.json"
        out_path.write_text(json.dumps([
            {
                "address": f"0x{addr:X}",
                "history": hist,
                "score": round(score(hist), 4),
            }
            for addr, hist in ranked[:50]
        ], indent=2))
        print(f"  wrote {out_path}")
    except Exception as e:
        print(f"  (failed to write candidates file: {e})")

    return [addr for addr, _ in ranked]


def capture_writer(rpc, candidates: list[int], axis_name: str,
                   capture_seconds: float = 10.0) -> dict | None:
    """Hardware-watchpoint-based writer capture. Picks the top scored
    candidate, arms DR0 for write on every game thread, waits for the user
    to exercise the input, returns the first captured RIP.

    Unlike MemoryAccessMonitor (which was silent on game heap pages), this
    uses Intel x86 debug registers via thread.setHardwareWatchpoint() and
    fires deterministically on every matching write.
    """
    if not candidates:
        return None
    # Pick the single best-scored candidate. 4 DR slots per thread is the
    # hardware cap; using one slot on one address keeps things simple and
    # reliable.
    target = candidates[0]
    target_hex = hex(target)
    print(f"  target address: {target_hex}")

    arm = rpc.arm_hw_write_watch(target_hex, 4, "w")
    armed = int(arm.get("armed", 0) if isinstance(arm, dict) else 0)
    total_threads = int(arm.get("totalThreads", 0) if isinstance(arm, dict) else 0)
    errs = arm.get("errors", []) if isinstance(arm, dict) else []
    print(f"  armed DR0 on {armed}/{total_threads} threads")
    for e in errs[:3]:
        print(f"    arm error tid={e.get('tid')}: {e.get('error')}")
    if armed == 0:
        print("  ! Could not arm any thread. Hardware watchpoints unavailable?")
        return None

    print()
    print("  " + "-" * 76)
    print(f"  WRITER CAPTURE ({axis_name}) via hardware watchpoint")
    print(f"  You have {capture_seconds:.0f} seconds:")
    print(f"    1. Press Enter below.")
    print(f"    2. Alt-tab to Cyberpunk 2077 - keep it focused.")
    if axis_name == "throttle":
        print(f"    3. Exercise the ACCELERATE input repeatedly (press + release, any pace).")
    elif axis_name == "brake":
        print(f"    3. Exercise the BRAKE input repeatedly (press + release, any pace).")
    else:
        print(f"    3. Exercise STEER LEFT + STEER RIGHT alternately.")
    print(f"    4. The script captures the first write and auto-disarms.")
    try:
        input("  Press Enter to arm: ")
    except (EOFError, KeyboardInterrupt):
        rpc.disarm_hw_write_watch()
        print("\nAborted.")
        sys.exit(130)

    t_end = time.time() + capture_seconds
    captured = None
    while time.time() < t_end:
        remaining = max(0.0, t_end - time.time())
        summary = rpc.collect_hw_hits()
        hits = summary.get("hits", []) if isinstance(summary, dict) else []
        if hits:
            captured = hits[0]
            break
        print(f"\r  waiting for write... {remaining:4.1f}s      ", end="", flush=True)
        time.sleep(0.25)
    print()
    rpc.disarm_hw_write_watch()

    if not captured:
        print("  ! No writes captured during window. The target address may not be "
              "the primary writer (mirror), or the input thread didn't run during "
              "the window. Try again.")
        return None

    rip = captured.get("rip")
    tid = captured.get("tid")
    print(f"  writer RIP = {rip} (thread {tid}, trap type '{captured.get('type')}')")
    return {"from": rip, "address": target_hex, "operation": "write"}


def walk_to_prologue(rpc, rip_hex: str, text_begin: int, text_size: int) -> tuple[int, bytes] | None:
    PROLOGUE_PREFIXES = [
        bytes.fromhex("48895c24"),  # mov [rsp+X], rbx
        bytes.fromhex("4889742418"),
        bytes.fromhex("40534883ec"),  # push rbx; sub rsp, X
        bytes.fromhex("4883ec"),      # sub rsp, X
        bytes.fromhex("48894c2408"),
    ]
    rip = int(rip_hex, 16)
    if rip < text_begin or rip > text_begin + text_size:
        print(f"  ! writer RIP 0x{rip:X} is outside Cyberpunk2077.exe .text; skipping.")
        return None
    # Scan backwards up to 2 KB looking for a prologue prefix on any 16-byte boundary.
    max_walk = 2048
    start = max(text_begin, rip - max_walk)
    # Read the window.
    window_hex = rpc.read_bytes(hex(start), rip - start + 8)
    window = bytes.fromhex(window_hex)
    best = None
    for prefix in PROLOGUE_PREFIXES:
        idx = window.rfind(prefix)
        if idx < 0:
            continue
        addr = start + idx
        if best is None or addr > best[0]:
            best = (addr, prefix)
    if not best:
        print(f"  ! Could not find a prologue prefix within {max_walk} bytes upstream of 0x{rip:X}.")
        return None
    prologue_addr = best[0]
    body_hex = rpc.read_bytes(hex(prologue_addr), 48)
    return prologue_addr, bytes.fromhex(body_hex)


def stabilize_pattern(md: capstone.Cs, prologue_addr: int, body: bytes) -> str:
    out = []
    for ins in md.disasm(body, prologue_addr):
        # Wildcard RIP-relative and branch operands; keep opcodes and reg encodings.
        mnemonic = ins.mnemonic
        if mnemonic in ("call", "jmp") and len(ins.bytes) >= 5 and ins.bytes[0] in (0xE8, 0xE9):
            out.append(f"{ins.bytes[0]:02X} ?? ?? ?? ??")
            continue
        if mnemonic.startswith("j") and len(ins.bytes) >= 2:
            # Keep opcode byte(s), wildcard the rel8/rel32.
            opcode_len = 2 if ins.bytes[0] == 0x0F else 1
            head = " ".join(f"{b:02X}" for b in ins.bytes[:opcode_len])
            tail = " ".join("??" for _ in ins.bytes[opcode_len:])
            out.append(f"{head} {tail}".strip())
            continue
        # RIP-relative LEAs / MOVs often have a 32-bit displacement. If the
        # instruction length is >= 7 and the disp is "big", wildcard the last 4.
        if len(ins.bytes) >= 7 and any(
            op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP
            for op in (ins.operands or [])
        ):
            keep = len(ins.bytes) - 4
            head = " ".join(f"{b:02X}" for b in ins.bytes[:keep])
            out.append(f"{head} ?? ?? ?? ??")
            continue
        out.append(" ".join(f"{b:02X}" for b in ins.bytes))
        if len(" ".join(out).split()) >= 32:
            break
    return " ".join(out)


def verify_unique(rpc, collector: "BulkCollector", pattern: str, text_begin: int, text_size: int) -> int:
    tokens = []
    for tok in pattern.split():
        tokens.append(None if tok.startswith("?") else int(tok, 16))
    chunk = 4 * 1024 * 1024
    count = 0
    off = 0
    print(f"  verifying uniqueness across {text_size / (1024 * 1024):.1f} MB of .text...", flush=True)
    while off < text_size:
        to_read = min(chunk + len(tokens), text_size - off)
        collector.bulk_bytes_payload = None
        rpc.read_bytes_bulk(hex(text_begin + off), to_read)
        deadline = time.time() + 30
        while collector.bulk_bytes_payload is None and time.time() < deadline:
            time.sleep(0.01)
        if collector.bulk_bytes_payload is None:
            raise RuntimeError("verify_unique: agent did not return bulk bytes in time")
        raw = collector.bulk_bytes_payload
        for i in range(0, len(raw) - len(tokens) + 1):
            ok = True
            for j, t in enumerate(tokens):
                if t is None:
                    continue
                if raw[i + j] != t:
                    ok = False
                    break
            if ok:
                count += 1
                if count > 1:
                    return count
        off += chunk
        pct = 100.0 * off / text_size
        print(f"\r  verify: {pct:5.1f}%     ", end="", flush=True)
    print()
    return count


def find_direct_callers(rpc, collector: "BulkCollector",
                         target_addr: int,
                         text_begin: int, text_size: int) -> list[int]:
    """Scan the game's .text for `call rel32` (opcode 0xE8) instructions
    whose target resolves to `target_addr`. Returns a list of RIPs at each
    call site (the address of the 0xE8 byte).

    Streams .text in 4 MiB chunks via the bulk-bytes agent RPC - same
    transport used by verify_unique - so we don't blow past Frida's 128 MiB
    per-message cap. No pattern matching, just a raw scan for 0xE8.
    """
    hits: list[int] = []
    chunk = 4 * 1024 * 1024
    off = 0
    print(f"  scanning {text_size / (1024 * 1024):.1f} MB of .text for direct "
          f"callers of 0x{target_addr:X}...", flush=True)
    while off < text_size:
        to_read = min(chunk + 8, text_size - off)
        collector.bulk_bytes_payload = None
        rpc.read_bytes_bulk(hex(text_begin + off), to_read)
        deadline = time.time() + 30
        while collector.bulk_bytes_payload is None and time.time() < deadline:
            time.sleep(0.01)
        if collector.bulk_bytes_payload is None:
            raise RuntimeError("find_direct_callers: agent did not return bulk bytes")
        raw = collector.bulk_bytes_payload
        # Walk each byte looking for 0xE8 followed by a rel32 that resolves to target.
        # Skip 0xE9 (jmp) - we want call sites only.
        for i in range(0, len(raw) - 5):
            if raw[i] != 0xE8:
                continue
            rel32 = int.from_bytes(raw[i + 1: i + 5], byteorder="little", signed=True)
            call_site = text_begin + off + i
            next_rip = call_site + 5
            if next_rip + rel32 == target_addr:
                hits.append(call_site)
        off += chunk
        pct = 100.0 * min(off, text_size) / text_size
        print(f"\r  caller-scan: {pct:5.1f}%  found={len(hits)}     ",
              end="", flush=True)
    print()
    return hits


def load_axis_pattern(axis_name: str) -> tuple[str, Path]:
    """Read gwheel/src/sigs.h and return the k{Axis}FnPattern literal."""
    sigs_h = Path(__file__).resolve().parent.parent.parent / "gwheel" / "src" / "sigs.h"
    if not sigs_h.exists():
        raise FileNotFoundError(f"sigs.h not found at {sigs_h}")
    sigs_text = sigs_h.read_text()
    axis_cap = axis_name.capitalize()
    slot = f"k{axis_cap}FnPattern"
    import re as _re
    m = _re.search(rf'{slot}\s*=\s*((?:"[^"]*"\s*)+);', sigs_text)
    if not m:
        raise RuntimeError(f"could not find {slot} in {sigs_h}")
    pattern = "".join(_re.findall(r'"([^"]*)"', m.group(1))).strip()
    return pattern, sigs_h


def sigscan_text(rpc, collector: "BulkCollector",
                 tokens: list[int | None],
                 text_begin: int, text_size: int,
                 label: str = "sigscan") -> list[int]:
    found: list[int] = []
    chunk_sz = 4 * 1024 * 1024
    o = 0
    scanned = 0
    timeouts = 0
    while o < text_size:
        to_read = min(chunk_sz + len(tokens), text_size - o)
        collector.bulk_bytes_payload = None
        rpc.read_bytes_bulk(hex(text_begin + o), to_read)
        deadline = time.time() + 30
        while collector.bulk_bytes_payload is None and time.time() < deadline:
            time.sleep(0.01)
        raw = collector.bulk_bytes_payload or b""
        if not raw:
            timeouts += 1
            print(f"\n[{label}] WARN: bulk_bytes timeout at off=0x{o:X}")
        scanned += len(raw)
        for i in range(0, len(raw) - len(tokens) + 1):
            ok = True
            for j, t in enumerate(tokens):
                if t is None:
                    continue
                if raw[i + j] != t:
                    ok = False
                    break
            if ok:
                found.append(text_begin + o + i)
                if len(found) > 1:
                    break
        pct = 100.0 * min(o + chunk_sz, text_size) / text_size
        print(f"\r  {label}: {pct:5.1f}%  scanned={scanned}  "
              f"matches={len(found)} timeouts={timeouts}   ",
              end="", flush=True)
        if len(found) > 1:
            break
        o += chunk_sz
    print()
    return found


def resolve_axis_entry(rpc, collector: "BulkCollector", md: capstone.Cs,
                       axis_name: str,
                       text_begin: int, text_size: int,
                       label: str = "resolve") -> tuple[int, int] | None:
    """Load the axis pattern from sigs.h, sigscan for it, and return
    (entry_addr, skip_used). Handles the case where the plugin's own
    PolyHook trampoline has replaced the first N bytes of the function
    by trying progressively deeper instruction-boundary skips.
    """
    pattern, _ = load_axis_pattern(axis_name)
    print(f"[{label}] k{axis_name.capitalize()}FnPattern = "
          f"{pattern[:80]}{'...' if len(pattern) > 80 else ''}")
    tokens_all = []
    for tok in pattern.split():
        tokens_all.append(None if tok.startswith("?") else int(tok, 16))
    print(f"[{label}] parsed {len(tokens_all)} tokens "
          f"({sum(1 for t in tokens_all if t is None)} wildcards)")

    literal_prefix = bytearray()
    for t in tokens_all:
        if t is None:
            break
        literal_prefix.append(t)
    skip_candidates = [0]
    try:
        off_c = 0
        for ins in md.disasm(bytes(literal_prefix), 0):
            off_c += ins.size
            if off_c >= len(literal_prefix):
                break
            skip_candidates.append(off_c)
    except Exception as e:
        print(f"[{label}] WARN: capstone disasm of prefix failed: {e}")
    print(f"[{label}] skip candidates (bytes from entry): {skip_candidates}")

    for skip in skip_candidates:
        tokens_skipped = tokens_all[skip:]
        if len(tokens_skipped) < 8:
            continue
        print(f"[{label}] trying skip={skip} ({len(tokens_skipped)} tokens)...")
        matches = sigscan_text(rpc, collector, tokens_skipped,
                               text_begin, text_size, label=label)
        if not matches:
            continue
        addrs = [m - skip for m in matches]
        if len(addrs) > 1:
            print(f"[{label}] ERROR: multiple matches after skip={skip} - "
                  f"pattern is ambiguous")
            return None
        if skip > 0:
            print(f"[{label}] matched after skipping {skip} bytes "
                  f"(likely hook trampoline on function entry)")
        return addrs[0], skip
    return None


def infer_ctx_offsets(addrs: dict[str, int]) -> dict[str, int]:
    # Trivial: assume all three axis addresses live in the same object and
    # report offsets relative to the lowest address. Ignore any __rip keys.
    clean = {k: v for k, v in addrs.items() if not k.endswith("__rip")}
    if not clean:
        return {}
    base = min(clean.values())
    return {name: addr - base for name, addr in clean.items()}


def emit_block(
    game_version: str,
    vehicle_pattern: str,
    ctx_offsets: dict[str, int],
    action_pattern: str | None,
) -> str:
    def off(name: str) -> str:
        return f"0x{ctx_offsets.get(name, 0):X}"

    lines = [
        "// --- BEGIN sigfinder output -----------------------------------------------",
        f"// Generated against Cyberpunk2077.exe game version \"{game_version}\".",
        f"// Captured at {time.strftime('%Y-%m-%d %H:%M:%S UTC', time.gmtime())}.",
        "",
        f'    inline constexpr const char* kGameVersionTested = "{game_version}";',
        "",
        f'    inline constexpr const char* kVehicleInputFnPattern = "{vehicle_pattern}";',
        f"    inline constexpr size_t kVehicleInputCtxThrottleOffset = {off('throttle')};",
        f"    inline constexpr size_t kVehicleInputCtxBrakeOffset    = {off('brake')};",
        f"    inline constexpr size_t kVehicleInputCtxSteerOffset    = {off('steer')};",
        f"    inline constexpr size_t kVehicleInputCtxClutchOffset   = 0;",
        f"    inline constexpr size_t kVehicleInputCtxHandbrakeOffset = 0;",
        "",
        f'    inline constexpr const char* kActionDispatchFnPattern = "{action_pattern or ""}";',
        "    inline constexpr size_t kActionDispatchNameHashOffset = 0;",
        "    inline constexpr size_t kActionDispatchValueOffset    = 0;",
        "// --- END sigfinder output -------------------------------------------------",
    ]
    return "\n".join(lines)


def read_game_version(module_path: str) -> str:
    try:
        pe = pefile.PE(module_path, fast_load=True)
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
        for fi in getattr(pe, "FileInfo", []):
            for entry in fi:
                if entry.Key.decode(errors="replace") == "StringFileInfo":
                    for st in entry.StringTable:
                        for k, v in st.entries.items():
                            if k == b"ProductVersion" or k == b"FileVersion":
                                return v.decode(errors="replace").strip()
    except Exception as e:
        logging.warning("could not read PE version info: %s", e)
    return "unknown"


# --- Entry point -----------------------------------------------------------

def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="Generate sigs.h signatures for gwheel.")
    ap.add_argument("--process", default="Cyberpunk2077.exe")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--no-fft", action="store_true", help="Skip action-dispatch discovery.")
    ap.add_argument("--log-level", default="INFO")
    ap.add_argument(
        "--axis",
        choices=["throttle", "brake", "steer", "all"],
        default="all",
        help="Capture only the named axis (default: all three).",
    )
    ap.add_argument(
        "--manual-rip",
        metavar="HEX",
        help="Skip narrowing + writer-capture. Take a manually-supplied writer "
             "RIP (as hex, e.g. 0x7FF6FD985432), walk back to the function "
             "prologue, and emit a pattern. Useful when MemoryAccessMonitor "
             "fails - pair with a Cheat Engine 'find what writes this address' "
             "run to get the RIP.",
    )
    ap.add_argument(
        "--manual-axis-addrs",
        metavar="LIST",
        default="",
        help="Comma-separated name=hex pairs, e.g. "
             "'steer=0x71D20F9730,throttle=0x71D28FB858'. Used with "
             "--manual-rip to compute context-offsets.",
    )
    ap.add_argument(
        "--find-callers",
        metavar="NAME",
        choices=["steer", "throttle", "brake"],
        help="Skip narrowing and writer-capture. Load the named axis pattern "
             "from gwheel/src/sigs.h, sigscan for the current absolute address "
             "(ASLR-aware), then scan CP2077.exe .text for direct `call rel32` "
             "call sites. Walk each back to its containing function prologue "
             "and emit stabilized patterns for each unique caller. Use when a "
             "captured function turns out to be a generic vtable-dispatching "
             "helper used by too many contexts.",
    )
    ap.add_argument(
        "--trace-callers",
        metavar="NAME",
        choices=["steer", "throttle", "brake"],
        help="Runtime caller tracing via Frida Interceptor.attach. Resolves "
             "the axis target (like --find-callers), attaches an onEnter hook, "
             "records return addresses for --trace-seconds, then reports top N "
             "callers with stabilized prologue patterns. Works for vtable-"
             "dispatched targets that --find-callers can't reach.",
    )
    ap.add_argument(
        "--trace-seconds",
        type=float,
        default=8.0,
        help="Duration of the --trace-callers capture window.",
    )
    ap.add_argument(
        "--trace-top-n",
        type=int,
        default=10,
        help="How many top callers to stabilize into patterns.",
    )
    args = ap.parse_args(argv)

    # Auto-log to tools/sigfinder/runs/ - a timestamped file per run plus a
    # rolling 'latest' copy. No Tee-Object needed on the caller side.
    runs_dir = Path(__file__).resolve().parent / "runs"
    runs_dir.mkdir(parents=True, exist_ok=True)
    ts = time.strftime("%Y%m%d-%H%M%S")
    axis_tag = args.axis
    log_path = runs_dir / f"sigfinder-{axis_tag}-{ts}.log"
    latest_path = runs_dir / f"sigfinder-{axis_tag}-latest.log"

    log_fh = open(log_path, "w", encoding="utf-8", buffering=1)

    class _Tee:
        def __init__(self, *streams): self._streams = streams
        def write(self, data):
            for s in self._streams:
                try:
                    s.write(data)
                    s.flush()
                except Exception:
                    pass
            return len(data) if isinstance(data, str) else 0
        def flush(self):
            for s in self._streams:
                try: s.flush()
                except Exception: pass
        def isatty(self): return False

    sys.stdout = _Tee(sys.__stdout__, log_fh)
    sys.stderr = _Tee(sys.__stderr__, log_fh)

    # Route logging to (the now-teed) stdout so both console + file capture
    # INFO lines from capstone / frida / our own logging calls.
    logging.basicConfig(
        level=getattr(logging, args.log_level.upper(), logging.INFO),
        format="%(levelname)s %(message)s",
        stream=sys.stdout,
    )

    print(f"[log] writing to {log_path}")

    session, rpc, collector = attach(args.process)
    try:
        mod = rpc.enum_module(args.process)
        tx = rpc.enum_text_section(args.process)
        text_begin = int(tx["begin"], 16)
        text_size = int(tx["size"])
        print(f"\nmodule base: {mod['base']}")
        print(f".text: 0x{text_begin:X} size 0x{text_size:X}")

        game_version = read_game_version(mod["path"])
        print(f"game version: {game_version}")

        print()
        print("=" * 80)
        print("  gwheel sigfinder")
        print("=" * 80)
        print("  IMPORTANT: drive the inputs from something the GAME already understands")
        print("  - keyboard (W/A/S/D), an Xbox/PS controller, or anything else CP2077")
        print("  is already reading. Do NOT use the steering wheel yet: the wheel is")
        print("  wired only to the plugin, and until this tool's output lands in")
        print("  sigs.h the game doesn't know the wheel exists.")
        print()
        print("  ANALOG inputs (controller triggers, thumbstick) are preferred over")
        print("  digital keys - the game's analog input pipeline is what we want to")
        print("  intercept for a smooth wheel feel.")
        print()
        print("  You will be walked through axes in this order: THROTTLE, BRAKE, STEER.")
        print("  (Or just one axis if --axis was passed.)")
        print("  For each prompt:")
        print("    1. Read the prompt in this window.")
        print("    2. Alt-tab to Cyberpunk 2077.")
        print("    3. Do what the prompt says with whatever input you're using.")
        print("    4. HOLD the input when the prompt says HOLD.")
        print("    5. Alt-tab back to this window.")
        print("    6. Press Enter. The scan runs for a few seconds.")
        print("  Keep V in the same vehicle, on flat ground, the whole time.")
        print("=" * 80)

        if args.dry_run:
            print("\n--dry-run specified; nothing to capture.")
            return 0

        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        md.detail = True

        # --- Caller-finder mode -----------------------------------------------
        if args.find_callers:
            resolved = resolve_axis_entry(
                rpc, collector, md, args.find_callers,
                text_begin, text_size, label="find-callers")
            if resolved is None:
                print(f"ERROR: could not resolve {args.find_callers} pattern. "
                      "If a gwheel detour is installed on this function, "
                      "temporarily remove the DLL and relaunch CP2077.")
                return 1
            target, used_skip = resolved
            print(f"[find-callers] resolved -> 0x{target:X} (skip={used_skip})")

            call_sites = find_direct_callers(rpc, collector, target,
                                             text_begin, text_size)
            print(f"\nDirect callers of 0x{target:X}: {len(call_sites)}")
            for cs in call_sites[:20]:
                print(f"  call-site 0x{cs:X}")
            if len(call_sites) > 20:
                print(f"  ... and {len(call_sites) - 20} more")

            # For each unique containing function, emit a stabilized pattern.
            seen_prologues: set[int] = set()
            unique_patterns: list[tuple[int, str, int]] = []
            for cs in call_sites:
                res = walk_to_prologue(rpc, hex(cs), text_begin, text_size)
                if not res:
                    continue
                prologue, body = res
                if prologue in seen_prologues:
                    continue
                seen_prologues.add(prologue)
                pat = stabilize_pattern(md, prologue, body)
                matches = verify_unique(rpc, collector, pat, text_begin, text_size)
                unique_patterns.append((prologue, pat, matches))

            print(f"\nUnique containing functions: {len(unique_patterns)}")
            for i, (prologue, pat, matches) in enumerate(unique_patterns):
                flag = "UNIQUE" if matches == 1 else f"non-unique ({matches} matches)"
                print(f"\n[{i}] prologue 0x{prologue:X}  {flag}")
                print(f"    pattern: {pat}")

            if not unique_patterns:
                print("\nNo direct callers found. Either the getter is called via "
                      "indirect call/jump, or the address doesn't match. Try a "
                      "different target.")
            return 0

        # --- Runtime caller-trace mode ----------------------------------------
        if args.trace_callers:
            resolved = resolve_axis_entry(
                rpc, collector, md, args.trace_callers,
                text_begin, text_size, label="trace-callers")
            if resolved is None:
                print(f"ERROR: could not resolve {args.trace_callers} pattern.")
                return 1
            target, used_skip = resolved
            text_end = text_begin + text_size
            print(f"[trace-callers] target 0x{target:X} (skip={used_skip})")

            # Snapshot entry bytes before arming. If they look like a hook
            # trampoline (E9 or FF 25) we warn - Frida's Interceptor may fight
            # with it, but we'll still try.
            entry_hex = rpc.read_bytes(hex(target), 16)
            entry_bytes = bytes.fromhex(entry_hex)
            print(f"[trace-callers] entry bytes: {' '.join(f'{b:02X}' for b in entry_bytes)}")
            if entry_bytes[0] == 0xE9 or (entry_bytes[0] == 0xFF and entry_bytes[1] == 0x25):
                print("[trace-callers] WARN: entry looks hooked (jmp trampoline). "
                      "Frida Interceptor may fail to attach - if so, temporarily "
                      "remove the DLL from red4ext/plugins/gwheel/ and relaunch.")

            print(f"[trace-callers] arming Interceptor at 0x{target:X} ...")
            arm = rpc.arm_caller_trace(hex(target), hex(text_begin), hex(text_end))
            if not (arm and arm.get("ok")):
                err = (arm or {}).get("error", "unknown")
                print(f"ERROR: arm_caller_trace failed: {err}")
                return 1
            print(f"[trace-callers] armed. Capturing for {args.trace_seconds:.1f}s.")
            print("[trace-callers] >>> Alt-tab to CP2077, get in a car, and exercise "
                  "the STEER axis left/right continuously. <<<")

            t_end = time.time() + args.trace_seconds
            last_peek = 0
            while time.time() < t_end:
                remaining = max(0.0, t_end - time.time())
                now = time.time()
                if now - last_peek > 0.5:
                    peek = rpc.peek_caller_trace() or {}
                    hits = int(peek.get("totalHits", 0) or 0)
                    uniq = int(peek.get("countsSize", 0) or 0)
                    print(f"\r  [trace] {remaining:4.1f}s left  hits={hits}  "
                          f"unique={uniq}   ", end="", flush=True)
                    last_peek = now
                time.sleep(0.05)
            print()

            summary = rpc.disarm_caller_trace() or {}
            counts_raw = summary.get("counts", {}) or {}
            total = int(summary.get("totalHits", 0) or 0)
            print(f"[trace-callers] disarmed. total hits={total}  unique={len(counts_raw)}")

            # Normalize. Keep only return addrs inside CP2077.exe .text.
            counts: dict[int, int] = {}
            for k, v in counts_raw.items():
                try:
                    ra = int(k, 16)
                except ValueError:
                    continue
                if ra < text_begin or ra >= text_end:
                    continue
                counts[ra] = counts.get(ra, 0) + int(v)

            ranked = sorted(counts.items(), key=lambda kv: kv[1], reverse=True)
            print(f"[trace-callers] {len(ranked)} unique callers inside .text")

            # Dump raw ranking for the record.
            try:
                import json
                out_path = runs_dir / f"callers-{args.trace_callers}.json"
                out_path.write_text(json.dumps([
                    {"return_addr": f"0x{a:X}", "hits": c}
                    for a, c in ranked[:200]
                ], indent=2))
                print(f"  wrote {out_path}")
            except Exception as e:
                print(f"  (failed to write callers file: {e})")

            top_n = max(1, args.trace_top_n)
            print(f"\nTop {min(top_n, len(ranked))} callers:")
            stabilized: list[tuple[int, int, int, str, int]] = []
            for return_addr, hits in ranked[:top_n]:
                res = walk_to_prologue(rpc, hex(return_addr), text_begin, text_size)
                if not res:
                    print(f"  ret=0x{return_addr:X} hits={hits}  "
                          f"[walk-back failed]")
                    continue
                prologue, body = res
                pat = stabilize_pattern(md, prologue, body)
                matches = verify_unique(rpc, collector, pat, text_begin, text_size)
                stabilized.append((return_addr, hits, prologue, pat, matches))

            print(f"\nStabilized patterns:")
            for i, (ra, hits, prologue, pat, matches) in enumerate(stabilized):
                flag = "UNIQUE" if matches == 1 else f"non-unique ({matches} matches)"
                print(f"\n[{i}] ret=0x{ra:X}  hits={hits}  prologue=0x{prologue:X}  {flag}")
                print(f"    pattern: {pat}")

            if not stabilized:
                print("\nNo stabilized patterns produced. Either Frida didn't "
                      "capture any calls (is the player in a car + moving "
                      "inputs?) or every caller landed outside .text.")
            return 0

        # --- Manual-RIP shortcut ----------------------------------------------
        if args.manual_rip:
            manual_rip_hex = args.manual_rip.lower().removeprefix("0x")
            try:
                int(manual_rip_hex, 16)
            except ValueError:
                print(f"ERROR: --manual-rip must be hex (got {args.manual_rip!r})")
                return 2

            axis_map: dict[str, int] = {}
            for pair in args.manual_axis_addrs.split(","):
                pair = pair.strip()
                if not pair or "=" not in pair:
                    continue
                name, hexstr = pair.split("=", 1)
                axis_map[name.strip()] = int(hexstr.strip(), 16)

            print(f"\n[manual] walking back from RIP 0x{manual_rip_hex} ...")
            res = walk_to_prologue(rpc, "0x" + manual_rip_hex, text_begin, text_size)
            if not res:
                print("[manual] prologue walk-back failed.")
                return 1
            prologue_addr, body = res
            vehicle_pattern = stabilize_pattern(md, prologue_addr, body)
            matches = verify_unique(rpc, collector, vehicle_pattern, text_begin, text_size)
            print(f"[manual] prologue at 0x{prologue_addr:X}")
            print(f"[manual] generated pattern ({len(vehicle_pattern.split())} tokens) matches {matches} site(s)")
            if matches != 1:
                print("[manual] ! pattern is not unique; widen the prologue walk-back window.")

            ctx_offsets = infer_ctx_offsets(axis_map) if axis_map else {}
            block = emit_block(game_version,
                               vehicle_pattern if matches == 1 else "",
                               ctx_offsets,
                               action_pattern=None)
            print("\n" + "=" * 78)
            print("Paste the block below over the matching block in gwheel/src/sigs.h.")
            print("=" * 78)
            print(block)
            return 0

        # --- Vehicle-input pass -----------------------------------------------
        selected_axes = VEHICLE_AXES if args.axis == "all" else [AXES_BY_NAME[args.axis]]
        axis_addrs: dict[str, int] = {}
        for axis in selected_axes:
            survivors = narrow_candidates(rpc, collector, axis)
            if not survivors:
                print(f"  ! Giving up on {axis.name}.")
                continue
            if len(survivors) > 32:
                print(f"  ! {len(survivors)} survivors is a lot; expect writer capture to struggle.")
            writer = capture_writer(rpc, survivors, axis.name)
            if not writer:
                continue
            axis_addrs[axis.name] = int(writer["address"], 16)
            axis_addrs[f"{axis.name}__rip"] = writer["from"]

        vehicle_pattern = ""
        if axis_addrs:
            # Pick the axis that gave us the clearest writer - steer is often
            # the strongest signal (largest absolute swings, fewest mirrors).
            picked_axis = None
            for preferred in ("steer", "throttle", "brake"):
                if preferred in axis_addrs:
                    picked_axis = preferred
                    break
            if picked_axis is None:
                picked_axis = next(iter(axis_addrs))
            picked_addr = axis_addrs[picked_axis]
            print(f"\n  Using {picked_axis} writer at 0x{picked_addr:X} for prologue walk-back.")
            # We already have the writer RIP from capture_writer above; stash
            # it into axis_addrs alongside the address so we can reuse it
            # without re-arming. For now, also accept a relock if needed.
            picked_rip = axis_addrs.get(f"{picked_axis}__rip")
            if picked_rip is None:
                print("  ! No cached RIP - this should have been captured earlier.")
            else:
                res = walk_to_prologue(rpc, picked_rip, text_begin, text_size)
                if res:
                    prologue_addr, body = res
                    vehicle_pattern = stabilize_pattern(md, prologue_addr, body)
                    matches = verify_unique(rpc, collector, vehicle_pattern, text_begin, text_size)
                    print(f"  generated pattern ({len(vehicle_pattern.split())} tokens) "
                          f"matches {matches} site(s)")
                    if matches != 1:
                        print("  ! pattern is not unique; try a different axis or "
                              "widen the prologue walk-back window.")
                        vehicle_pattern = ""

        ctx_offsets = infer_ctx_offsets(axis_addrs)

        # --- Action-dispatch pass ---------------------------------------------
        action_pattern = ""
        if not args.no_fft:
            print("\n== Action-dispatch pass ==")
            print("  The action-dispatch discovery is user-guided and not fully automated. "
                  "Skipping for now; leave kActionDispatchFnPattern empty and rerun later.")

        # --- Emit -------------------------------------------------------------
        block = emit_block(game_version, vehicle_pattern, ctx_offsets, action_pattern)
        print("\n" + "=" * 78)
        print("Paste the block below over the matching block in gwheel/src/sigs.h.")
        print("=" * 78)
        print(block)
        return 0
    finally:
        session.detach()
        try:
            log_fh.flush()
            log_fh.close()
            # Refresh the 'latest' copy so I can always grep the most recent run.
            import shutil
            shutil.copyfile(log_path, latest_path)
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
