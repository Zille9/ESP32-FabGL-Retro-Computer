// Classic99 for ESP32 - TMS9919 / SN76494 sound chip
// Derived from Classic99 console/sound.cpp (sound_update, setfreq, setvol) and
// Tiemul.cpp (wsndbyte) (C) 2009-2024 Mike Brent aka Tursi aka HarmlessLion.com
// See the original licence there. Not for distribution without the author's permission.
//
// Port notes: runs as a FabGL WaveformGenerator on the sound ISR side. Counters use
// 8.8 fixed point instead of doubles; DC fade, DAC (cassette) and SID mixing are not
// ported. Speech samples (speech.cpp) are added to the output here.

#include <Arduino.h>
#include "fabgl.h"
#include "sound9919.h"
#include "speech.h"

// SMS Power logarithmic volume table (as used by Classic99), 0 = loudest, 15 = off
static const int sms_volume_table[16] = {
    32767, 26028, 20675, 16422, 13045, 10362, 8231, 6538,
    5193, 4125, 3277, 2603, 2067, 1642, 1304, 0
};

#define CHIP_CLOCK 3579545             // divided by 16 to tick the counters
#define NOISE_TAPS 0x0003

// shared with the CPU task: plain ints, written whole
static volatile int nRegister[4];      // tone counts 0-1023, noise control 0-7
static volatile int nVolume[4] = {15, 15, 15, 15};
static volatile bool noiseReset = false;

class TMS9919Generator : public fabgl::WaveformGenerator {
public:
    void setFrequency(int) override {}

    int getSample() override {
        if (m_clocksPerSample == 0) {
            // 8.8 fixed point chip clocks per output sample
            m_clocksPerSample = (int)(((int64_t)CHIP_CLOCK * 256) / 16 / sampleRate());
            m_maxAudibleCount = (CHIP_CLOCK / 32) / (sampleRate() / 2);
        }
        if (noiseReset) {
            noiseReset = false;
            m_lfsr = 0x4000;
        }

        int out = 0;
        for (int idx = 0; idx < 3; idx++) {
            int reg = nRegister[idx];
            int period = (reg ? reg : 0x400) << 8;
            m_counter[idx] -= m_clocksPerSample;
            while (m_counter[idx] <= 0) {
                m_counter[idx] += period;
                m_output[idx] = -m_output[idx];
            }
            // tones above half the sample rate only make aliasing noise: mute them
            if (reg != 0 && reg <= m_maxAudibleCount) continue;
            out += m_output[idx] * m_amp[nVolume[idx]];
        }

        int noiseCtl = nRegister[3];
        int noiseClk;
        switch (noiseCtl & 0x03) {
            case 0: noiseClk = 0x10; break;
            case 1: noiseClk = 0x20; break;
            case 2: noiseClk = 0x40; break;
            default: noiseClk = nRegister[2] ? nRegister[2] : 0x400; break;
        }
        m_counter[3] -= m_clocksPerSample;
        while (m_counter[3] <= 0) {
            m_counter[3] += noiseClk << 8;
            m_noisePos = -m_noisePos;
            if (m_noisePos > 0) {               // shift on the rising edge only
                int in = 0;
                if (noiseCtl & 0x4) {
                    // white noise
                    if (__builtin_parity(m_lfsr & NOISE_TAPS)) in = 0x4000;
                    if (m_lfsr & 0x01) {
                        m_noiseOut = (m_noiseOut == 0) ? 1 : -m_noiseOut;
                    }
                } else {
                    // periodic noise
                    if (m_lfsr & 0x0001) {
                        in = 0x4000;
                        m_noiseOut = 1;
                    } else {
                        m_noiseOut = 0;
                    }
                }
                m_lfsr = (m_lfsr >> 1) | in;
            }
        }
        out += m_noiseOut * m_amp[nVolume[3]];

        // FabGL does not clamp, and the DAC takes -127..127
        out = out * volume() / 127 + speechSample(sampleRate());
        if (out > 127) out = 127; else if (out < -127) out = -127;
        return out;
    }

    TMS9919Generator() {
        for (int i = 0; i < 16; i++) {
            m_amp[i] = sms_volume_table[i] * 31 / 32767;    // 4 channels max ~124
        }
    }

private:
    int m_amp[16];
    int m_counter[4] = {0, 0, 0, 0};
    int m_output[3] = {1, 1, 1};
    int m_noisePos = 1;
    int m_noiseOut = 0;
    int m_lfsr = 0x4000;
    int m_clocksPerSample = 0;
    int m_maxAudibleCount = 0;
};

static fabgl::SoundGenerator *soundGen = nullptr;
static TMS9919Generator chip;
static int latch_byte = 0;
static int oldFreq[3] = {0, 0, 0};

void soundInit() {
    soundGen = new fabgl::SoundGenerator();
    soundGen->attach(&chip);
    chip.enable(true);
    soundGen->play(true);
}

void soundReset() {
    for (int i = 0; i < 4; i++) {
        nVolume[i] = 15;
        nRegister[i] = 0;
    }
    oldFreq[0] = oldFreq[1] = oldFreq[2] = 0;
    latch_byte = 0;
}

void soundMute(bool mute) {
    if (soundGen) soundGen->play(!mute);
}

static void setfreq(int chan, int freq) {
    if (chan == 3) {
        nRegister[3] = freq & 0x07;
        noiseReset = true;                  // writing noise control resets the shift register
    } else {
        nRegister[chan] = freq & 0x3ff;     // counters run out on their own
    }
}

static void setvol(int chan, int vol) {
    nVolume[chan] = vol & 0xf;
}

// from Classic99 wsndbyte
void soundWrite(Byte c) {
    if (c & 0x80) {
        latch_byte = c;
    }

    switch (c & 0xf0) {
        case 0x90:
        case 0xb0:
        case 0xd0:
        case 0xf0:
            setvol((c & 0x60) >> 5, c & 0x0f);
            break;

        case 0xe0:
            setfreq(3, c & 0x07);
            break;

        case 0x80:
        case 0xa0:
        case 0xc0: {
            int nChan = (latch_byte & 0x60) >> 5;
            oldFreq[nChan] = (oldFreq[nChan] & 0xfff0) | (c & 0x0f);
            setfreq(nChan, oldFreq[nChan]);
            break;
        }

        default: {
            // data byte: goes to whatever is latched
            int nChan = (latch_byte & 0x60) >> 5;
            if (latch_byte & 0x10) {
                setvol(nChan, c & 0x0f);
            } else if (nChan == 3) {
                setfreq(3, c & 0x07);
            } else {
                oldFreq[nChan] = (oldFreq[nChan] & 0xf) | ((c & 0x3f) << 4);
                setfreq(nChan, oldFreq[nChan]);
            }
            break;
        }
    }
}
