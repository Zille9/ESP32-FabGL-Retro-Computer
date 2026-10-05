<h1 align="center">
  <br>
  <b>SMS/GG Emulator for TTGO VGA32</b>
  <br>
</h1>

<p align="center">
  A high-performance Sega Master System (SMS) and Game Gear (GG) emulator designed specifically for the <strong>TTGO VGA32</strong> board.  
  It outputs crisp <strong>320x240 VGA video</strong> at <strong>6-bit color (64 colors)</strong> and accepts input via <strong>PS/2 keyboard</strong>.
</p>

---

## Hardware Requirements

| Component               | Specification                                |
|-------------------------|----------------------------------------------|
| **Board**               | TTGO VGA32 (ESP32-based)                    |
| **Video Output**        | VGA (320x240, 6-bit color, 64 colors)       |
| **Input**               | PS/2 Keyboard                               |
| **Audio**               | DAC (built-in, mono)                        |
| **Storage**             | microSD card (FAT32 formatted)              |

---

## Default Pin Configuration

### VGA Output
| Signal      | GPIO Pin |
|-------------|----------|
| RED Low     | 21       |
| RED High    | 22       |
| GREEN Low   | 18       |
| GREEN High  | 19       |
| BLUE Low    | 4        |
| BLUE High   | 5        |
| HSYNC       | 23       |
| VSYNC       | 15       |

### PS/2 Keyboard Input
| Signal      | GPIO Pin |
|-------------|----------|
| Clock       | 33       |
| Data        | 32       |

### SD Card (SPI)
| Signal      | GPIO Pin |
|-------------|----------|
| CS          | 13       |
| MOSI        | 12       |
| MISO        | 2        |
| SCLK        | 14       |

---

## Keyboard Controls

| SMS/GG Button | Keyboard Key    |
|---------------|-----------------|
| **Button 1**  | `Z` or `z`      |
| **Button 2**  | `X` or `x`      |
| **Start**     | `ENTER`         |
| **Pause**     | `SPACE`         |
| **Up**        | `↑` (Up Arrow)  |
| **Down**      | `↓` (Down Arrow)|
| **Left**      | `←` (Left Arrow)|
| **Right**     | `→` (Right Arrow)|
| **Reset**     | `ESC` (resets the system) |

---

## Supported Systems

| System                 | Support         |
|------------------------|-----------------|
| **Sega Master System** | Full (SMS)      |
| **Game Gear**          | Full (GG)       |

### ROM Compatibility

- **SMS ROMs** - `.sms` extension
- **GG ROMs** - `.gg` extension
- Place ROMs directly in the root of the SD card
- Supports standard cartridge mappers (up to 4MB)

---

## Features

- **Full SMS/GG hardware emulation** (Z80 CPU, VDP, SN76489 PSG)
- **VGA output at 320x240 resolution** with 64 simultaneous colors
- **PS/2 keyboard input** with full button mapping
- **DAC audio output** with native SMS sample rate (15,720 Hz)
- **microSD card support** for loading ROM files
- **Game selection menu** with scrollable list and ROM browser
- **Configurable frameskip** (default: 1 = render every 2nd frame)
- **Realistic PSG audio** with proper volume tables and DC offset filtering
- **Hardware-accurate noise channel** (LFSR feedback)
- **Both SMS and Game Gear palettes** supported

---

## Audio Features

The emulator implements accurate SN76489 PSG audio emulation:

- **4 channels**: 3 tone generators + 1 noise generator
- **Hardware volume table** measured from real SMS
- **DC offset filtering** eliminates hum
- **FIR low-pass filter** reduces aliasing (15,720 Hz sample rate)
- **Mono output** via built-in DAC

---

## Performance & Frameskip

The emulator uses several optimizations to maintain smooth gameplay:

- **CPU runs at 240 MHz** for maximum performance
- **Frameskip configurable** (default: 1 = render 1 of every 2 frames)
- **Timer-based audio interrupt** for precise timing
- **Efficient tile caching system** (512 cached tiles)
- **Optimized VGA DMA output** via I2S
- **Screen centering** for both SMS (192 lines) and GG (144 lines)

### Frame Timing

| System | Lines | VGA Output | Center Offset |
|--------|-------|------------|---------------|
| SMS    | 192   | 240 lines  | +24 lines     |
| GG     | 144   | 240 lines  | +48 lines     |

---

## Required Libraries

This project would not be possible without these amazing libraries:

| Library | Author(s) | Purpose |
|---------|-----------|---------|
| [fabgl](https://github.com/fabGL-VGA/fabGL) | fdivitto | PS/2 keyboard input |
| [VGA_6bit](https://github.com/bitluni) | bitluni, Ricardo Massaro | VGA output via I2S |
| [SD](https://github.com/espressif/arduino-esp32) | Espressif | SD card access |
| [SPI](https://github.com/espressif/arduino-esp32) | Espressif | SPI communication |

### Core Emulation Components

- **Z80 CPU emulator** from the SMS Plus emulator
- **VDP (Video Display Processor)** emulation
- **SN76489 PSG audio** with accurate noise feedback
- **ROM banking/mapper support** for large cartridges

---

## Special Thanks

- **esp_8_bit** - For the original SMS emulator core
- **Charles MacDonald (SMS Plus)** - For the excellent SMS/GG emulation foundation
- **bitluni** - For the original VGA library and inspiration
- **ackerman** - For VGA optimization and DMA improvements
- **Ricardo Massaro** - For the 6-bit VGA implementation
- **fdivitto (fabGL)** - For excellent PS/2 keyboard support
- **SMS Power! community** - For hardware documentation
- **Espressif** - For the powerful ESP32 platform

---

## Getting Started

### Step 1: Prepare SD Card
1. Format a microSD card as **FAT32**
2. Copy your `.sms` and `.gg` ROM files to the root directory

### Step 2: Upload the Code
1. Open `SMS_TTGO_VGA.ino` in Arduino IDE
2. Select **ESP32 Dev Module** as the board
3. Set **Partition Scheme** to "No OAuth (Large APP)"
4. Upload to your TTGO VGA32 board

### Step 3: Play!
1. Connect VGA monitor to TTGO VGA32
2. Connect PS/2 keyboard to the PS/2 port
3. Power on the device
4. Select a game using arrow keys and `ENTER`
5. Play! Press `ESC` at any time to return to the game selection menu

---

## Building from Source

### Arduino IDE Settings
| Setting | Value |
|---------|-------|
| Board | ESP32 Dev Module |
| CPU Frequency | 240 MHz (WiFi/BT) |
| Flash Size | 4MB (32Mb) |
| Partition | No OAuth (Large APP) |
| PSRAM | Disabled |

### Compiler Optimizations

The code uses aggressive GCC optimizations:
- `-O3` (maximum speed optimization)
- `-omit-frame-pointer`
- `-finline-functions`
- Functions placed in IRAM for faster access

### Configuration Options (config.h)

| Option | Description |
|--------|-------------|
| `FRAME_SKIP` | 0 = render every frame, 1 = skip every other frame |
| `SAMPLE_RATE` | 15720 (NTSC SMS) or 15600 (PAL) |
| `DEBUG` | Enable serial debug output |
| `CONFIG_ESP32_WIFI_ENABLED` | Disable WiFi for more CPU |
| `CONFIG_BT_ENABLED` | Disable Bluetooth for more CPU |

---

## Known Limitations

- **No FM sound support** (YM2413 not implemented)
- **No save state functionality**
- **Audio is mono only** (built-in DAC limitation)
- **Limited mapper support** (standard banking only)
- **No light gun support**
- **No real-time clock emulation**

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| No VGA output | Check pin connections, ensure VGA cable is connected |
| Keyboard not working | Check PS/2 connections, try restarting the board |
| "SD card... ERROR" | Check SD card format (must be FAT32), reinsert card |
| No audio | Check speaker connection, verify DAC is enabled |
| Garbage on screen | Check VGA cable, ensure correct pin mapping |
| Game doesn't load | Verify ROM is valid (.sms or .gg extension) |

---

## Performance Tips


1. **Use smaller ROMs** if experiencing slowdowns
2. **Increase frameskip** for demanding games
3. **Use SMS ROMs** (GG has more lines to render, slightly slower)

---

## Technical Specifications

| Component | Specification |
|-----------|---------------|
| **CPU** | Z80 @ 3.58 MHz (emulated) |
| **VDP** | TMS9918-derived @ 10.7 MHz |
| **Resolution** | SMS: 256x192, GG: 160x144 |
| **Colors** | 64 colors (6-bit VGA) |
| **Palette** | 32 colors (15-bit RGB) |
| **Audio** | SN76489 PSG (4 channels) |
| **Sample Rate** | 15,720 Hz (NTSC) |


---

## License

This project is based on the SMS Plus emulator and is released under the **GNU General Public License v3.0 (GPLv3)**.

See the [LICENSE](LICENSE) file for more details.

---

<p align="center">
  <sub>Built with ❤️ for the TTGO VGA32 community</sub>
</p>