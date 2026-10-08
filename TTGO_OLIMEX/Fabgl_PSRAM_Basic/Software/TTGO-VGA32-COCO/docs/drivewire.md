# DriveWire: Becker Port and External Client

The emulator speaks **DriveWire** through an emulated **Becker port**
(`$FF41` status, `$FF42` data), the same virtual port XRoar, MAME and VCC use.
It pairs the port with the HDB-DOS DriveWire ROMs. CoCo software, including
`fujinet-lib` and FujiNet's CONFIG, reaches DriveWire only through HDB-DOS's
`DWRead`/`DWWrite` vectors, so the transport is invisible to it.

| Mode | Back end | Needs |
|------|----------|-------|
| **Off** | none; `disk11.rom` and the WD1793 floppy controller | nothing |
| **External** | TCP client to pyDriveWire, DW4 or FujiNet-PC on port 65504 | WiFi and a server |

The mode is chosen in **F3 → Settings → DriveWire** (or `POST /api/bus`) and is
stored in NVS. Changing it saves and restarts.

An earlier build also had an *Internal DW* mode (a disk-only DriveWire server on
the ESP32). It was removed; a board that had it saved boots with the bus Off and
serves its mounted disks through the WD1793 again.

## Two ways a CoCo reads a sector

### Disk BASIC and the WD1793 (Mode: Off)

![Disk BASIC path: DSKCON drives the WD1793 registers; sv_disk.cpp answers each $FF4B read on core 1](images/disk-path-wd1793.svg)

With `disk11.rom` in the cartridge slot, BASIC drives an emulated WD1793 one
register at a time. Every byte of the sector is a CPU read of `$FF4B`, answered
inline on core 1 by `sv_disk.cpp` from the Disk Manager's PSRAM copy of the
`.DSK` image, and an INTRQ-driven NMI ends the transfer. See
[disk-hal.md](disk-hal.md) for the HALT/DRQ/NMI details.

### HDB-DOS over DriveWire (Mode: External)

With `hdbdw3bc3.rom` (CoCo 3) or `hdbdw3bck.rom` (CoCo 2) in the slot there is
no floppy controller. HDB-DOS sends a short request through the Becker port; the
`dw_client` task on core 0 forwards it over TCP to the server, which answers with
the whole sector. The disks are the server's, not the Disk Manager's.

`DIR 1` reading the directory (track 17, sector 3) looks like this on the wire:

| Direction | Bytes | Meaning |
|-----------|-------|---------|
| CoCo → server | `D2 00 00 03 AA` | READEX, DW drive 0, LSN 938 (HDB-DOS addresses `DRIVE n` as LSN `n × 630`) |
| server → CoCo | 256 bytes | Sector data |
| CoCo → server | `hi lo` | 16-bit sum of the bytes it received |
| server → CoCo | `00` | OK. `F3` = checksum mismatch (HDB-DOS retries with REREADEX), `F6` = drive not ready |

## Using External mode

1. Put `hdbdw3bc3.rom` (CoCo 3) and/or `hdbdw3bck.rom` (CoCo 2) in `/roms` on
   the SD card. Optional timeout builds: `hdbdw3bc3t.rom` / `hdbdw3bckt.rom`.
2. Run a DriveWire server on another machine: pyDriveWire, DW4, or FujiNet-PC
   (`./build.sh -p COCO`), listening on port 65504.
3. **F3 → Setup → DriveWire:** turn **External Server** on. A popup asks for
   the Host (IP or `.local` name), Port and HDB-DOS ROM variant; it refuses to
   turn on while the Host is empty or the ROM file is missing from `/roms`,
   and says which file to copy. Host, Port and HDB-DOS ROM then appear as
   rows and stay editable. An orange bar warns when no WiFi network is
   saved. Then
   **Save & Restart.** The boot banner then reads
   `HDB-DOS 1.4 BECKER COCO 3`.
4. On the CoCo: `DIR 0`, `LOAD`, `SAVE`, `LOADM`/`EXEC` as usual, or FujiNet's
   CONFIG when the server is FujiNet-PC.

The Settings → DriveWire screen shows the link state and byte counters. Without
a keyboard, the debug API does the same:

```bash
IP=192.168.8.190
curl http://$IP/api/bus                     # mode, link, byte counters, reply latency
curl -XPOST "http://$IP/api/bus?mode=1&host=192.168.8.221&port=65504"   # reboots
```

## Caveats

- **No timeout in standard HDB-DOS.** `DWRead` spins with interrupts masked
  until a byte arrives. If the link is lost mid-command, the CoCo freezes. The
  `t`-suffixed ROMs (Settings → HDB-DOS ROM: Timeout) give up after 2 s instead.
- **FujiNet-PC read timeout.** FujiNet-PC's Becker-over-IP transport waits only
  500 ms per byte; a WiFi retransmit can desync it. A patch raising it to 5 s is
  in `tools/fujinet-pc-boip-timeout.patch`.

## Files

| File | Role |
|------|------|
| `src/core/becker.*` | Becker port, lock-free TX (1 KB) / RX (4 KB) rings in PSRAM |
| `src/net/dw_bus.*` | Mode, NVS settings, ROM selection, back-end start/stop |
| `src/net/dw_client.*` | External back end: TCP client with auto-reconnect |
| `src/supervisor/sv_fujinet.*` | Settings → DriveWire OSD screen |
| `tools/dw_test_server.py` | Minimal Python 3 DriveWire server for testing External mode |
| `tools/dw_proxy.py` | Logging TCP proxy for byte-level traces |
