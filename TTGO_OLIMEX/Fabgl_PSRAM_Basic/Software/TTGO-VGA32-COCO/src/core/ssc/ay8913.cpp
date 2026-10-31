/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : ay8913.cpp
 *  Module : Speech/Sound Cartridge — AY-3-8913 PSG
 * ============================================================
*/

// Provenance: VCC ssc/ay8913.cpp (commit db8f7d7, 780TECH), a clean-room
// AY-3-8910-family model. Generators and mixer unchanged.
//
// Ay8913 is small and lives in internal RAM (static in ssc.cpp), so every
// store in this file lands there: the PSRAM memw workaround can be dropped.

#pragma GCC optimize("O2", "jump-tables")
#include "../../utils/no_psram_memw.h"
#include "ay8913.h"


namespace ssc {

// Logarithmic approximation of the AY's per-channel volume table
// (-2dB per step, level 15 = full scale, level 0 = silence). See the
// header comment: this is not a measured-silicon table, just a
// reasonable-sounding approximation of the chip's documented
// logarithmic response.
const int16_t Ay8913::kVolumeTable[16] = {
    0, 1304, 1642, 2067, 2603, 3277, 4125, 5193,
    6538, 8231, 10362, 13045, 16422, 20675, 26028, 32767
};

Ay8913::Ay8913()
{
    Reset();
}

void Ay8913::Reset()
{
    for (auto& r : m_regs) r = 0;
    m_selected = 0;
    for (int i = 0; i < 3; i++) { m_tone_counter[i] = 0; m_tone_output[i] = 1; }
    m_noise_counter = 0;
    m_noise_lfsr = 1;
    m_noise_output = 1;
    m_env_counter = 0;
    m_env_step = 0;
    m_env_dir_up = true;
    m_env_holding = false;
}

void Ay8913::SelectRegister(uint8_t reg)
{
    m_selected = reg & 0x0f;
}

void Ay8913::WriteData(uint8_t data)
{
    m_regs[m_selected] = data;

    if (m_selected == 13) {
        // envelope shape write resets the ramp
        m_env_step = 0;
        m_env_holding = false;
        m_env_dir_up = (data & 0x04) != 0; // Attack
    }
}

uint8_t Ay8913::ReadData() const
{
    return m_regs[m_selected];
}

int Ay8913::TonePeriod(int ch) const
{
    int fine = m_regs[ch * 2];
    int coarse = m_regs[ch * 2 + 1] & 0x0f;
    int p = (coarse << 8) | fine;
    return p ? p : 1;
}

int Ay8913::NoisePeriod() const
{
    int p = m_regs[6] & 0x1f;
    return p ? p : 1;
}

int Ay8913::EnvPeriod() const
{
    int fine = m_regs[11];
    int coarse = m_regs[12];
    int p = (coarse << 8) | fine;
    return p ? p : 1;
}

void Ay8913::TickOneEnvelopeStep()
{
    if (m_env_holding) return;

    uint8_t shape = m_regs[13];
    bool cont = (shape & 0x08) != 0;
    bool alt  = (shape & 0x02) != 0;
    bool hold = (shape & 0x01) != 0;

    m_env_step++;
    if (m_env_step >= 32)
    {
        if (!cont)
        {
            m_env_step = 32;
            m_env_holding = true;
        }
        else if (hold)
        {
            m_env_step = 31;
            m_env_holding = true;
            if (alt) m_env_dir_up = !m_env_dir_up;
        }
        else
        {
            m_env_step = 0;
            if (alt) m_env_dir_up = !m_env_dir_up;
        }
    }
}

int Ay8913::CurrentEnvelopeLevel() const
{
    uint8_t shape = m_regs[13];
    bool cont = (shape & 0x08) != 0;

    if (!cont && m_env_holding)
        return 0;

    int step = m_env_step & 31;
    int level = m_env_dir_up ? step : (31 - step);
    return level >> 1; // 0..31 -> 0..15
}

// Advances a divide-by-period counter by `steps` internal steps, exactly as
// `steps` single steps of "if (++c >= p) { c = 0; event; }" would, and
// returns the number of events. Only the state after the last step is
// sampled, so the generators can jump instead of looping step by step.
static inline __attribute__((always_inline)) int AdvanceCounter(int& c, int p, int steps)
{
    if (steps <= 0) return 0;
    int events = 0;
    if (c >= p) { c = 0; events = 1; steps--; }   // period shrank below the count
    int total = c + steps;
    if (total < p) { c = total; return events; }
    c = total % p;
    return events + total / p;
}

int16_t Ay8913::Render(int steps)
{
    for (int ch = 0; ch < 3; ch++)
        m_tone_output[ch] ^= AdvanceCounter(m_tone_counter[ch], TonePeriod(ch), steps) & 1;

    for (int n = AdvanceCounter(m_noise_counter, NoisePeriod(), steps); n > 0; n--) {
        // 17-bit LFSR, taps at bit0 and bit3 (standard AY polynomial)
        uint32_t bit = ((m_noise_lfsr ^ (m_noise_lfsr >> 3)) & 1);
        m_noise_lfsr = (m_noise_lfsr >> 1) | (bit << 16);
        m_noise_output = int(m_noise_lfsr & 1);
    }

    for (int n = AdvanceCounter(m_env_counter, EnvPeriod(), steps); n > 0; n--)
        TickOneEnvelopeStep();

    uint8_t mixer = m_regs[7];
    int32_t mix = 0;

    for (int ch = 0; ch < 3; ch++)
    {
        bool tone_enabled  = ((mixer >> ch) & 1) == 0;       // active low
        bool noise_enabled = ((mixer >> (ch + 3)) & 1) == 0; // active low

        bool tone_bit  = tone_enabled  ? (m_tone_output[ch] != 0) : true;
        bool noise_bit = noise_enabled ? (m_noise_output != 0)    : true;

        if (!(tone_bit && noise_bit)) continue; // silent this instant

        uint8_t ampreg = m_regs[8 + ch];
        int volIndex;
        if (ampreg & 0x10) volIndex = CurrentEnvelopeLevel();
        else                volIndex = ampreg & 0x0f;

        mix += kVolumeTable[volIndex & 0x0f];
    }

    // Average the (up to 3) contributing channels down so the mix
    // doesn't clip when all three are active simultaneously.
    mix /= 3;
    if (mix > 32767) mix = 32767;
    if (mix < -32768) mix = -32768;
    return int16_t(mix);
}

} // namespace ssc
