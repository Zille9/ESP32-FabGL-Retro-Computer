# Speech/Sound Cartridge

The Tandy Speech/Sound Cartridge (26-3144) speaks English text and plays
three-voice sound. It is a small computer of its own: a TMS7040
microcontroller (the "PIC7040") runs the text-to-speech firmware and drives an
SP0256-AL2 speech chip and an AY-3-8913 sound generator, with 2 KB of RAM. The
CoCo sees only two I/O ports.

The emulation is a port of VCC's `ssc/` module (commit db8f7d7, by 780TECH),
itself ported from MAME's `coco_ssc.cpp`. All four parts are emulated: there is
no shortcut, because the text-to-speech rules live in the firmware.

**Turning it on:** F3 → Setup → Cartridges → Speech/Sound (ON / OFF). It
restarts the emulator. It works on the CoCo 2 and the CoCo 3, alongside Disk
BASIC (it has no CoCo-side ROM, so it is decoded next to the floppy controller
like a built-in Multi-Pak) and alongside Orchestra-90 (`$FF7A/B` do not
overlap).

**ROMs:** two dumps in `/roms/` on the SD card, with the MAME file names:

| File | Size | Chip |
|---|---|---|
| `pic-7040-510.bin` | 4096 bytes | TMS7040 firmware |
| `sp0256-al2.bin` | 2048 bytes | SP0256-AL2 allophone ROM |

They are copyrighted and are not distributed with the emulator. If either is
missing or the wrong size, the Settings popup names it and nothing changes;
at boot the cartridge stays off for that boot (the setting is kept).

## Source files

| File | Role |
|---|---|
| `src/core/ssc/ssc.h/.cpp` | Glue: ports, 2 KB RAM, BUSY, sound-activity circuit, mix; core-0 task and rings |
| `src/core/ssc/tms7000.h/.cpp` | TMS7040 CPU core |
| `src/core/ssc/sp0256.h/.cpp` | SP0256-AL2 LPC filter and microsequencer |
| `src/core/ssc/ay8913.h/.cpp` | AY-3-8913 tone/noise/envelope generator |
| `src/core/ssc/ssc_bus.h` | Chip-to-glue wiring (direct calls instead of `std::function`) |
| `src/core/sound.cpp` | `sound_set_ssc()` — mixes the cartridge into the audio output |
| `src/core/machine.cpp` | `$FF7D`/`$FF7E` decode, ROM load, reset, per-scanline call |
| `src/supervisor/sv_menu.cpp` | Cartridges row and confirm popup (`CART_TOGGLES`) |
| `tools/ssc_test/` | Host tests (see below) |

The chip cores keep their BSD-3-Clause notices (MAME: hap, Tim Lindner, Joseph
Zbiciak) under the project header; BSD-3 is GPL-3 compatible.

## Hardware model

| Address | Access | Function |
|---|---|---|
| `$FF7D` | write | Bit 0: reset line. 1 resets the SP0256; a 1 → 0 edge resets the TMS7040 and the AY |
| `$FF7D` | read | `$FF` |
| `$FF7E` | write | Command byte (a text character, or a control code) |
| `$FF7E` | read | Status |

Status bits (`$FF7E` read; bits 0-4 read 1):

| Bit | Set means |
|---|---|
| 7 (`$80`) | Ready: the last byte has been taken (BUSY released) |
| 6 (`$40`) | SP0256 on standby (not speaking) |
| 5 (`$20`) | No PSG sound activity |

Idle reads `$FF` (255); while speaking, bit 6 clears (191). A program waits
for bit 7 before each byte and ends a sentence with a carriage return:

```
10 Y=&HFF7E:A$="HELLO I AM A COLOR COMPUTER"
20 FOR I=1 TO LEN(A$)
30 IF (PEEK(Y) AND 128)=0 THEN 30
40 POKE Y,ASC(MID$(A$,I,1)):NEXT
50 IF (PEEK(Y) AND 128)=0 THEN 50
60 POKE Y,13
```

Inside, as in MAME: TMS7040 port A is the host command byte (a write raises
INT3), port B is RAM A0-A7, port C is the bus control (RAM A8-A10/R-W/CS, AY
BC1/BDIR/CS, SP0256 ALD strobe, BUSY back to the host) and port D the data bus.
The SP0256's data-request line drives INT1.

## Clocks

Everything advances once per CoCo scanline (15 720 Hz). The clock ratios are
exact integer fractions (no `double`; the ESP32 FPU is single precision):

| Chip | Clock | Per scanline |
|---|---|---|
| TMS7040 | 1.789772 MHz, /2 internal | 56.93 cycles |
| AY-3-8913 | 1.789772 MHz, /8 internal step | 14.23 steps |
| SP0256 | 3.12 MHz, /312 | 0.636 samples (10 kHz) |

An instruction that runs past the end of a scanline takes its extra cycles
from the next one (VCC drops them, so its TMS7040 runs a little fast).

## Where it runs: core 0

On core 1, next to the 6809, the three chips cost about 2.6 ms per frame even
after optimisation, and the CoCo 3 fell from 60 to ~49 FPS in `WIDTH 80` and
`HSCREEN 2`. They now run on core 0, which otherwise carries WiFi, lwIP, the
debug server and the DriveWire client.

```
 core 1 (emulator loop)                         core 0 (task "ssc", priority 3)
 ---------------------                          -------------------------------
 $FF7D/$FF7E write --> command ring (64) ---->  apply writes (INT3, reset, ...)
 READY drops at once                            run 8 scanlines of the chips
                                                 ... until 48 samples queued
 every scanline: play 1 sample <-- sample ring (256) <--
 every 16 scanlines: wake the task -----------> (or a 2 ms timeout)
 $FF7E read <-- released count, standby/activity bits published by core 0
```

- **BUSY** is kept as sequence numbers: core 1 counts the bytes the CoCo wrote;
  core 0 publishes how many had been taken at the last BUSY release. READY is
  "all written bytes released", so it drops the moment the CoCo writes, as on
  the cartridge.
- **Latency:** the cartridge runs up to 48 scanlines (~3 ms) ahead, so it sees
  a write up to ~3 ms later than the real one would. Speech is not sensitive
  to this.
- **Priority 3:** above the debug server and DriveWire client (1), far below
  WiFi and lwIP. The task blocks between rounds, so the core-0 idle task and
  its watchdog run.
- **Underruns:** if core 0 has no sample ready, core 1 repeats the last one
  and counts it. About 2-3 per second at the prompt, ~30 per second while
  speaking with the debug API polled, and ~1000 during a screenshot (core 0
  is busy encoding the PNG): about 64 ms of held sound.
- **Memory:** nothing is allocated until the cartridge is turned on. Then the
  TMS7040 registers (~200 bytes) and the 2 KB task stack are in internal RAM,
  and the TMS7040 ROM, the SP0256 (~10 KB) and the 2 KB static RAM in PSRAM:
  about 4 KB of internal RAM in all.

Measured on the board (CoCo 3, 6809, speaking continuously): 60 FPS at the
BASIC prompt, in `WIDTH 80` and in `HSCREEN 2`; CoCo 2 at 60 FPS. With the
real ROMs the CoCo 3 and CoCo 2 both speak the test sentence (28 bytes, 20
allophones).

The work done on core 1 before moving it (kept, it helps on core 0 too): the
TMS7040 opcode handlers and register-file/ROM access are inlined into the
dispatch switch (~220 → ~130 ESP32 cycles per instruction), and the AY
generators jump by the step count instead of stepping.

## Audio path

```
AY sample x 2.0 + SP0256 sample x 1.75  ->  clamp to -127..127   (VCC's gains)
                                             |
       sound_set_ssc(level):  ssc_out = 64 + level / 2
                                             |
sound_update():  out = CoCo sound + Orchestra-90 + ssc_out    (saturates at 255)
```

- The CoCo output is unipolar, the cartridge's is bipolar: it is added at half
  scale on a fixed bias of 64, which is applied only while the cartridge is on.
- Mono, one DAC pin, like everything else on this board.
- One sample per scanline: speech (10 kHz) fits; high PSG tones alias, as
  Orchestra-90 samples do.

## Debug API

`/api/status` has an `ssc` block: `enabled`, and while on `busy`, `commands`
(bytes the CoCo wrote), `allophones` (SP0256 allophones 5-63 strobed; the
firmware strobes pauses 0-4 all the time), `ay_writes`, `underruns`, `queued`
(samples ready) and `stack_free` (the core-0 task's lowest unused stack).

## Host tests

`tools/ssc_test/run.sh` builds the ported files with g++ on the Pi:

- **chips_test** — VCC's smoke tests for each chip, plus random TMS7000
  programs run side by side with VCC's original core (400 000 slices; PC,
  register file and cycle counts must match).
- **glue_test** — a synthetic PIC firmware (spin, then on INT3: AY register,
  allophone, BUSY release) and a random SP0256 ROM, run side by side with
  VCC's `ssc_core.cpp` one scanline at a time: BUSY timing, AY writes,
  allophones, the sound-activity bit and every output sample must match.
  Then the firmware's batching, a random-firmware no-hang run, and the core-0
  build on a thread under ThreadSanitizer.
- **Real ROMs** — with `pic-7040-510.bin` and `sp0256-al2.bin` in `$SSC_ROMS`
  (default `/roms`), sends `HELLO` + CR, writes `ssc_hello.wav` (15 720 Hz)
  and compares the allophones with VCC.

`VCC_SSC` points at VCC's `ssc/` directory (default: the copy on this Pi);
without it the comparisons are skipped.

## Limitations

- Mono, and the volume balance against the CoCo's own sound is VCC's gains
  on a guessed bias; it may need tuning by ear.
- Sound holds briefly when core 0 is busy (screenshots, heavy WiFi traffic).
- No hot-plug: turning it on or off restarts the emulator.
- Not tested yet: SSC sound programs from disk (PSG music), and the
  Settings popup with the ROMs removed.
