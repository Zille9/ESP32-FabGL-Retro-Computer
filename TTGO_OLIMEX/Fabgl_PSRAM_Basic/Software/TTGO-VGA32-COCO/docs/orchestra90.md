# Orchestra-90/CC

The Orchestra-90/CC is a Tandy cartridge with two 8-bit DACs for stereo music
and an 8 KB ROM holding its music editor. The emulation is a port of XRoar's
`orch90.c`.

**Turning it on:** F3 → Setup → Cartridges → Orchestra-90 (ON / OFF). It needs `orch90.rom`
in `/roms/` on the SD card and restarts the emulator. While it is on, the
cartridge ROM takes the place of Disk BASIC, so there are no disk commands.
It works on both the CoCo 2 and the CoCo 3.

## Source files

| File | Role |
|---|---|
| `src/core/orch90.h/.cpp` | The two DAC latches and the enable flag |
| `src/core/sound.cpp` | `sound_set_external()` — mixes the cartridge into the audio output |
| `src/core/machine.cpp` | `$FF7A`/`$FF7B` decode, cartridge ROM load, autostart pulse |
| `src/supervisor/sv_menu.cpp` | Cartridges row and the shared confirm popup (`SV_CART_POPUP`) |
| `src/supervisor/supervisor.cpp` | NVS load/save (`supervisor_load_cart_config()` and friends) |

## Hardware model

| Address | Access | Function |
|---|---|---|
| `$FF7A` | write | Left channel DAC, 8-bit unsigned |
| `$FF7B` | write | Right channel DAC, 8-bit unsigned |

There is no status register, timer or interrupt: the program writes sample
values directly and times them itself.

- **Decode:** `machine_write_coco2()` and `machine_write_coco3()` pass the two
  addresses to `orch90_write()` only while `orch90_enabled()`. With the
  cartridge off the ports do nothing.
- **Levels:** unsigned with no centring. `$00` is silence; an idle `$80` is a
  DC offset that the audio jack's AC coupling removes.
- **Reset:** the latches start at 0 and keep their value across a machine
  reset, as in XRoar and on the real cartridge.

## Audio path

The board has a single DAC output (GPIO25; GPIO26 is the PS/2 mouse clock), so
the two channels are mixed to **mono**:

```
$FF7A --+
        +-- orch90_write() -- sound_set_external(left, right)
$FF7B --+                          |  ext_level = (left + right) / 2
                                   v
          sound_update():  out = CoCo sound (6-bit DAC / single-bit, via mux)
                                 + ext_level        (saturates at 255)
                                   v
                           hal_audio_set_level()
```

- The cartridge level is added whatever the sound mux selects, as in XRoar.
- The level is captured once per scanline, like every other sound source, so
  sample playback faster than about 7.8 kHz aliases.

See `audio-hal.md` for the rest of the audio chain.

## Cartridge ROM and autostart

- **File:** `orch90.rom` (`ROM_ORCH90_FILE` in `config.h`), 8 KB, loaded at
  `$C000` by `load_cart_rom()` in place of `disk11.rom`.
- **Loaded at boot only.** This is why the Setup row asks to confirm and
  restarts.
- **Autostart:** a program cartridge ties its CART line to the Q clock, and
  BASIC starts the ROM when it sees that FIRQ. `cart_rom_autostarts()` treats
  any ROM that does not begin with `DK` (the Disk BASIC signature) as a program
  cartridge, and `cart_autostart_pulse()` then pulses PIA1 CB1 once per frame.
- **Missing ROM:** if the stored ROM name is not on the SD card, the loader
  falls back to `disk11.rom` so the machine still boots.

## Setup row and NVS

The row opens a popup over the list:

- **Turning it on** first checks for `/roms/orch90.rom`. If it is missing the
  popup only says to copy it to the SD card, and nothing changes.
- **Turning it off** clears the stored cartridge, so Disk BASIC is back after
  the restart.
- Both ask No / Yes (default No) before calling `supervisor_save_and_restart()`.

| NVS key (`"sv"`) | Type | Meaning |
|---|---|---|
| `orch90` | Bool | DAC ports enabled (default false) |
| `cart_rom` | String | ROM to load at `$C000` instead of Disk BASIC; the toggle sets it to `orch90.rom` or removes it |

`supervisor_load_cart_config()` runs in `setup()` after `dw_bus_load_config()`
and before `machine_init()`, so the Orchestra-90 ROM also replaces DriveWire's
HDB-DOS ROM.

## Limitations

- Mono only: left and right are averaged.
- No Disk BASIC while the cartridge is on.
- Sample rates above about 7.8 kHz alias.
