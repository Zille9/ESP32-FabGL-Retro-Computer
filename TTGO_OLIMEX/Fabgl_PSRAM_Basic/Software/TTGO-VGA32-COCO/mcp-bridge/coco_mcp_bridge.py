#!/usr/bin/env python3
"""
CoCo 3 ESP32 emulator — MCP bridge.

Translates the device's small HTTP/JSON debug API (see
TTGO-VGA32-COCO/docs/wifi-debug.md) into MCP tools so an agent (Claude Desktop /
Claude Code) can inspect and control the running 6809.

The device exposes form/query-parameter endpoints; each tool here is a thin
httpx call. Configure the device address with the COCO_DEBUG_HOST environment
variable, e.g.:

    COCO_DEBUG_HOST=http://192.168.1.50

Run:  python coco_mcp_bridge.py
"""

import os
import time

import httpx
from mcp.server.fastmcp import FastMCP, Image

# --- configuration ---
HOST = os.environ.get("COCO_DEBUG_HOST", "http://coco3.local").rstrip("/")
TIMEOUT = float(os.environ.get("COCO_DEBUG_TIMEOUT", "5.0"))

mcp = FastMCP("coco3-debug")


# --- HTTP helpers ---

def _get(path: str, **params):
    r = httpx.get(f"{HOST}{path}", params=params or None, timeout=TIMEOUT)
    r.raise_for_status()
    return r


def _post(path: str, **data):
    # The device parses application/x-www-form-urlencoded parameters.
    r = httpx.post(f"{HOST}{path}", data=data or None, timeout=TIMEOUT)
    r.raise_for_status()
    return r


# --- tools ---

@mcp.tool()
def status() -> dict:
    """Get emulator status: machine type, paused flag, firmware and API version."""
    return _get("/api/status").json()


@mcp.tool()
def read_registers() -> dict:
    """Read the CPU registers (A, B, D, X, Y, U, S, PC, DP, CC), decoded
    condition-code flags (E F H I N Z V C), and the cycle count. `cpu` is
    "6809" or "6309"; on a 6309 the reply also has E, F, W, V, MD and `native`
    (MD bit 0)."""
    return _get("/api/registers").json()


@mcp.tool()
def write_registers(
    pc: int | None = None,
    a: int | None = None,
    b: int | None = None,
    d: int | None = None,
    x: int | None = None,
    y: int | None = None,
    u: int | None = None,
    s: int | None = None,
    dp: int | None = None,
    cc: int | None = None,
    e: int | None = None,
    f: int | None = None,
    w: int | None = None,
    v: int | None = None,
    md: int | None = None,
) -> dict:
    """Set any subset of the CPU registers. Only the values you pass are
    changed. E, F, W, V and MD are the HD6309 registers (MD bit 0 = native
    mode, bit 1 = FIRQ stacks everything). Returns the full register set
    after the write."""
    fields = {k: val for k, val in dict(
        pc=pc, a=a, b=b, d=d, x=x, y=y, u=u, s=s, dp=dp, cc=cc,
        e=e, f=f, w=w, v=v, md=md
    ).items() if val is not None}
    if not fields:
        raise ValueError("provide at least one register to write")
    return _post("/api/registers", **fields).json()


@mcp.tool()
def read_memory(addr: int, length: int = 16, space: str = "cpu") -> dict:
    """Read `length` bytes starting at `addr`.

    space="cpu"  -> MMU-correct view (what the 6809 sees, ROM/I-O mapped in).
    space="phys" -> raw physical RAM (CoCo 3 512KB linear).

    Returns {addr, len, space, data(hex), bytes(list of ints)}.
    """
    j = _get("/api/mem", addr=addr, len=length, space=space).json()
    hexstr = j.get("data", "")
    j["bytes"] = list(bytes.fromhex(hexstr)) if hexstr else []
    return j


@mcp.tool()
def write_memory(addr: int, data_hex: str, space: str = "cpu") -> dict:
    """Write bytes (given as a hex string, e.g. "86ff39") at `addr`.
    space="cpu" (MMU-correct) or "phys" (raw physical RAM)."""
    return _post("/api/mem", addr=addr, data=data_hex.replace(" ", ""), space=space).json()


@mcp.tool()
def pause() -> dict:
    """Pause emulation at the next frame boundary (CPU frozen, server responsive)."""
    return _post("/api/pause").json()


@mcp.tool()
def resume() -> dict:
    """Resume emulation after a pause."""
    return _post("/api/resume").json()


@mcp.tool()
def inject_code(addr: int, data_hex: str, pc: int | None = None,
                resume_after: bool = False) -> dict:
    """Write a blob of 6809 code/data (hex string) into CPU memory at `addr`.
    Optionally set PC to begin executing it, and optionally resume the CPU."""
    args = dict(addr=addr, data=data_hex.replace(" ", ""))
    if pc is not None:
        args["pc"] = pc
    if resume_after:
        args["resume"] = 1
    return _post("/api/inject", **args).json()


@mcp.tool()
def reset() -> dict:
    """Cold-reset the emulated CoCo in place (machine_reset). The device stays
    up and WiFi/server are unaffected."""
    return _post("/api/reset").json()


@mcp.tool()
def get_machine() -> dict:
    """Get the current machine type (3 = CoCo 2, 4 = CoCo 3), CPU ("6809" or
    "6309") and CoCo 3 memory size in KB."""
    return _get("/api/machine").json()


@mcp.tool()
def set_machine(machine_type: int, wait: bool = True, cpu: int | None = None,
                ram_kb: int | None = None) -> dict:
    """Switch machine type (3 = CoCo 2, 4 = CoCo 3) and, optionally, the CPU
    (`cpu` = 6809 or 6309) and the CoCo 3 memory size (`ram_kb` = 128, 512,
    1024 or 2048). Pass the current machine type to change only the others.

    IMPORTANT: this reboots the device — the current connection drops. If
    `wait` is true, the tool polls /api/status until the device comes back
    (requires auto-connect enabled so it rejoins WiFi automatically).
    """
    fields = {"type": machine_type}
    if cpu is not None:
        fields["cpu"] = cpu
    if ram_kb is not None:
        fields["ram"] = ram_kb
    resp = _post("/api/machine", **fields).json()
    if not wait or not resp.get("rebooting"):
        return resp
    # Device reboots; wait for it to rejoin and report the new settings.
    deadline = time.time() + 60
    time.sleep(3)
    while time.time() < deadline:
        try:
            st = _get("/api/status").json()
            if int(st.get("machine_type", -1)) == machine_type and \
               (cpu is None or st.get("cpu") == str(cpu)) and \
               (ram_kb is None or int(st.get("coco3_ram_kb", -1)) == ram_kb):
                return {"rebooted": True, "status": st}
        except Exception:
            pass
        time.sleep(2)
    return {"rebooted": True, "warning": "device did not report new type within timeout",
            "initial_response": resp}


# Disk mounts can read a whole floppy image into PSRAM, so the device allows
# them up to 10 s (h_post_disk).
DISK_TIMEOUT = float(os.environ.get("COCO_DISK_TIMEOUT", "15.0"))


def _disk_drive(drive: int | str) -> str:
    """Normalise a drive: 0-3 (floppy) or "hd0" / "hd1" (hard disk)."""
    d = str(drive).strip().lower()
    if d in ("0", "1", "2", "3", "hd0", "hd1"):
        return d
    raise ValueError('drive must be 0-3 (floppy) or "hd0" / "hd1" (hard disk)')


def _disk_post(**data) -> dict:
    r = httpx.post(f"{HOST}/api/disk", data=data, timeout=DISK_TIMEOUT)
    r.raise_for_status()
    return r.json()


@mcp.tool()
def list_disks() -> dict:
    """List the mounted images. `drives` are the WD1793 floppy drives 0-3
    (path, bytes, read_only, dirty). `hd_enabled` says whether the virtual hard
    disk at $FF80-$FF86 is on (Settings > Cartridges > Hard Disk); `hard_disks`
    are its two drives hd0 / hd1 (path, read_only)."""
    return _get("/api/disk").json()


@mcp.tool()
def mount_disk(drive: int | str, path: str) -> dict:
    """Mount an image from the SD card, as the Disk Manager does (the mount is
    remembered and comes back at boot). `drive` is 0-3 for a floppy (.DSK /
    .VDK) or "hd0" / "hd1" for a hard disk (.VHD). `path` is the full SD path,
    e.g. "/vhd/EOU.VHD". Hard disks are only seen by the CoCo while the hard
    disk is on (see list_disks); RGB-DOS checks for one at cold start, so
    reset after mounting."""
    return _disk_post(op="mount", drive=_disk_drive(drive), path=path)


@mcp.tool()
def eject_disk(drive: int | str) -> dict:
    """Unmount drive 0-3 or "hd0" / "hd1". Floppy writes are flushed to the SD
    card first."""
    return _disk_post(op="eject", drive=_disk_drive(drive))


@mcp.tool()
def flush_disk(drive: int | str) -> dict:
    """Write pending changes to the SD card now: floppy drive 0-3, or "hd0" /
    "hd1" (flushes both hard disks)."""
    return _disk_post(op="flush", drive=_disk_drive(drive))


# USB HID usages the supervisor understands.
OSD_KEYS = {
    "up": 0x52, "down": 0x51, "left": 0x50, "right": 0x4F,
    "enter": 0x28, "esc": 0x29, "backspace": 0x2A, "tab": 0x2B,
    "pgup": 0x4B, "pgdn": 0x4E, "home": 0x4A, "end": 0x4D, "f": 0x09,
}


@mcp.tool()
def osd_keys(keys: list[str], delay_s: float = 0.3) -> dict:
    """Press keys in the supervisor menu (the OSD), in order. "toggle" opens or
    closes it (like F3); the others reach it only while it is open: up, down,
    left, right, enter, esc, backspace, tab, pgup, pgdn, home, end, f (flush in
    the Disk Manager), or a raw HID usage number as a string. They never reach
    the emulated keyboard. Screenshots don't work while the OSD is open (the
    emulation is paused), so close it before taking one."""
    sent = []
    for k in keys:
        name = str(k).strip().lower()
        if name == "toggle":
            _post("/api/key", toggle=1)
        else:
            hid = OSD_KEYS.get(name)
            if hid is None:
                hid = int(name, 0)
            _post("/api/key", hid=hid)
        sent.append(name)
        time.sleep(delay_s)   # one menu step per frame or two
    return {"ok": True, "sent": sent}


@mcp.tool()
def read_nvram() -> dict:
    """Dump all persisted settings in the device's NVS "sv" namespace as JSON
    (machine_type, serial_mode, kbd_layout, joyLevel, joyInv, WiFi creds, ...)."""
    return _get("/api/nvram").json()


# Screenshots are an uncompressed ~295 KB PNG that the device streams slowly
# (~15-20 s), so they need a much longer timeout than the JSON endpoints.
SCREENSHOT_TIMEOUT = float(os.environ.get("COCO_SCREENSHOT_TIMEOUT", "45.0"))


@mcp.tool()
def screenshot() -> Image:
    """Capture the current screen as a PNG. Note: this advances the emulation by
    exactly one frame, then restores the prior pause state."""
    r = httpx.get(f"{HOST}/api/screenshot.png", timeout=SCREENSHOT_TIMEOUT)
    r.raise_for_status()
    return Image(data=r.content, format="png")


if __name__ == "__main__":
    mcp.run()
