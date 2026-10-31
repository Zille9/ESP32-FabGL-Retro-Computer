# Virtual Hard Disk (VHD)

A port of VCC's hard disk pak (`HardDisk/cc3vhd.cpp`, the "EmuDsk" device): two
virtual hard drives at `$FF80-$FF86`, each a raw `.vhd` image file on the SD card
of up to 512 MB. The guest writes a sector number and a buffer address, then a
command, and the whole 256-byte sector moves to or from CoCo memory at once.

It works with the same software as in VCC: **RGB-DOS** (`rgbdos.rom`) for BASIC,
and NitrOS-9 with the **`emudsk`** driver (for example the EOU images for VCC).

## Turning it on

F3 → Setup → **Multi-Pak - Insert Cartridges** → **Hard Disk** opens a picker. The current mode is
marked "current"; picking a different one restarts the emulator.

| Mode | `$FF80-$FF86` | Cartridge ROM at `$C000` |
|---|---|---|
| Off | not decoded | Disk BASIC (or whatever Load Cart chose) |
| **On, boot RS-DOS (NitrOS-9 compatible)** | on | Disk BASIC (`disk11.rom`) |
| **On, boot RGB-DOS** | on | `rgbdos.rom` |

- **RS-DOS** is VCC's Multi-Pak setup: the FD-502 controller (RS-DOS) in one
  slot, the hard disk pak in another. The CoCo boots Disk BASIC, and `DOS` boots
  NitrOS-9 from a floppy whose `emudsk` driver then finds the VHD.
- **RGB-DOS** needs `rgbdos.rom` in `/roms/`, or chosen with Load Cart from any
  folder. Without it, the row says "no ROM" and can't be picked.
- Both On modes turn Orchestra-90 off, since it also needs the cartridge slot.
- The setting is stored in NVS as `vhd_en`, the ROM as `cart_rom`.

Then mount images in the **Disk Manager** (F3 → Disks). With the hard disk on,
it shows six buttons: D0–D3 for the floppies, then **HD0** and **HD1**. ENTER
on a `.vhd` file offers HD0/HD1; any other image offers Drive 0–3. The mounts
are stored as `hd0_path`/`hd1_path` and come back at every boot, before the CPU
starts, so RGB-DOS finds them at cold start. Turning the hard disk Off
removes both from NVS: turning it back on starts with HD0/HD1 empty.

The debug API and the MCP bridge can mount too: `POST /api/disk` with
`op=mount&drive=hd0&path=/vhd/EOU.VHD` (MCP: `mount_disk("hd0", ...)`).

## Booting NitrOS-9

With the EOU images for VCC (a boot floppy such as `63EMU.DSK` and a VHD such
as `63SDC.VHD`):

1. Hard Disk → **On, boot RS-DOS**.
2. Disks: the `.vhd` on **HD0**, the boot `.dsk` on **Drive 0**.
3. Type `DOS`.

Checked on the board on 2026-10-18.

With RGB-DOS, type `DRIVE OFF` before `DOS` (see below): once RGB-DOS has found
a hard disk, every drive number points at the VHD, so `DOS` would read track 34
of the VHD instead of the floppy.

## RGB-DOS

The `rgbdos.rom` used for testing says "HARD DISK BASIC, BASED ON CODE IN
RGBDOS, ORIGINAL (C) 1986, 1989 RGB COMPUTER SYSTEMS, CHANGES FOR EMULATOR BY
R.GAULT SEP 2012". The addresses below are from that ROM.

- **Detection** (`$DBA6`): command 2 (flush), then the status must read 0.
  With no image mounted the status is 4 and RGB-DOS prints "HARD DRIVE(S) NOT
  FOUND". It runs at cold start, so mount first or reset after mounting.
- **Floppy count** (`$014E`): cleared at start-up; set to 4 when no hard disk
  is found. Drive numbers below it go to the floppy controller, the rest to
  the hard disk.
  - `DRIVE OFF` sets it to 4: drives 0–3 are floppies again.
  - `DRIVE ON` sets it back to 0: every drive is on the hard disk.
  - `DRIVE STOP n` selects the hard disk unit (`$FF86`).
- **Layout** (DSKCON `$DA25`): RS-DOS drive n starts at LSN
  `$5A000 + n * 630`. A sector is at `$5A000 + n*630 + track*18 + sector - 1`.
  `$5A000` (368,640 sectors, about 94 MB) is a 24-bit constant at `$D938`. The
  space before it is the NitrOS-9 partition, as in the EOU images.
- `$FF86` is only written if `$DFFF` is non-zero.

`tools/vhd_test/mkrgbvhd.py` builds an image with this layout: drives 4 and 5
formatted, an ASCII `HELLO.BAS` on drive 4 (95 MB).

## Device

| Port | Read | Write |
|---|---|---|
| `$FF80-$FF82` | LSN bits 23-16 / 15-8 / 7-0 | same |
| `$FF83` | status | command |
| `$FF84-$FF85` | buffer address high / low | same |
| `$FF86` | drive select | 0 or 1; other values ignored |

| Command | Action |
|---|---|
| 0 | read the sector at the LSN into the buffer |
| 1 | write the buffer to the sector at the LSN |
| 2 | flush the image to the SD card; clears the LSN and buffer address |

| Status | Meaning |
|---|---|
| `$00` | OK (also after a mount) |
| `$FF` | power-on, no command yet |
| `$FE` | unknown command |
| `$04` | no image in the drive, offset past 512 MB, or short write (also after an unmount) |
| `$05` | write to a read-only image |

- The file offset is `LSN × 256`. There is no header.
- Reads past the end of the file return zeros. Writes past the end extend it.
- The buffer is moved with `machine_read()`/`machine_write()`, a byte at a time
  through the MMU, so it may cross an 8 KB bank boundary (VCC's header warns
  against that; here it works).
- A machine reset flushes and selects drive 0 (VCC's `VhdReset`). Mounts stay.
- An image that can't be opened for writing mounts read-only.

Two VCC quirks are not copied: its zero-fill test (`Temp > BytesMoved`) leaves
one stale byte after a short read, and its range check compares a byte offset
with `>` instead of `>=`.

## Source files

| File | Role |
|---|---|
| `src/core/vhd.h/.cpp` | The device: ports, commands, status, two drives, enable flag |
| `src/core/blockfile.h/.cpp` | Image files read and written in place, a sector at a time (shared with the planned CoCo SDC) |
| `src/core/machine.cpp` | `$FF80-$FF86` decode in the four CoCo 2/3 read/write handlers; `vhd_reset()` and `vhd_tick()` in the dispatchers |
| `src/supervisor/sv_menu.cpp` | Multi-Pak row and the Hard Disk picker (`hd_popup_*`) |
| `src/supervisor/sv_filebrowser.cpp` | HD0/HD1 buttons and mount popups (slots 4 and 5) |
| `src/supervisor/supervisor.cpp` | NVS: `vhd_en`, `hd0_path`/`hd1_path`, auto-mount, flush on supervisor entry and restart |
| `src/net/debug_rpc.cpp`, `debug_server.cpp` | `/api/disk` with `drive=hd0` / `hd1` |

## Image files and performance

Images can be hundreds of MB, so unlike the floppies (`sv_disk.cpp`) nothing is
cached in PSRAM: each command seeks and reads or writes 256 bytes of the file.
All of this runs on core 1, inside the CPU's write to `$FF83`.

- **Arduino's 4 KB stdio buffer.** Arduino-ESP32's `vfs_api.cpp` gives every
  `File` a 4,096-byte buffer, and every `seek()` discards it. Each sector read
  then pulled 4 KB off the card: 9.6 ms read plus 1.1 ms seek per sector at
  the 4 MHz SPI clock. `blockfile.cpp` calls `setBufferSize(512)` right after
  opening, and skips the seek when the file is already at the offset and the
  last access was the same direction. (C requires a seek between a write and
  a read.)
- **Measured** with a 6809 loop issuing back-to-back reads: 0.77 ms per sector
  sequential (1,024 sectors in 1.0 s), about 3.9 ms with backward seeks across
  the image. NitrOS-9 from EOU averaged about 1.5 ms per sector over 6,300
  reads, worst case 61 ms.
- **Internal RAM.** An open FatFS file with the default buffer costs about
  5 KB of internal RAM; with 512 bytes it is about 760 B. Files are also closed
  after 3 s without access (`BLOCKFILE_IDLE_MS`), flushing first, and reopen on
  the next command.
- Writes are also flushed on command 2, unmount, reset, supervisor entry and
  restart.

A program that does nothing but issue sector commands slows the emulation
(about 7 FPS during the sequential test), because each command blocks the
scanline loop for the length of the SD transfer. Real software does work
between sectors.

## Testing

`tools/vhd_test/`:

- `make_pattern_vhd.py` writes `TEST.VHD` (1 MB, each sector starts with its
  own LSN). `port_test.py` mounts nothing itself: mount it on HD0 first, then
  it drives the ports through `/api/inject` and `/api/mem` and checks reads,
  zero-fill past the end, register read-back, status codes, drive select, a
  write and its read-back, and flush.
- `mkrgbvhd.py` builds the RGB-DOS test image (see above).

Set `COCO_DEBUG_HOST` to the board's address (default `http://coco3.local`).

## Limits

- Two drives, images up to 512 MB.
- No image creation on the device: make images on a PC.
- The Cloud-9 real-time clock that VCC's hard disk pak also contains
  (`$FF78/$FF79/$FF7C`) is not emulated.
- The CoCo 2 decode is in place but untested.
