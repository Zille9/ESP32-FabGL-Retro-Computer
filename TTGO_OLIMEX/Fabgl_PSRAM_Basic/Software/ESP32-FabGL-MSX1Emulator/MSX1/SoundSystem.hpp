#pragma once

#include <Arduino.h>
#include "fabgl.h"

// --- 1. Constants and Settings ---
// Buffer size used for both PSG and SCC sample buffers
static constexpr int BUFFER_SIZE = 256;

// SCC Volume conversion table
const uint8_t SCC_VOL_TABLE[16] = {
    0, 2, 4, 7, 10, 15, 21, 26, 33, 39, 47, 54, 62, 68, 73, 75
};

// --- 2. Shared Global Variables ---
// Using 'inline' to allow inclusion in multiple files without linker errors
inline volatile uint8_t psg_regs[16];
inline uint8_t old_psg_regs[16] = { 0xFF };
inline uint8_t psg_address = 0;
inline int8_t scc_waveform[5][32] = { 0 };
inline uint16_t scc_f[6] = { 0 };
inline uint8_t scc_v[6] = { 0 };
inline uint8_t scc_enable = 0;
inline bool scc_active = false;
inline bool psg2scc = false;

// --- 3. Preset Waveform Data ---
inline uint8_t Deep_Cloud_Noise[32] = { 0x80, 0xA5, 0xC8, 0xD0, 0xCE, 0xEF, 0xFF, 0xDF, 0xC0, 0xE0, 0xD5, 0xA0, 0x95, 0xB0, 0xC0, 0xA0, 0x7F, 0x5A, 0x30, 0x15, 0x05, 0x20, 0x40, 0x10, 0x00, 0x15, 0x30, 0x50, 0x70, 0x60, 0x40, 0x65 };
inline uint8_t Alien_Chirp[32] = { 0x80, 0x9A, 0xAD, 0xB9, 0xBE, 0xBF, 0xA0, 0xC0, 0x80, 0xB0, 0x90, 0xA0, 0xC0, 0xE0, 0xFF, 0xA0, 0x60, 0x20, 0x00, 0x10, 0x00, 0x30, 0x50, 0x7F, 0x40, 0x20, 0x10, 0x00, 0x10, 0x20, 0x40, 0x60 };
inline uint8_t Dirty_Bass[32] = { 0x80, 0x88, 0x70, 0x98, 0x80, 0x90, 0x78, 0xA0, 0x90, 0xB0, 0xA0, 0xD0, 0xC0, 0xF0, 0xEF, 0xA0, 0x78, 0x60, 0x70, 0x50, 0x60, 0x40, 0x50, 0x30, 0x40, 0x20, 0x30, 0x10, 0x20, 0x00, 0x10, 0x20 };
inline uint8_t Pseudo_Vocal[32] = { 0x80, 0x85, 0x90, 0xA0, 0xC0, 0xFF, 0xC0, 0xA0, 0x90, 0x85, 0x80, 0x7B, 0x70, 0x60, 0x40, 0x00, 0x40, 0x60, 0x70, 0x7B, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xFF, 0xD0, 0xC0, 0xB0, 0xA0, 0x90 };
inline uint8_t Orchestral_Power[32] = { 0xFF, 0xFF, 0xFF, 0xE0, 0xC0, 0xA0, 0x90, 0x80, 0x88, 0x90, 0xA0, 0xC0, 0xE0, 0xFF, 0xFF, 0xFF, 0x80, 0x40, 0x20, 0x10, 0x00, 0x00, 0x10, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xE0, 0xFF, 0x80 };
inline uint8_t Orchestral_Power_PhaseShift[32] = { 0x80, 0x40, 0x20, 0x10, 0x00, 0x00, 0x10, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xE0, 0xFF, 0x80, 0xFF, 0xFF, 0xFF, 0xE0, 0xC0, 0xA0, 0x90, 0x80, 0x88, 0x90, 0xA0, 0xC0, 0xE0, 0xFF, 0xFF, 0xFF };
inline uint8_t Warm_Body[32] = { 0x80, 0x98, 0xB0, 0xC8, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC8, 0xB0, 0x98, 0x80, 0x68, 0x50, 0x38, 0x20, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x20, 0x38, 0x50, 0x68 };
inline uint8_t Warm_Body_PhaseShift[32] = { 0x80, 0x68, 0x50, 0x38, 0x20, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x20, 0x38, 0x50, 0x68, 0x80, 0x98, 0xB0, 0xC8, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC8, 0xB0, 0x98 };
inline uint8_t Metallic_Lead[32] = { 0x80, 0xC0, 0xFF, 0xC0, 0x80, 0x40, 0x00, 0x40, 0x80, 0xFF, 0x80, 0xFF, 0x80, 0x40, 0x00, 0x40, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0xFF, 0xE0, 0xC0, 0xA0, 0x80, 0x60, 0x40, 0x20 };

// --- 4. Class Implementations ---

// Waveform generator for Konami SCC
class SCCGenerator : public fabgl::WaveformGenerator {
private:
    int16_t sampleBuffer[BUFFER_SIZE];
    volatile int head = 0, tail = 0;
    int lastSample = 0;
    int8_t waveform[32];
    uint32_t phase = 0, phaseInc = 0;
    uint8_t curVol = 0;

public:
    int getSample() override {
        if (head == tail) return lastSample;
        lastSample = sampleBuffer[tail];
        tail = (tail + 1) % BUFFER_SIZE;
        return lastSample;
    }
    void setFrequency(int value) override {}
    
    // Update frequency and waveform parameters
    void updateParams(int8_t* newWave, uint32_t freq, uint8_t vol) {
        if (freq == 0) {
            phaseInc = 0;
        } else {
            // Clock for SCC: 111860 Hz constant
            phaseInc = (uint32_t)(((111860 / (freq + 1)) << 18));
        }
        curVol = vol;
        memcpy(waveform, newWave, 32);
    }

    void fillBuffer() {
        int nextHead = (head + 1) % BUFFER_SIZE;
        while (nextHead != tail) {
            sampleBuffer[head] = calculateNextSample();
            head = nextHead;
            nextHead = (head + 1) % BUFFER_SIZE;
        }
    }

private:
    inline int16_t calculateNextSample() {
        if (curVol == 0 || phaseInc == 0) return 0;
        int8_t sample = waveform[(phase >> 27) & 0x1F];
        phase += phaseInc;
        return (int16_t)((int)sample * curVol >> 8);
    }
};

// Waveform generator for AY-3-8910 (PSG)
class AY38910Generator : public fabgl::WaveformGenerator {
private:
    const uint32_t psgClock = 111860;
    uint32_t tickAccum = 0, tickStep = 0;
    volatile uint32_t tonePeriod[3] = { 1, 1, 1 }, noisePeriod = 1, envPeriod = 1;
    volatile uint8_t envShape = 0, regs[16] = { 0 };
    volatile bool holding = true, cont = false, alt = false, hold = false, att = false;
    volatile uint32_t envCounter = 0, envStep = 0, envVol = 0;
    uint32_t toneCounter[3] = { 0, 0, 0 }, toneOut = 0, noiseCounter = 0, lfsr = 1, noiseOut = 0;
    int16_t volTable[16] = { 0, 5, 10, 15, 20, 26, 32, 40, 48, 58, 68, 78, 90, 102, 114, 127 };
    int16_t sampleBuffer[BUFFER_SIZE];
    volatile int head = 0, tail = 0;
    int lastSample = 0;

public:
    void writeReg(uint8_t reg, uint8_t val) {
        if (reg > 13) return;
        regs[reg] = val;
        switch (reg) {
            case 0: case 1: tonePeriod[0] = (regs[0] | ((regs[1] & 0x0F) << 8)) ? (regs[0] | ((regs[1] & 0x0F) << 8)) : 1; break;
            case 2: case 3: tonePeriod[1] = (regs[2] | ((regs[3] & 0x0F) << 8)) ? (regs[2] | ((regs[3] & 0x0F) << 8)) : 1; break;
            case 4: case 5: tonePeriod[2] = (regs[4] | ((regs[5] & 0x0F) << 8)) ? (regs[4] | ((regs[5] & 0x0F) << 8)) : 1; break;
            case 6: noisePeriod = (regs[6] & 0x1F) ? (regs[6] & 0x1F) : 1; break;
            case 11: case 12: envPeriod = (regs[11] | (regs[12] << 8)) ? (regs[11] | (regs[12] << 8)) : 1; break;
            case 13:
                envShape = val & 0x0F; envStep = 0; envCounter = 0;
                cont = envShape & 0x08; att = envShape & 0x04; alt = envShape & 0x02; hold = envShape & 0x01;
                envVol = att ? 0 : 15; holding = false;
                break;
        }
    }

    void fillBuffer() {
        if (tickStep == 0) {
            int rate = sampleRate();
            if (rate > 0) setSampleRate(rate);
            else return;
        }
        int nextHead = (head + 1) % BUFFER_SIZE;
        while (nextHead != tail) {
            sampleBuffer[head] = calculateNextSample();
            head = nextHead;
            nextHead = (head + 1) % BUFFER_SIZE;
        }
    }

    void setSampleRate(int rate) {
        this->fabgl::WaveformGenerator::setSampleRate(rate);
        tickStep = (rate > 0) ? (uint32_t)(((uint64_t)psgClock << 16) / rate) : (uint32_t)(((uint64_t)psgClock << 16) / 16384);
    }

    int getSample() override {
        if (head == tail) return lastSample;
        lastSample = sampleBuffer[tail];
        tail = (tail + 1) % BUFFER_SIZE;
        return lastSample;
    }
    void setFrequency(int value) override {}

private:
    inline int calculateNextSample() {
        uint32_t curR7 = regs[7], curR8 = regs[8], curR9 = regs[9], curR10 = regs[10];
        uint32_t curTP0 = tonePeriod[0], curTP1 = tonePeriod[1], curTP2 = tonePeriod[2], curNP = noisePeriod;
        
        tickAccum += tickStep;
        uint32_t ticks = tickAccum >> 16; tickAccum &= 0xFFFF;
        
        while (ticks--) {
            toneCounter[0] += 2; while (toneCounter[0] >= curTP0) { toneCounter[0] -= curTP0; toneOut ^= 1; }
            toneCounter[1] += 2; while (toneCounter[1] >= curTP1) { toneCounter[1] -= curTP1; toneOut ^= 2; }
            toneCounter[2] += 2; while (toneCounter[2] >= curTP2) { toneCounter[2] -= curTP2; toneOut ^= 4; }
            
            if (++noiseCounter >= curNP) {
                noiseCounter = 0;
                // LFSR for noise generation
                lfsr = (lfsr >> 1) | (((lfsr & 1) ^ ((lfsr >> 3) & 1)) << 16);
                noiseOut = lfsr & 1;
            }
            if (!holding && ++envCounter >= envPeriod) { envCounter = 0; stepEnvelope(); }
        }

        uint32_t mixMask = (toneOut | curR7) & ((noiseOut ? 0x07 : 0) | (curR7 >> 3));
        int volA = volTable[(curR8 & 0x10) ? envVol : (curR8 & 0x0F)];
        int volB = volTable[(curR9 & 0x10) ? envVol : (curR9 & 0x0F)];
        int volC = volTable[(curR10 & 0x10) ? envVol : (curR10 & 0x0F)];

        // Noise dampening logic for specific MSX sound profiles
        if ((curR7 & 0x09) == 0x01) volA >>= 1; 
        if ((curR7 & 0x12) == 0x02) volB >>= 1; 
        if ((curR7 & 0x24) == 0x04) volC >>= 1;

        int mixedSample = 0;
        if (mixMask & 1) mixedSample += volA; 
        if (mixMask & 2) mixedSample += volB; 
        if (mixMask & 4) mixedSample += volC;

        return (mixedSample >> 1) - 95;
    }

    inline void stepEnvelope() {
        if (envStep < 15) { 
            envStep++; 
            envVol = att ? envStep : (15 - envStep); 
        } else {
            if (!cont) { 
                holding = true; envVol = 0; 
            } else {
                if (hold) { 
                    holding = true; 
                    if (alt) att = !att; 
                    envVol = att ? 15 : 0; 
                } else { 
                    envStep = 0; 
                    if (alt) att = !att; 
                    envVol = att ? 0 : 15; 
                }
            }
        }
    }
};

// --- 5. Global Instances and Utility Functions ---

inline SCCGenerator* scc_ch[5];
inline AY38910Generator psgGen;

// Periodic sound parameter update, typically called from an IRAM-safe interrupt
inline void IRAM_ATTR updateSCCSound() {
    for (int i = 0; i < 5; i++) {
        if (!scc_ch[i]) continue;
        if ((scc_enable & (1 << i)) && scc_f[i] > 10) {
            // Channels 4 and 5 share the same waveform bank on SCC
            int waveIdx = (i < 4) ? i : 3;
            scc_ch[i]->updateParams((int8_t*)scc_waveform[waveIdx], scc_f[i], SCC_VOL_TABLE[scc_v[i]]);
        } else {
            scc_ch[i]->updateParams((int8_t*)scc_waveform[0], 0, 0);
        }
    }
}