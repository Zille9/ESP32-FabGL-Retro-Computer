<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">

<h1>RETRO-GAMER</h1>

<p><strong>Multi-System Retro Emulator for ESP32-FabGL</strong></p>

<p>
    <span class="badge">License: GPL v3</span>
    <span class="badge badge-blue">Platform: ESP32</span>
    <span class="badge badge-orange">Display: VGA</span>
</p>

<hr>

<h2>Table of Contents</h2>
<ul>
    <li><a href="#description">Description</a></li>
    <li><a href="#supported-systems">Supported Systems</a></li>
    <li><a href="#features">Features</a></li>
    <li><a href="#hardware-requirements">Hardware Requirements</a></li>
    <li><a href="#pin-configuration">Pin Configuration</a></li>
    <li><a href="#installation">Installation</a></li>
    <li><a href="#sd-card-structure">SD Card Structure</a></li>
    <li><a href="#usage">Usage</a></li>
    <li><a href="#building-from-source">Building from Source</a></li>
    <li><a href="#development">Development</a></li>
    <li><a href="#troubleshooting">Troubleshooting</a></li>
    <li><a href="#known-limitations">Known Limitations</a></li>
    <li><a href="#acknowledgements">Acknowledgements</a></li>
    <li><a href="#license">License</a></li>
</ul>

<hr>

<h2 id="description">Description</h2>

<p><strong>Retro-Gamer</strong> is a multi-system retro emulator firmware for ESP32-based devices with VGA output via FabGL. It brings retro gaming to life on a big screen with crisp 320x240 resolution and 64 colors (RGB222).</p>

<p>This project is a port and enhancement of <a href="https://github.com/ducalex/retro-go">ducalex's Retro-Go</a>, adapted to run on the FabGL VGA framework. It features a unified launcher for all supported systems, in-game menus, save states, and extensive customization options.</p>

<p>Unlike the original Retro-Go (which was designed for handheld devices with small LCD screens), Retro-Gamer is optimized for <strong>VGA displays</strong> and <strong>PS/2 keyboard input</strong>, making it perfect for DIY retro gaming consoles and arcade cabinets.</p>

<hr>

<h2 id="supported-systems">Supported Systems</h2>

<table>
    <thead>
        <tr><th>System</th><th>Core</th><th>Extensions</th><th>Status</th></tr>
    </thead>
    <tbody>
        <tr><td>NES</td><td>nofrendo</td><td><code>.nes</code> </td><td>✅ Stable</td></tr>
        <tr><td>Game Boy</td><td>gnuboy</td><td><code>.gb</code></td><td>✅ Stable</td></tr>
        <tr><td>Game Boy Color</td><td>gnuboy</td><td><code>.gbc</code></td><td>✅ Stable</td></tr>
        <tr><td>Sega Master System</td><td>smsplus</td><td><code>.sms</code></td><td>✅ Stable</td></tr>
        <tr><td>Sega Game Gear</td><td>smsplus</td><td><code>.gg</code></td><td>✅ Stable</td></tr>
        <tr><td>SG-1000</td><td>smsplus</td><td><code>.sg</code></td><td>✅ Stable</td></tr>
        <tr><td>ColecoVision</td><td>smsplus</td><td><code>.col</code></td><td>✅ Stable</td></tr>
        <tr><td>PC Engine / TurboGrafx</td><td>pce-go</td><td><code>.pce</code></td><td>✅ Stable</td></tr>
        <tr><td>SNES / Super Famicom</td><td>snes9x</td><td><code>.sfc</code> <code>.smc</code></td><td>⚠️ Slow</td></tr>
        <tr><td>Atari Lynx</td><td>handy</td><td><code>.lnx</code></td><td>✅ Stable</td></tr>
        <tr><td>Mega Drive / Genesis</td><td>gwenesis</td><td><code>.md</code> <code>.gen</code> <code>.bin</code> <code>.smd</code></td><td>⚠️ Slow</td></tr>
    </tbody>
</table>



<hr>

<h2 id="features">Features</h2>

<table>
    <thead>
        <tr><th>Feature</th><th>Description</th></tr>
    </thead>
    <tbody>
        <tr><td><strong>11 Emulators</strong></td><td>NES, GB/GBC, SMS, GG, SG-1000, ColecoVision, PCE, SNES, Lynx, Genesis</td></tr>
        <tr><td><strong>In-Game Menu</strong></td><td>Save/Load states (4 slots), turbo, palette, rotate, sound toggle, reset, quit</td></tr>
        <tr><td><strong>Favorites &amp; Recent</strong></td><td>Quick access to your favorite and recently played games</td></tr>
        <tr><td><strong>Save States</strong></td><td>4 slots per game</td></tr>
         <tr><td><strong>WiFi File Manager</strong></td><td>Browser-based file upload/download</td></tr>
        <tr><td><strong>VGA Output</strong></td><td>320x240 resolution with 64 colors (RGB222)</td></tr>
        <tr><td><strong>PS/2 Keyboard</strong></td><td>Full keyboard input with WASD support</td></tr>
        <tr><td><strong>ROM Cache</strong></td><td>Fast boot times with cached ROM list</td></tr>
        <tr><td><strong>Sound Control</strong></td><td>Toggle audio ON/OFF</td></tr>
        <tr><td><strong>Turbo / Fast Forward</strong></td><td>2× speed with continuous audio</td></tr>
    </tbody>
</table>

<hr>

<h2 id="hardware-requirements">Hardware Requirements</h2>

<h3>Minimum Requirements:</h3>
<ul>
    <li><strong>ESP32</strong> with PSRAM (mandatory)</li>
    <li><strong>VGA compatible display</strong> (standard VGA monitor)</li>
    <li><strong>PS/2 Keyboard</strong></li>
    <li><strong>MicroSD Card</strong> (for ROMs and saves)</li>
</ul>

<h3>Supported Boards:</h3>
<ul>
    <li><strong>LilyGO TTGO VGA32</strong></li>
    <li><strong>Olimex ESP32-SBC-FabGL Rev B</strong></li>
</ul>

<hr>

<h2 id="pin-configuration">Pin Configuration</h2>

<h3>LilyGO TTGO VGA32:</h3>

<table>
    <thead>
        <tr><th>Function</th><th>GPIO Pin</th></tr>
    </thead>
    <tbody>
        <tr><td>VGA Red 0</td><td>GPIO 22</td></tr>
        <tr><td>VGA Red 1</td><td>GPIO 21</td></tr>
        <tr><td>VGA Green 0</td><td>GPIO 19</td></tr>
        <tr><td>VGA Green 1</td><td>GPIO 18</td></tr>
        <tr><td>VGA Blue 0</td><td>GPIO 5</td></tr>
        <tr><td>VGA Blue 1</td><td>GPIO 4</td></tr>
        <tr><td>VGA HSYNC</td><td>GPIO 23</td></tr>
        <tr><td>VGA VSYNC</td><td>GPIO 15</td></tr>
        <tr><td>SD CS</td><td>GPIO 13</td></tr>
        <tr><td>Keyboard Clock</td><td>GPIO 33</td></tr>
        <tr><td>Keyboard Data</td><td>GPIO 32</td></tr>
        <tr><td>Audio DAC</td><td>GPIO 25</td></tr>
    </tbody>
</table>

<h3>Olimex ESP32-SBC-FabGL Rev B:</h3>
<p>Same as above, except <strong>SD CS = GPIO 4</strong></p>

<hr>

<h2 id="installation">Installation</h2>

<h3>1. Prepare the SD Card</h3>

<p>Create the following folder structure on your SD card:</p>

<pre>
SD Card
├── retro-go/
│   ├── config/
│   │   ├── favorites.txt
│   │   ├── recent.txt
│   │   └── wifi.json (optional)
│   └── saves/
│       ├── nes/
│       ├── gb/
│       ├── sms/
│       ├── gg/
│       ├── pce/
│       ├── snes/
│       ├── lnx/
│       ├── md/
│       └── misc/
└── roms/
    ├── nes/         (NES .nes, .fc)
    ├── gb/          (Game Boy .gb)
    ├── gbc/         (Game Boy Color .gbc)
    ├── sms/         (Master System .sms)
    ├── gg/          (Game Gear .gg)
    ├── sg1000/      (SG-1000 .sg)
    ├── pce/         (PC Engine .pce)
    ├── snes/        (SNES .sfc, .smc)
    ├── lnx/         (Lynx .lnx)
    ├── col/         (ColecoVision .col)
    └── md/          (Mega Drive/Genesis .md, .gen, .bin, .smd)
</pre>

<h3>2. Flash the Firmware</h3>

<ol>
    <li>Download the latest firmware from the <a href="https://github.com/foky26/Retro-Gamer/releases">Releases</a> page.</li>
    <li>Connect your ESP32 board to your computer via USB.</li>
    <li>Flash using esptool:</li>
</ol>

<pre><code>esptool.py write_flash --flash_size detect 0x0 RetroGamer.bin</code></pre>

<p>Or use the <a href="https://espressif.github.io/esptool-js/">esptool web version</a> for a GUI-based flashing experience.</p>

<h3>3. Add ROMs</h3>

<p>Place your ROM files in the corresponding folders on the SD card (see <a href="#sd-card-structure">SD Card Structure</a> above).</p>

<hr>

<h2 id="sd-card-structure">SD Card Structure</h2>

<h3>ROM Folders</h3>

<table>
    <thead>
        <tr><th>System</th><th>Folder</th><th>Extensions</th></tr>
    </thead>
    <tbody>
        <tr><td>NES</td><td><code>/roms/nes/</code></td><td>.nes</td></tr>
        <tr><td>Game Boy</td><td><code>/roms/gb/</code></td><td>.gb</td></tr>
        <tr><td>Game Boy Color</td><td><code>/roms/gbc/</code></td><td>.gbc</td></tr>
        <tr><td>Master System</td><td><code>/roms/sms/</code></td><td>.sms</td></tr>
        <tr><td>Game Gear</td><td><code>/roms/gg/</code></td><td>.gg</td></tr>
        <tr><td>SG-1000</td><td><code>/roms/sg1000/</code></td><td>.sg</td></tr>
        <tr><td>PC Engine</td><td><code>/roms/pce/</code></td><td>.pce</td></tr>
        <tr><td>SNES</td><td><code>/roms/snes/</code></td><td>.sfc, .smc</td></tr>
        <tr><td>Lynx</td><td><code>/roms/lnx/</code></td><td>.lnx</td></tr>
        <tr><td>ColecoVision</td><td><code>/roms/col/</code></td><td>.col</td></tr>
        <tr><td>Mega Drive / Genesis</td><td><code>/roms/md/</code></td><td>.md, .gen, .bin, .smd</td></tr>
    </tbody>
</table>

<h3>Save States</h3>

<p>Save states are stored in:</p>
<pre><code>/retro-go/saves/&lt;system&gt;/&lt;rom_name&gt;.sav&lt;slot&gt;</code></pre>

<p>Example: <code>/retro-go/saves/nes/Super_Mario_Bros.sav0</code></p>

<hr>

<h2 id="usage">Usage</h2>

<h3>Launcher Controls</h3>

<table>
    <thead>
        <tr><th>Key</th><th>Action</th></tr>
    </thead>
    <tbody>
        <tr><td>↑ / ↓ / W / S</td><td>Navigate ROM list</td></tr>
        <tr><td>Enter</td><td>Play selected ROM</td></tr>
        <tr><td>F</td><td>Toggle favorite</td></tr>
        <tr><td>Tab</td><td>Switch view (All / Favorites / Recent)</td></tr>
        <tr><td>ESC</td><td>Refresh ROM list and update cache</td></tr>
    </tbody>
</table>

<h3>In-Game Controls</h3>

<table>
    <thead>
        <tr><th>Key</th><th>Action</th></tr>
    </thead>
    <tbody>
        <tr><td>↑ / ↓ / ← / → (or WASD)</td><td>D-Pad</td></tr>
        <tr><td>Z / Space</td><td>A Button</td></tr>
        <tr><td>X / Ctrl</td><td>B Button</td></tr>
        <tr><td>Enter</td><td>Start</td></tr>
        <tr><td>Right Shift</td><td>Select</td></tr>
        <tr><td>Tab</td><td>Toggle Turbo Mode</td></tr>
        <tr><td>ESC</td><td>Open In-Game Menu</td></tr>
    </tbody>
</table>

<h3>In-Game Menu</h3>

<p>Press <strong>ESC</strong> during gameplay to open the in-game menu, which provides:</p>

<ul>
    <li><strong>Resume</strong>: Return to game</li>
    <li><strong>Save State [0-3]</strong>: Save current state to selected slot</li>
    <li><strong>Load State [0-3]</strong>: Load state from selected slot</li>
    <li><strong>Turbo</strong>: Toggle fast-forward mode</li>
    <li><strong>Sound</strong>: Toggle audio ON/OFF</li>
    <li><strong>Palette</strong>: Change color palette (NES/GB only)</li>
    <li><strong>Rotate</strong>: Rotate screen (Lynx only)</li>
    <li><strong>Reset Game</strong>: Reset the current game</li>
    <li><strong>Quit to Menu</strong>: Exit to launcher</li>
</ul>

<h3>WiFi File Manager</h3>

<ol>
    <li>Create <code>/retro-go/config/wifi.json</code> on your SD card:</li>
</ol>

<pre><code>{
  "ssid0": "your-wifi-ssid",
  "password0": "your-wifi-password"
}</code></pre>

<p>Up to 4 networks can be configured (ssid0-ssid3).</p>

<ol start="2">
    <li>Connect your ESP32 to the network (WiFi connection happens automatically on boot).</li>
    <li>Find the IP address in the boot screen (top-right corner).</li>
    <li>Open your web browser and navigate to <code>http://&lt;IP_ADDRESS&gt;/</code> to manage files.</li>
</ol>

<h3>Turbo Mode</h3>

<p>Press <strong>Tab</strong> in-game to toggle turbo mode. When enabled:</p>
<ul>
    <li>The game runs at approximately 2× speed</li>
    <li>Audio is continuous (no interruptions)</li>
    <li>Useful for skipping long cutscenes or grinding in RPGs</li>
</ul>

<h3>ROM Cache</h3>

<p>Retro-Gamer uses a ROM cache to speed up boot times. The cache is stored at <code>/retro-go/rom_cache.txt</code> on your SD card.</p>

<p><strong>When to refresh the cache:</strong></p>
<ul>
    <li>After adding new ROMs</li>
    <li>After removing ROMs</li>
    <li>If ROMs are not appearing correctly in the launcher</li>
</ul>

<p><strong>How to refresh the cache:</strong></p>

<ol>
    <li><strong>From the launcher</strong>: Press <strong>ESC</strong> to force a cache refresh and rescan all ROM directories.</li>
    <li><strong>Manual method</strong>: Delete the file <code>/retro-go/rom_cache.txt</code> from your SD card and restart the device. The cache will be rebuilt automatically.</li>
    <li><strong>From the web interface</strong>: Use the WiFi file manager to navigate to <code>/retro-go/</code> and delete <code>rom_cache.txt</code>, then restart the device.</li>
</ol>

<p>The cache is automatically updated when:</p>
<ul>
    <li>The system detects changes in ROM directories (based on file modification timestamps)</li>
    <li>You press ESC in the launcher</li>
</ul>

<hr>

<h2 id="building-from-source">Building from Source</h2>

<h3>Requirements</h3>

<ul>
    <li><a href="https://github.com/espressif/esp-idf">ESP-IDF v4.4 or later</a></li>
    <li><a href="https://github.com/fdivitto/FabGL">FabGL Library</a></li>
</ul>

<h3>Build Instructions</h3>

<ol>
    <li>Clone the repository:</li>
</ol>

<pre><code>git clone https://github.com/foky26/Retro-Gamer.git
cd Retro-Gamer</code></pre>

<ol start="2">
    <li>Set up the ESP-IDF environment:</li>
</ol>

<pre><code>. $HOME/esp/esp-idf/export.sh</code></pre>

<ol start="3">
    <li>Build the firmware:</li>
</ol>

<pre><code>idf.py build</code></pre>

<ol start="4">
    <li>Flash to your device:</li>
</ol>

<pre><code>idf.py flash</code></pre>

<h3>Build Metrics</h3>

<table>
    <thead>
        <tr><th>Metric</th><th>Value</th></tr>
    </thead>
    <tbody>
        <tr><td>Flash Usage</td><td>~1.9 MB / 3.14 MB (61%)</td></tr>
        <tr><td>DRAM Usage</td><td>~120 KB / 327 KB (36%)</td></tr>
        <tr><td>Free DRAM</td><td>~207 KB</td></tr>
        <tr><td>PSRAM</td><td>~4 MB (for ROM data, framebuffers, audio)</td></tr>
    </tbody>
</table>

<hr>

<h2 id="development">Development</h2>

<h3>Debug System</h3>

<p>The project includes a comprehensive debug system controlled by <code>DEBUG_ENABLED</code>:</p>

<pre><code>// In config.h or debug.h
#define DEBUG_ENABLED 1  // Enable debug output
#define DEBUG_ENABLED 0  // Disable debug output</code></pre>

<p>Debug macros available:</p>
<ul>
    <li><code>DBG_ERROR(tag, format, ...)</code> - Critical errors (red)</li>
    <li><code>DBG_WARN(tag, format, ...)</code> - Warnings (yellow)</li>
    <li><code>DBG_INFO(tag, format, ...)</code> - Information (white)</li>
    <li><code>DBG_VERBOSE(tag, format, ...)</code> - Detailed debug (gray)</li>
</ul>

<h3>Architecture</h3>

<pre>
Retro_Gamer.ino          ← Main: hardware init, launcher UI, game loop
├── src/
│   ├── debug.h           ← Debug system (Serial)
│   ├── nes_bridge.c/h    ← NES emulator bridge
│   ├── gb_bridge.c/h     ← Game Boy bridge
│   ├── sms_bridge.c/h    ← SMS/GG/SG-1000/ColecoVision bridge
│   ├── pce_bridge.c/h    ← PC Engine bridge
│   ├── snes_bridge.c/h   ← SNES bridge
│   ├── lynx_bridge.c/h   ← Atari Lynx bridge
│   ├── genesis_bridge.c/h ← Mega Drive bridge
│   ├── wifi_manager.*    ← WiFi + HTTP server
│   └── sound_control.c/h ← Audio control
</pre>

<h3>Adding a New Emulator</h3>

<ol>
    <li>Create a new bridge file in <code>src/&lt;system&gt;_bridge.c</code></li>
    <li>Implement the required bridge functions:
        <ul>
            <li><code>bool &lt;system&gt;_bridge_init(int sample_rate)</code></li>
            <li><code>int &lt;system&gt;_bridge_load_rom(const char *path)</code></li>
            <li><code>void &lt;system&gt;_bridge_run_frame(bool draw)</code></li>
            <li><code>void &lt;system&gt;_bridge_set_input(uint32_t buttons)</code></li>
            <li><code>void &lt;system&gt;_bridge_shutdown(void)</code></li>
            <li><code>uint8_t* &lt;system&gt;_bridge_get_framebuffer(int *width, int *height)</code></li>
            <li><code>int16_t* &lt;system&gt;_bridge_get_audio(int *num_samples)</code></li>
        </ul>
    </li>
    <li>Add the system to <code>emu_type_t</code> in <code>Retro_Gamer.ino</code></li>
    <li>Update <code>getEmuType()</code>, <code>emuName()</code>, and <code>emuShortName()</code></li>
</ol>

<hr>

<h2 id="troubleshooting">Troubleshooting</h2>

<table>
    <thead>
        <tr><th>Problem</th><th>Solution</th></tr>
    </thead>
    <tbody>
        <tr><td><strong>Black/no VGA output</strong></td><td>Check VGA cable connection. Ensure monitor supports 640×480@60Hz.</td></tr>
        <tr><td><strong>"PSRAM not found"</strong></td><td>Your board must have ESP32-WROVER module (with PSRAM). Regular ESP32 not supported.</td></tr>
        <tr><td><strong>SD card not detected</strong></td><td>Format as FAT32 (not exFAT). Check card is ≤32GB.</td></tr>
        <tr><td><strong>Keyboard not working</strong></td><td>Must be PS/2 protocol (not USB-to-PS/2 adapter). Check CLK=33, DAT=32.</td></tr>
        <tr><td><strong>No audio</strong></td><td>Audio output is on GPIO 25 (internal DAC). Connect amplified speaker.</td></tr>
        <tr><td><strong>ROM list empty</strong></td><td>ROMs must be in <code>/roms/{platform}/</code> directories. Check file extensions.</td></tr>
        <tr><td><strong>SNES very slow</strong></td><td>Expected — ESP32 at 240MHz struggles with SNES emulation.</td></tr>
        <tr><td><strong>WiFi won't work</strong></td><td>Check <code>wifi.json</code> format. Ensure SD card is properly inserted.</td></tr>
        <tr><td><strong>Compile error: DRAM overflow</strong></td><td>Set <code>DEBUG_ENABLED 0</code> to reduce memory usage.</td></tr>
    </tbody>
</table>

<hr>

<h2 id="known-limitations">Known Limitations</h2>

<ul>
    <li><strong>Save States</strong>: Work for most systems but may have occasional issues with some games</li>
    <li><strong>SNES Performance</strong>: ~15-25 FPS due to ESP32 CPU limitations</li>
    <li><strong>VGA Color Depth</strong>: 64 colors (R2G2B2) — adequate for retro systems</li>
    <li><strong>Audio</strong>: 8-bit DAC output at 22050Hz — acceptable for retro audio</li>
    <li><strong>No Bluetooth</strong>: Bluetooth gamepad support is not implemented</li>
    <li><strong>Single Player Only</strong>: No multiplayer/netplay support</li>
</ul>

<hr>

<h2 id="acknowledgements">Acknowledgements</h2>

<h3>Original Retro-Go (ducalex)</h3>
<ul>
    <li>The NES/GB/SMS emulators and base library were originally from the "Triforce" fork of the <a href="https://github.com/othercrashoverride/go-play">official Go-Play firmware</a> by crashoverride, Nemo1984, and many others.</li>
    <li>The design of the launcher was originally inspired/copied from <a href="https://github.com/pelle7/odroid-go-emu-launcher">pelle7's go-emu</a>.</li>
    <li>PCE-GO is a fork of <a href="https://github.com/kallisti5/huexpress">HuExpress</a> and <a href="https://github.com/pelle7/odroid-go-pcengine-huexpress/">pelle7's port</a> was used as reference.</li>
    <li>The Lynx emulator is a port of <a href="https://github.com/libretro/libretro-handy">libretro-handy</a>.</li>
    <li>The SNES emulator is a port of <a href="https://github.com/libretro/snes9x2005">Snes9x 2005</a>.</li>
    <li>The Genesis emulator is a port of <a href="https://github.com/bzhxx/gwenesis/">Gwenesis</a> by bzhxx.</li>
    <li>PNG support is provided by <a href="https://github.com/lvandeve/lodepng/">lodepng</a>.</li>
    <li>Special thanks to the <a href="https://forum.odroid.com/viewtopic.php?f=159&t=37599">ODROID-GO</a> community for encouraging the development of retro-go!</li>
</ul>

<h3>FabGL Port (This Project)</h3>
<ul>
    <li><strong>ducalex</strong> for creating and maintaining the original Retro-Go firmware</li>
    <li><strong>UfkuAcik</strong> for the initial port to FabGL VGA framework</li>
    <li><strong>FabGL</strong> library by Francesco Di Vittorio for providing the VGA, PS/2, and audio framework</li>
    <li>The <a href="https://github.com/fdivitto/FabGL">FabGL community</a> for their support and contributions</li>
</ul>

<h3>Additional Thanks</h3>
<ul>
    <li>All the emulator authors whose work made this project possible</li>
    <li>The open-source community for their continuous support and contributions</li>
</ul>

<hr>

<h2 id="license">License</h2>

<p>This project is licensed under the <strong>GNU General Public License v3.0</strong> - see the <a href="COPYING">COPYING</a> file for details.</p>

<h3>Exception:</h3>
<ul>
    <li>handy-go/components/handy (Lynx emulator, zlib license)</li>
</ul>

<hr>

<h2>Contributors</h2>

<p><a href="https://github.com/foky26/Retro-Gamer/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=foky26/Retro-Gamer" alt="Contributors" />
</a></p>

<hr>

<p><strong>Retro-Gamer</strong> - Bringing retro gaming to VGA displays, one pixel at a time! 🎮</p>


</html>