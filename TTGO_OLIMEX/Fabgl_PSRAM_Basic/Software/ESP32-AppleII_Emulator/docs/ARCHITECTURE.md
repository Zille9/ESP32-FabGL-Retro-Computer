# Architecture

How the emulator is put together, and — more importantly — the handful of
constraints that are not visible from reading the code. Several of these were
found by profiling or debugging on real hardware; breaking them silently costs
performance or correctness rather than failing the build.

## The shape of it

The emulation core is decoupled from the ESP32/FabGL platform layer.
`Apple2Machine` and its components know nothing about VGA or FabGL directly.
Only `VGA` (`src/VGA/VGA.h`), the `.ino`, `src/Supervisor/`, and the PS/2
keyboard and mouse code in `Apple2Device.cpp` touch `fabgl::` types.

`AppleVideo.cpp` and `AppleFont.cpp` do include `VGA.h` (and so, transitively,
`fabgl.h`) in order to draw — but they use only the `VGA` wrapper, never
`fabgl::` types directly. **Keep it that way: `VGA` is the intended seam.** It is
the single place where the emulator meets real hardware pixels, and porting to
another display backend should mean rewriting that one file.

```
ESP32-VGA_AppleII_Emulator.ino   setup: SD, VGA, PS/2, picks the machine, starts EmulationTask
  └── Apple2Machine                one MachineProfile: Apple ][+ or Apple //e (enhanced)
        ├── CPU                    6502 / 65C02 interpreter
        ├── Memory                 page-table address map: Language Card, IIe MMU, $Cxxx ROM
        └── Apple2Device           soft switches, keyboard, speaker
              ├── Card* slots[8]   slot 6: DiskIICard, slot 7: HardDiskCard, slot 1 or 2: SuperSerialCard
              ├── Joystick         paddles 0/1 and buttons from the PS/2 mouse
              └── AppleVideo       text / lores / hires / double hires -> VGA
  ├── MachineProfile               what distinguishes the models
  ├── RomLoader                    ROM files from /roms on the SD card
  ├── Settings                     NVS: the chosen machine, disks to remount
  ├── VGA                          the only fabgl:: seam (VGA16Controller, 640x200)
  └── Supervisor                   F1 menu
```

### Machines

`MachineProfile` (`src/AppleII/MachineProfile.*`) is a table: CPU type, whether
the IIe MMU and auxiliary memory exist, the ROM files and their sizes, whether a
built-in ROM may stand in, and the slot population. The core is written against
these fields rather than model names, so the Apple IIc and IIc Plus are meant to
arrive as new rows plus the hardware they add (new `Card`s, the IIc's extra
switches), not as a rewrite. The ids are stored in NVS: never renumber them.

At boot `setup()` reads the saved model (`Tools/Settings.h`), checks with
`RomLoader::FirstMissing` that its ROM files are on the card, and falls back to
the ][+ — whose ROMs are built in (`rombios.h`) — if they are not, keeping the
reason as `bootNote` for the supervisor. Switching model in the supervisor saves
the choice and the mounted disk paths and calls `ESP.restart()`, so each model's
memory is laid out on a clean heap; `setup()` remounts the disks once.

### Main loop

`setup()` brings up PSRAM, the SD card (SPI pins 14/2/12/13, CS 13), FabGL's VGA
controller (a 640x200 16-colour framebuffer, shown line-doubled on standard
640x480@60Hz timing) and the PS/2 keyboard, creates the machine and the
supervisor, and starts `EmulationTask` on **core 0** (see the constraints
below). The Arduino `loop()` task then deletes itself.

`RunFrame()` asks `FramePacer` (`src/AppleII/FramePacer.*`, tested on the host
by `tests/host/run-pacer-tests.sh`) what to do each pass. A real Apple II draws
60.05 frames a second of 17,030 CPU cycles (65 cycles x 262 lines). At **1X**,
the default, the loop runs one frame's cycles, draws, and waits out the rest of
the 16,652 us (`vTaskDelay` for whole milliseconds, then a short spin). At
**MAX** (F4, saved in NVS) it runs four frames' worth and never waits. While the
Disk II motor runs it goes flat out at either speed and draws only every 100 ms,
so disk loads are as fast as before. When the emulator falls behind, drawing is
skipped for at most three frames in a row, and a backlog of more than eight
frames is dropped rather than caught up. The F2 counter shows frames actually
drawn; the serial log also prints `speed : N%` against a real Apple II's
1.020484 MHz.

### Components

**`Apple2Machine`** (`src/AppleII/Apple2Machine.*`) owns `cpu`, `mem` and
`device` as plain members and installs the cards its profile has.
`InitMachine()` creates memory for the profile, then the device — which builds
the default font — and only then `LoadRoms()`: system ROM, Disk II PROM and, on
the //e, the character ROM. The order matters: loaded before the device, the
character ROM would be wiped by the default font.

**`CPU`** (`src/AppleII/AppleCpu.*`) is an instruction-level 6502 interpreter;
`AppleCpu65C02.cpp` adds the 65C02 opcodes, dispatched from the default case of
the opcode switch when `cmos` is set. It never touches raw memory — every read
and write goes through `Memory::ReadByte`/`WriteByte`. `tests/host/` builds this
code on the host and runs Klaus Dormann's 6502 and 65C02 functional tests
against it (`tests/host/run-cpu-tests.sh`).

**`Memory`** (`src/AppleII/AppleMem.*`) maps the 64K address space as 256 pages
through the `readPage`/`writePage` tables; a NULL entry sends the access down the
slow path, to the soft switches and peripheral ROM logic in `$C000`-`$CFFF`.
`Remap()` rebuilds the tables from the Language Card flags and, on the //e, the
MMU switches (80STORE, RAMRD, RAMWRT, ALTZP, INTCXROM, SLOTC3ROM, INTC8ROM). The
pages whose access changes which ROM is visible stay on the slow path so the
access is seen, in `CxAccess()`: a card's own `$Cn00` page selects that slot's
`$C800`-`$CFFF` expansion ROM (`expSlot`), the //e's `$C3xx` sets INTC8ROM, and
`$CFFF` releases both. The //e's internal ROM wins over a card's expansion ROM
while INTCXROM or INTC8ROM is on, as it does on real hardware. Writes to ROM land in a
sink page, so the write fast path needs no ROM check.

**`Apple2Device`** (`src/AppleII/Apple2Device.*`) is everything that is not CPU
or raw memory: the soft switches (the //e's `$C000`-`$C01F` block and its status
bits in `IIeSwitch()`), the keyboard, the speaker and the slot cards. `$C090`-
`$C0FF` goes to `slots[n]->Io()`, and `Memory` maps each card's `SlotRom()` at
`$Cn00` and, for a card that has one, its `ExpansionRom()` at `$C800`-`$CFFF`. `DiskIICard` is slot 6: two drives of nibblized images in PSRAM, 35
tracks of `0x1A00` nibbles each, read from SD via `Tools/FileSystem.h`. A `.nib`
is loaded as is. A 140K `.dsk`/`.do`/`.po` is read into the tail of the same
buffer and nibblized in place by `DskImage` (`src/AppleII/DskImage.*`, tested on
the host by `tests/host/run-dsk-tests.sh`) as standard 16-sector 6-and-2 tracks,
volume 254, with the DOS or ProDOS sector interleave picked by the extension.
Each track's output ends before the next track's input begins, so it needs one
4K track of scratch rather than a second disk buffer. A write through the data
latch marks its track dirty; `Flush()` saves the dirty tracks when the motor
stops, when the other drive is selected, and on eject. A `.nib` takes the raw
track back (`FileSystem::WriteAt`, an in-place overwrite). A sector image takes
it only when all 16 sectors still decode (`DskImage::DenibblizeTrackToImage`);
a track that does not (copy protection, a half-formatted track) stays in memory
and is logged. If the card refuses a write the drive turns write-protected, so
DOS reports it. Only the RESET line (power-up reset and Ctrl-Reset) stops the
motors besides `$C088`; `$CFFF` only releases the `$C800` ROMs, as on the real
card.

**`HardDiskCard`** (`src/AppleII/HardDiskCard.*`, `tests/host/run-hdd-tests.sh`)
sits in slot 7, after AppleWin's `Harddisk.cpp`: one ProDOS block device backed
by a `.hdv`, a ProDOS-order `.2mg` (header: format at `$0C`, locked flag bit 31
at `$10`, data offset at `$18`, length at `$1C`) or a `.po` bigger than 140K,
kept open on the SD card and read and written 512 bytes at a time. Its 256-byte
firmware, built for the slot it is in:

| Address | Contents |
|---|---|
| `$Cn01/03/05/07` | ID bytes `$20/$00/$03/$3C`: bootable, a block device, not SmartPort |
| `$Cn08` | boot: READ block 0 of unit `$n0` to `$0800`, `JMP $0801` with X = `$n0`. On a read error or a blank boot block (`$0800` = `$00`), back to the autostart slot scan at `$FABA` (same address in the ][+ and //e ROMs) so the floppy boots, or `JMP $E000` if the scan did not bring us here |
| `$Cn40` | driver: `STA $C0x0` runs the command, then A = error code (register 1), X/Y = block count (registers 2/3), `CMP #1` sets carry on an error |
| `$CnFC-FD` / `$CnFE` / `$CnFF` | block count / status `$07` / driver entry `$40` |

The driver's write to register 0 makes the card read ProDOS's command block in
zero page (`$42` command, `$43` unit, `$44-45` buffer, `$46-47` block) through
`CardBus` (`MemoryBus` in `Apple2Machine`: memory as the CPU sees it) and do the
whole transfer before the next instruction. STATUS, READ, WRITE and FORMAT are
handled; drive 2, another slot, or a block past the end return `$28`/`$27`. With
no image the card has no firmware, so the boot scan passes slot 7 by. The card hides its boot PROM while
no disk is inserted, so the machine boots to BASIC instead of hanging on an
empty drive.

**`SuperSerialCard`** (`src/AppleII/SuperSerialCard.*`) is the second card, in
slot 1 or slot 2 as `Settings` remembers it, installed by
`Apple2Machine::SetSerialSlot()` at boot or from the supervisor while the
machine runs. It answers the registers the card's firmware polls - the two DIP
switch banks, and the 6551's data, status, command and control - and its 2K
firmware (`/roms/ssc.rom`, kept in PSRAM) shows at `$Cn00` and `$C800`-`$CFFF`.
There is no IRQ line, so only polled use works: `PR#n` / `IN#n` and printing.
The serial line itself is the ESP32's USB UART, since VGA, SD, PS/2 and audio
take every other pin, so installing the card silences the firmware's own log
(`Log::Mute()`, `Tools/Log.h`) - otherwise both would write into the same
terminal. Output can also be captured to `/printer/print-NNN.txt`, buffered
512 bytes at a time and flushed after a second of silence, so a listing is not
one SD write per character. Register behaviour is host-tested by
`tests/host/run-ssc-tests.sh`.

The keyboard drains FabGL's event queue every frame and uses each event's own
ASCII value — the character made with the modifiers as they were when the key
went down; translating later, with the modifiers as they are by then, reordered
fast typing. Characters wait in a 16-key type-ahead queue and are latched one at
a time as the program clears the strobe. Set `KEY_TRACE` to 1 at the top of
`Apple2Device.cpp` for a serial trace of every key event.

The joystick is a PS/2 mouse on the second port (`KeyboardPort0_MousePort1`:
keyboard on GPIO 33/32, **mouse on GPIO 26/27**). GPIO 26 used to be driven as
a second speaker pin; the speaker is now GPIO 25 only, and nothing else may
drive 26. `UpdateGamepad()` drains FabGL's mouse deltas every frame into
`Joystick` (`src/AppleII/Joystick.*`, no FabGL types, host-tested by
`tests/host/run-joystick-tests.sh`), which accumulates them into two 0-255
axes. The paddle timers are emulated in CPU time, not wall time: an access to
`$C070`-`$C07D` records `cpu->CurrentTick()`, and `$C064`-`$C067` return bit 7
until `axis * 11` cycles have passed — 11 cycles being one pass of the ROM's
PREAD loop — so `PDL()` counts exactly the axis value. `$C061`/`$C062` are the
left/right mouse buttons OR'd with Left/Right Alt.

**`AppleVideo`** (`src/AppleII/AppleVideo.*`) renders into the framebuffer as
palette indices: a 560x192 picture at (40,4), each 40-column dot two pixels wide,
in a black border. The palette is the 16 lores colours in Apple order, so a lores
nibble is its own index; text is white. It reads screen memory straight from
`mem.ram`/`mem.auxRam`, as the video circuit does, whatever RAMRD says. There is
no intermediate backbuffer: redundant drawing is suppressed by the per-cell
dirty caches `TextCache` (80 wide), `LoResCache` and `HiResCache` (plus
`previousBit` for hires colour fringing), each forced to a full repaint every 30
frames via `!flashCycle` and on any change of video mode.

**`VGA`** (`src/VGA/VGA.h`) wraps the global `AppleVGAController`, a
`fabgl::VGA16Controller` subclass. Rows are 4-bit packed, even x in the high
nibble; `row(y)` returns a whole scanline, and `pair()` writes two identical
pixels as one byte, which is all double-width content needs. `setPalette()`
loads the 16 entries.

**`Supervisor`** (`src/Supervisor/`) is the F1 menu. `Supervisor::Run()` is
called from the frame loop and returns only when the menu is closed, which is
what pauses emulation. A grid of tiles leads to the disk manager (drive buttons
D1, D2 and HD above the SD card browser), the Setup list (machine, keyboard,
serial card, capture, speed, debug log) and the About page; each screen is a
function with its own key loop. It is split in three: `SupervisorLogic.h` is
the hardware-free part (grid and list movement, the serial key decoder, name
fitting) and is tested on the host by `tests/host/run-supervisor-tests.sh`;
`SupervisorUI` holds the theme and the frame, row, popup and icon primitives,
drawn with FabGL's `Canvas` and its fonts at double width; `Supervisor` holds
the screens. It calls `Apple2Device::InvalidateRenderCache()` on close so the
emulator repaints fully.

## Constraints

These are the things to be careful about. Each one is cheap to break by
accident and expensive to diagnose afterwards.

### Emulated memory must stay in internal SRAM

All memory blocks are allocated with `heap_caps_malloc(MALLOC_CAP_INTERNAL)`,
**not** `ps_malloc`. The 6502 core touches them on nearly every emulated cycle,
and PSRAM's SPI latency — plus a 32K cache that cannot hold the working set —
made the interpreter roughly **1.6x slower**.

Plain `malloc()` is not sufficient either: the Arduino core builds with
`CONFIG_SPIRAM_USE_MALLOC`, so large allocations can still land in PSRAM.
`Memory::Create()` allocates hottest-first, logs where each block ended up and
warns on a PSRAM fallback — watch that output if emulation speed suddenly drops.

The //e needs about 146K of internal RAM (main and auxiliary 64K each, a 16K
ROM, the page tables) and leaves only ~22K free, with a 14K largest block. That
is why `EmulationTask`'s stack is 8K, and why the `.ino` falls back to running
the emulator from `loop()` if the task cannot be created. The boot log prints
`[mem] internal free ..., largest block ...` just before creating it.

### The framebuffer must stay in internal SRAM too

`VGA16Controller`'s interrupt handler converts the framebuffer to signal levels
line by line. Some FabGL installs are patched to put the framebuffer in PSRAM
(this project's build machine has one); there the ISR streams it over the same
bus the flash-resident interpreter uses, and emulation lost ~20%.
`AppleVGAController` overrides `allocateViewPort()` to use internal RAM
whichever FabGL is installed. The framebuffer being internal also keeps the ISR
safe while flash is written (NVS saves), when PSRAM and flash are unreachable.

### Emulation runs off the video interrupt's core

The VGA16 ISR is pinned to core 1 (`FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE`),
where the Arduino loop runs. Emulating there cost a third of the speed, so the
emulator runs in its own task on core 0, which has nothing else to do (no WiFi or
BT). The task never yields, so core 0's idle-task watchdog is disabled.

### Remap after every banking change

`Memory`'s page tables are only as current as the last `Remap()` or
`RemapLanguageCard()`. Anything that changes a Language Card or MMU flag,
PAGE2/HIRES while 80STORE is on, or what a card's `SlotRom()` returns (a disk
mounted or ejected) must remap, or the CPU keeps seeing the old mapping.

### Keep emulated time off the per-instruction path

`CPU::tick` is brought up to date once per `Run()` call; `CurrentTick()` adds the
cycles the call in progress has used through a pointer to its budget, for
readers like the //e's VBL flag at `$C019`. Updating `tick` after every
instruction cost 3-6%.

### Single-buffering is load-bearing

The dirty caches work only because the VGA framebuffer *persists between
frames*. An unchanged cell is simply not redrawn, so whatever is already in the
framebuffer must still be there on the next frame.

Enabling FabGL double buffering would silently break this: cached cells would be
stale in whichever buffer was not written, showing up as flickering leftover
glyphs. `vga->show()` is deliberately a no-op.

### No periodic repaint

The renderer used to repaint every cell every 30 passes. The caches already
notice every change (the text key includes the flash phase), so that only cost
time. What it was hiding was `AppleVideo::Reset()` marking lores and hires cells
as drawn with value 0; `Reset()` now leaves every cell dirty. Hires cells are
drawn through `HiresRender`'s 8 KB lookup table (every byte, both column
parities, both left neighbours), checked against the old per-dot loop by
`tests/host/run-hires-tests.sh`.

### Overlays must invalidate the cells they cover

The F2 FPS overlay (`fpsOverlay`/`fpsValue`, drawn by
`AppleVideo::RenderFpsOverlay()` in the top-right nine text cells, `999FPS 1X`) is host-side
UI, not emulated state — it is initialised in the `Apple2Device` constructor
rather than `Reset()`, and the frame loop feeds it the per-second frame count.

Because it paints over cells the caches consider clean, `AppleVideo::Render()`
calls `InvalidateFpsOverlayRegion()` for that corner on every overlaid frame
(40- and 80-column cells alike), and the F2 handler calls it again on toggle.
**Any other overlay drawn on top of the emulated screen needs the same
treatment**, or it will leave debris behind when it moves or disappears.

### The supervisor repaints only what changed

The supervisor draws into the live framebuffer, so clearing and repainting a
whole screen on every keypress is visible as flicker. A move repaints the two
tiles or rows the cursor left and reached; a list repaints fully only when it
scrolls, and a screen only after a popup closed over it.

### The palette has one owner at a time

The framebuffer holds 4-bit indices into one 16-entry palette, shared by the
emulator and the supervisor, and the Apple picture stays visible around the
menu box. On opening, the supervisor loads `menuPalette` (`SupervisorUI.cpp`):
the Apple's lores colours with nine entries changed, among them the six a
hires picture uses, which become the Apple logo's stripe colours. The paused
picture therefore shifts slightly while the menu is up. On close,
`InvalidateRenderCache()` makes `AppleVideo` load its own palette again, clear
the border and repaint every cell. The frame loop renders one Apple frame
before `Run()`, so there is a picture to draw over even when the menu opens
before the first emulated frame.

### Canvas colours need the lookup rebuilt, and it is crude

FabGL resolves a colour drawn through `fabgl::Canvas` to a palette index with a
table it builds once, from its own default palette; `setPaletteItem()` does not
update it. `AppleVGAController::refreshColorLookup()` (`VGA.h`) rebuilds it and
the supervisor calls it after loading its palette. The rebuild takes the first
entry of the same hue whose saturation and brightness are within a wide margin,
so with black at index 0 a light grey resolves to black, and white to the
first grey. `menuPalette` is arranged so that each theme colour in
`SupervisorUI.h` resolves to an entry of its own colour, and only those colours
may be drawn. With the debug setting on, opening the menu prints
`menu: palette ok` or names the colour that resolves wrongly, and
`menu: pixels bg=2 stripes=12 13 9 1 3 6` read back from the framebuffer.

### The supervisor can be driven from the serial port

With the Debug log setting on (NVS key `debug`, or a build with
`-DSUP_DEBUG_DEFAULT=1`) every selection prints a line such as
`menu: Setup > Speed 1X`, and VT key sequences on the USB serial port act as
keys: arrows, Enter, Esc, Tab, PgUp/PgDn, Home/End, and F1 (`ESC O P`) to open
or close the menu. Both stop while the Super Serial Card is installed, since
the port is then the card's line.

### No partitions.csv in the sketch folder

The ESP32 Arduino core uses a `partitions.csv` found in the sketch folder
instead of the `PartitionScheme` in the FQBN — and copies it into the build
cache, where it outlives the file. The repository used to carry one from the
upstream code: a 928K app and no partition named `nvs`, so every settings save
failed while arduino-cli still reported the 3 MB `huge_app` limit.
`tools/build-firmware.sh` compiles with `--clean` so a stale table cannot survive
into a release.

## Emulation speed

At 1X the machine runs at 100% of a real Apple II and draws 60 frames a second.
At MAX the idle ][+ runs at about 266% (40 passes of four frames a second), and
*Karateka* loads in about 19 seconds. Measure with a `PERF_TRACE` build (see
[BUILD_AND_RELEASE.md](BUILD_AND_RELEASE.md#measuring-speed)). Four things
dominate performance, all found by measuring on hardware rather than by reading
the code:

**The CPU's cycle budget is 32-bit** (`Cycles` in `AppleCpu.h`). Every bus
access decrements it; as a `long long` that was 64-bit arithmetic on a 32-bit
CPU, and switching it gave 18% more speed and a 14% faster Karateka load. The
running total since reset (`tick`) stays 64-bit.

**Memory placement and core assignment** — see the constraints above.

**`CPU::fastDiskDelay`** (`src/AppleII/AppleCpu.cpp`, on by default) skips DOS
3.3 RWTS's drive spin-up wait at `$BD9E`. That loop is a pure nested software
timer with no I/O, present so a physical drive can reach speed. DOS re-arms it
on essentially every RWTS call, and it was consuming **~70% of all emulated
cycles** — Karateka took 103s to load, of which ~112 seconds of *emulated* time
was this wait. Skipping it brought the load down to 35s.

The skip verifies all 13 opcode bytes of the loop before firing and writes back
the `$46`/`$47` the loop would have left, so machine state is identical. A
non-matching ROM or a custom RWTS just executes normally. This only helps DOS
3.3-derived loaders.

**Binary layout** — two builds that differ by a line of code can differ by a few
percent in speed, from how the code and data happen to fall in the flash and
PSRAM caches. Don't chase differences under ~5% between builds; measure again
after a real change.

> When investigating a slowdown, measure where emulated *cycles* go before
> optimising the host. The first two host-side fixes here produced zero
> improvement in load time, because the bottleneck was the emulated machine
> doing 7.4x too much work.

## Odds and ends

`src/AppleII/LodeRunner.h` contains a large embedded byte array sized to match
`DISKSIZE`, but it is not wired into `DiskIICard` or referenced anywhere else.
Treat it as inert, orphaned data rather than an active code path, unless you are
specifically re-working floppy loading.

## Building

The build targets a plain ESP32 (TTGO VGA32-class board) via the FQBN
`esp32:esp32:esp32:PSRAM=enabled,PartitionScheme=huge_app`; adjust the FQBN
board options for other ESP32 variants. Entry point is
`ESP32-VGA_AppleII_Emulator.ino`, with sources under `src/` per the Arduino
sketch layout. Requires arduino-cli core `esp32:esp32` 2.0.x (**not** 3.x) and
the FabGL 1.0.9 user library.

See [BUILD_AND_RELEASE.md](BUILD_AND_RELEASE.md) for the full build, flash and
release procedure.
