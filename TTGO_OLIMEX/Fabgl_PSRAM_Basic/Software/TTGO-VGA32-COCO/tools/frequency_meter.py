#!/usr/bin/env python3
# ============================================================
#        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
#   (C) 2026 Reinaldo Torres / CoCo Byte Club
#   https://github.com/reyco2000/TTGO-VGA32-COCO
#   Co-developed with Claude Code
#   GPL-3.0-or-later License
# ============================================================
#  File   : tools/frequency_meter.py
#  Module : Audio test tool — measures the pitch of the emulator's
#           sound output from a PC audio input
# ============================================================
"""
frequency_meter.py — measures the pitch of the emulator's sound output.

Built to test how accurate the TTGO-VGA32-COCO emulator's sound is. It runs on
a PC, listens to the board's audio, and prints the dominant frequency
(interpolated between FFT bins), the closest musical note (12-TET, A4 = 440 Hz),
the offset in cents, and a live bar per note.

Setup:
  1. Feed the board's 3.5 mm audio output into the PC: a cable from the jack to
     the PC's line-in or microphone input gives the cleanest reading; a
     microphone in front of a speaker also works.
  2. Find the capture device that carries that signal:
         python frequency_meter.py --list-devices
  3. Run the meter on it:
         python frequency_meter.py --device 8
     With no --device the system default input is used.
  4. Play a steady tone on the CoCo and read the Hz and cents. Hold the tone for
     at least the analysis window (--window-ms, 750 ms by default).

Reference readings (CoCo 3, Disk BASIC). These are the correct values: PLAY
times notes with a delay loop and the 60 Hz interrupt takes time from it, so
the machine is not at concert pitch. See docs/audio-hal.md, "Pitch".
    PLAY "T1L1O2AAAA"   217.84 Hz   A3 -17.1 cents
    PLAY "T1L1O4AAAA"   867.73 Hz   A5 -24.3 cents

Keys while running: Ctrl+C quits.
Needs numpy and sounddevice (pip install numpy sounddevice); no GUI.
"""

import argparse
import sys
import queue
import time

import numpy as np
import sounddevice as sd

# --- note math (equal temperament) -------------------------------------------------

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
A4_MIDI = 69          # MIDI note number of A4
A4_FREQ = 440.0


def freq_to_note(freq):
    """Return (note_name, octave, midi, cents_deviation) for a frequency in Hz."""
    if freq <= 0:
        return "---", 0, 0, 0.0
    midi = A4_MIDI + 12.0 * np.log2(freq / A4_FREQ)
    midi_round = int(round(midi))
    cents = 100.0 * (midi - midi_round)           # midi is in semitones: 100 cents each
    note = NOTE_NAMES[midi_round % 12]
    octave = midi_round // 12 - 1
    return note, octave, midi_round, cents


def note_display(note, octave, cents):
    """Format 'A4' -> 'A4 (+12 cents)  *' where * marks a near-perfect match."""
    marker = "" if abs(cents) > 15 else "  *"
    sign = "+" if cents >= 0 else ""
    return f"{note}{octave}  ({sign}{cents:.1f}c) {marker}"


def midi_to_freq(midi):
    """Standard frequency (Hz) for a MIDI note number, A4 = 440 Hz."""
    return A4_FREQ * 2.0 ** ((midi - A4_MIDI) / 12.0)


def note_energy(midi, freqs, spectrum, harmonics=4, band_semitones=0.5):
    """Total energy in a note by summing its fundamental plus a few harmonics.

    More forgiving than reading a single FFT bin: a weak fundamental still lights
    up if the overtones carry the energy (guitars, voices, phones).
    """
    f0 = midi_to_freq(midi)
    total = 0.0
    for h in range(1, harmonics + 1):
        fh = f0 * h
        if fh >= freqs[-1]:
            break
        lo = fh * 2.0 ** (-band_semitones / 12.0)
        hi = fh * 2.0 ** (band_semitones / 12.0)
        sel = (freqs >= lo) & (freqs <= hi)
        if sel.any():
            total += spectrum[sel].max()
    return total


def refine_peak(freqs, mag, idx):
    """Frequency of the spectral peak at bin idx, interpolated between bins.

    Fits a parabola through the log magnitude of the peak bin and its two
    neighbours. With a Hann window this lands within a few hundredths of a bin,
    instead of the half-bin error of reading the bin centre.
    """
    if idx <= 0 or idx >= len(mag) - 1:
        return float(freqs[idx])
    a, b, c = np.log(mag[idx - 1:idx + 2] + 1e-12)
    denom = a - 2.0 * b + c
    if denom >= 0:                                   # not a real peak
        return float(freqs[idx])
    offset = 0.5 * (a - c) / denom                   # -0.5 .. +0.5 bins
    return float(freqs[idx] + offset * (freqs[1] - freqs[0]))


# --- audio capture -----------------------------------------------------------------

class AudioMeter:
    def __init__(self, device=None, samplerate=None, channels=1, block_seconds=0.1,
                 window_seconds=0.75, smoothing=0.6):
        self.channels = channels
        if samplerate:
            self.srate = int(samplerate)
        else:
            self.srate = int(sd.query_devices(device, "input")["default_samplerate"])
        self.block = max(256, int(block_seconds * self.srate))
        self.smoothing = smoothing                 # temporal averaging for the bar display
        self.queue = queue.Queue(maxsize=64)
        self.smooth_spectrum = None
        self.level_rms = -120.0

        # Analysis window: the FFT runs over the last window_seconds of audio,
        # not one capture block, so bins are ~1.3 Hz apart instead of ~10 Hz.
        n = 1
        while n < window_seconds * self.srate:
            n *= 2
        self.ring = np.zeros(n, dtype=np.float32)
        self.window = self._windowing(n)

        # Float32 sample format is universally supported.
        self.stream = sd.InputStream(
            device=device, channels=channels, samplerate=self.srate,
            dtype="float32", blocksize=self.block, callback=self._fill,
        )
        self.stream.start()

    def _fill(self, indata, frames, time_info, status):
        if status:
            print(f"  [audio status] {status}", file=sys.stderr)
        try:
            self.queue.put_nowait(indata[:, 0].copy())   # keep mono
        except queue.Full:
            pass                                       # drop old blocks if we lag

    def _windowing(self, n):
        try:
            return np.hanning(n)                       # smooth FFT edges
        except Exception:
            return np.ones(n)

    def next_block(self):
        """Return the FFT magnitude spectrum (Hz x mag) for the newest audio block,
        or None if nothing new has arrived."""
        if self.queue.empty():
            return None
        samples = None
        while not self.queue.empty():
            samples = self.queue.get_nowait()          # append every block to the window
            k = min(len(samples), len(self.ring))
            self.ring = np.roll(self.ring, -k)
            self.ring[-k:] = samples[-k:]
        n = len(self.ring)
        spectrum = np.fft.rfft(self.ring * self.window)
        mag = np.abs(spectrum)
        self.level_rms = 20.0 * np.log10(np.sqrt(np.mean(samples ** 2)) + 1e-12)
        freqs = np.fft.rfftfreq(n, 1.0 / self.srate)
        pos = freqs > 0
        freqs, mag = freqs[pos], mag[pos]

        # Smooth the LINEAR magnitude (not dB) so note_energy can sum it.
        if self.smooth_spectrum is None:
            self.smooth_spectrum = mag.copy()
        else:
            self.smooth_spectrum = (self.smoothing * self.smooth_spectrum
                                    + (1 - self.smoothing) * mag)
        return freqs, self.smooth_spectrum

    def close(self):
        self.stream.stop()
        self.stream.close()


# --- display ----------------------------------------------------------------------

def print_devices():
    print("Capture devices:")
    for i, d in enumerate(sd.query_devices()):
        if d["max_input_channels"] > 0:
            print(f"  {i:3d}  {d['name']}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--device", type=int, default=None, help="input device index")
    ap.add_argument("--list-devices", action="store_true", help="show devices and exit")
    ap.add_argument("--rate", type=int, default=None, help="samplerate (default: device)")
    ap.add_argument("--channels", type=int, default=1, help="input channels (default 1)")
    ap.add_argument("--block-ms", type=int, default=100, help="capture block length in ms")
    ap.add_argument("--window-ms", type=int, default=750,
                    help="analysis window in ms; longer gives finer pitch but reacts "
                         "slower (750 ms is about 1.3 Hz per FFT bin)")
    ap.add_argument("--refresh-ms", type=int, default=120, help="display refresh in ms")
    ap.add_argument("--min-note", type=int, default=36, help="lowest MIDI note shown (36 = C2)")
    ap.add_argument("--max-note", type=int, default=84, help="highest MIDI note shown (84 = C6)")
    ap.add_argument("--cols", type=int, default=30, help="bar width in characters")
    ap.add_argument("--gate", type=float, default=-55.0,
                    help="silence gate in dBFS: below this RMS level, bars clamp to "
                         "empty and the display shows SILENCE (raise toward -30 to be "
                         "more aggressive, e.g. --gate -35)")
    args = ap.parse_args()

    if args.list_devices:
        print_devices()
        return

    try:
        meter = AudioMeter(device=args.device, samplerate=args.rate,
                           channels=args.channels, block_seconds=args.block_ms / 1000.0,
                           window_seconds=args.window_ms / 1000.0)
    except Exception as e:
        print(f"Could not open audio input{(' ' + str(args.device)) if args.device is not None else ''}: {e}")
        print("Run with --list-devices to see valid inputs.")
        sys.exit(1)

    # Note ladder: each row is a note with its standard frequency over an
    # amplitude bar, shown in TWO side-by-side columns (lower half left,
    # upper half right), high note on top of each column (piano-style).
    # ANSI cursor-home redraws in place instead of scrolling forever.
    midis_all = list(range(args.min_note, args.max_note + 1))
    n_notes = len(midis_all)
    n_cols = 2 if n_notes > 12 else 1                     # small ranges fit one column
    half = (n_notes + 1) // 2                             # left col = lower half
    n_rows = half                                         # able to hold the upper half too
    refresh = args.refresh_ms / 1000.0
    cols = args.cols
    bar_h = max(1, cols)
    print("\x1b[2J")  # clear once up front

    try:
        while True:
            result = meter.next_block()
            if result is None:
                time.sleep(0.02)
                continue
            freqs, spectrum = result
            start = time.time()

            # dominant frequency overall
            peak_idx = int(np.argmax(spectrum))
            dom_freq = refine_peak(freqs, spectrum, peak_idx)
            note, octave, dom_midi, cents = freq_to_note(dom_freq)
            disp = note_display(note, octave, cents)

            # energy per displayed note (fundamental + harmonics)
            energies = np.array([note_energy(m, freqs, spectrum) for m in midis_all])
            e_db = 20.0 * np.log10(energies + 1e-9)
            ref_db = e_db.max()
            floor_db = ref_db - 40.0                      # show a 40 dB window
            played = int(round(dom_midi))
            peak_db = 20.0 * np.log10(spectrum[peak_idx] + 1e-9)

            # silence gate: once RMS drops below the threshold, freeze the bars
            # empty so the room noise doesn't dance around.
            gated = meter.level_rms < args.gate

            print("\x1b[H")  # cursor home, overwrite last frame
            if gated:
                print(f"🔇  {meter.level_rms:6.1f} dBFS  →  SILENCE (gate {args.gate:.0f} dB)\n")
            else:
                print(f"🎙  {dom_freq:8.2f} Hz  →  {disp}   level {peak_db:5.1f} dB\n")

            # color the loudest note red and the next two yellow
            if gated:
                hot = {}
            else:
                order = np.argsort(-e_db)
                hot = {int(order[0]): "\x1b[31m",                       # 1st red
                       int(order[1]): "\x1b[33m" if n_notes > 1 else None,   # 2nd yellow
                       int(order[2]): "\x1b[33m" if n_notes > 2 else None}   # 3rd yellow
            RST = "\x1b[0m"

            def cell(m, i):
                label = f"{NOTE_NAMES[m % 12]}{m // 12 - 1}"
                if gated:
                    bar = ""
                    frac = 0.0
                    mark = "  "
                else:
                    frac = np.clip((e_db[i] - floor_db) / 40.0, 0.0, 1.0)
                    bar = "#" * int(frac * bar_h)
                    mark = " ◄" if m == played else ("  " if frac > 0.02 else "  ")
                # pad generously so both columns align even though ANSI codes
                # count toward the visible length here; we color only the bar.
                pad_to = bar_h + 22
                s = (f"{label:<3} {midi_to_freq(m):7.1f} Hz |"
                     f"{bar:<{bar_h}}|{mark}".ljust(pad_to))
                color = hot.get(i)
                if color:
                    # wrap just the '#' run; escape codes keep the plain layout
                    s = s.replace(bar, f"{color}{bar}{RST}", 1)
                return s

            if n_cols == 1:
                for i in range(n_notes - 1, -1, -1):
                    print("    " + cell(midis_all[i], i))
            else:
                for row in range(n_rows):                 # 0 = top of both columns
                    i_left = half - 1 - row               # left: high on top
                    i_right = n_notes - 1 - row           # right: high on top, but only its own half
                    if i_right < half:                    # right half ran out -> blank
                        i_right = -1
                    print("    " +
                          (cell(midis_all[i_left], i_left) if i_left >= 0 else "")
                          + "   " +
                          (cell(midis_all[i_right], i_right) if 0 <= i_right < n_notes else ""))

            elapsed = time.time() - start
            time.sleep(max(0.0, refresh - elapsed))
    except KeyboardInterrupt:
        pass
    finally:
        meter.close()
        print("\nstopped.")


if __name__ == "__main__":
    main()