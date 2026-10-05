#include "shared.h"

#define MAX_OUTPUT  0x7FFF
#define STEP        0x10000
#define FB_WNOISE   0x12000
#define FB_PNOISE   0x08000
#define NG_PRESET   0x0F35

// Tabla de volúmenes de hardware SMS real, escalada a MAX_OUTPUT
static const int PSGVolumeValues[16] = {
    32767, 26028, 20675, 16422,
    13045, 10362,  8231,  6538,
     5193,  4125,  3277,  2602,
     2067,  1642,  1304,     0
};

t_SN76496 sn[MAX_76496];

void SN76496Write(int chip, int data)
{
    t_SN76496 *R = &sn[chip];

    if (data & 0x80)
    {
        int r = (data & 0x70) >> 4;
        int c = r / 2;

        R->LastRegister = r;
        R->Register[r] = (R->Register[r] & 0x3f0) | (data & 0x0f);
        switch (r)
        {
            case 0: case 2: case 4:
                R->Period[c] = R->UpdateStep * R->Register[r];
                if (R->Period[c] == 0) R->Period[c] = R->UpdateStep;
                if (r == 4 && (R->Register[6] & 0x03) == 0x03)
                    R->Period[3] = 2 * R->Period[2];
                break;
            case 1: case 3: case 5: case 7:
                R->Volume[c] = R->VolTable[data & 0x0f];
                break;
            case 6:
                {
                    int n = R->Register[6];
                    R->NoiseFB = (n & 4) ? FB_WNOISE : FB_PNOISE;
                    n &= 3;
                    R->Period[3] = (n == 3) ? 2 * R->Period[2]
                                            : (R->UpdateStep << (5 + n));
                    R->RNG = NG_PRESET;
                    R->Output[3] = R->RNG & 1;
                }
                break;
        }
    }
    else
    {
        int r = R->LastRegister;
        int c = r / 2;
        switch (r)
        {
            case 0: case 2: case 4:
                R->Register[r] = (R->Register[r] & 0x0f) | ((data & 0x3f) << 4);
                R->Period[c] = R->UpdateStep * R->Register[r];
                if (R->Period[c] == 0) R->Period[c] = R->UpdateStep;
                if (r == 4 && (R->Register[6] & 0x03) == 0x03)
                    R->Period[3] = 2 * R->Period[2];
                break;
        }
    }
}

void SN76496Update(int chip, signed short int *buffer[2], int length, unsigned char mask)
{
    int i, j;
    int buffer_index = 0;
    t_SN76496 *R = &sn[chip];

    if (!buffer || !buffer[0] || !buffer[1]) return;

    for (i = 0; i < 4; i++)
    {
        if (R->Volume[i] == 0)
        {
            if (R->Count[i] <= length * STEP) R->Count[i] += length * STEP;
        }
    }

    while (length > 0)
    {
        int vol[4];
        unsigned int out[2];
        int left;

        vol[0] = vol[1] = vol[2] = vol[3] = 0;

        for (i = 0; i < 3; i++)
        {
            if (R->Output[i]) vol[i] += R->Count[i];
            R->Count[i] -= STEP;
            while (R->Count[i] <= 0)
            {
                R->Count[i] += R->Period[i];
                if (R->Count[i] > 0)
                {
                    R->Output[i] ^= 1;
                    if (R->Output[i]) vol[i] += R->Period[i];
                    break;
                }
                R->Count[i] += R->Period[i];
                vol[i] += R->Period[i];
            }
            if (R->Output[i]) vol[i] -= R->Count[i];
        }

        left = STEP;
        do
        {
            int nextevent = (R->Count[3] < left) ? R->Count[3] : left;

            if (R->Output[3]) vol[3] += R->Count[3];
            R->Count[3] -= nextevent;
            if (R->Count[3] <= 0)
            {
                // Feedback original correcto para SMS — no modificar
                if (R->RNG & 1) R->RNG ^= R->NoiseFB;
                R->RNG >>= 1;
                R->Output[3] = R->RNG & 1;
                R->Count[3] += R->Period[3];
                if (R->Output[3]) vol[3] += R->Period[3];
            }
            if (R->Output[3]) vol[3] -= R->Count[3];

            left -= nextevent;
        } while (left > 0);

        out[0] = out[1] = 0;
        for (j = 0; j < 4; j++)
        {
            int k = vol[j] * R->Volume[j];
            if (mask & (1 << (4 + j))) out[0] += k;
            if (mask & (1 << (0 + j))) out[1] += k;
        }

        if (out[0] > MAX_OUTPUT * STEP) out[0] = MAX_OUTPUT * STEP;
        if (out[1] > MAX_OUTPUT * STEP) out[1] = MAX_OUTPUT * STEP;

        buffer[0][buffer_index] = (int16_t)(out[0] / STEP);
        buffer[1][buffer_index] = (int16_t)(out[1] / STEP);

        buffer_index++;
        length--;
    }
}

void SN76496_set_clock(int chip, int clock)
{
    t_SN76496 *R = &sn[chip];
    R->UpdateStep = ((double)STEP * R->SampleRate * 16) / clock;
}

void SN76496_set_gain(int chip, int gain)
{
    t_SN76496 *R = &sn[chip];
    int i;
    // Usar tabla de volúmenes medida de hardware real escalada a MAX_OUTPUT
    for (i = 0; i < 15; i++)
        R->VolTable[i] = PSGVolumeValues[i];
    R->VolTable[15] = 0;
}

int SN76496_init(int chip, int clock, int volume, int sample_rate)
{
    int i;
    t_SN76496 *R = &sn[chip];

    R->SampleRate = sample_rate;
    SN76496_set_clock(chip, clock);

    for (i = 0; i < 4; i++) R->Volume[i] = 0;

    R->LastRegister = 0;
    for (i = 0; i < 8; i += 2)
    {
        R->Register[i]     = 0;
        R->Register[i + 1] = 0x0f;
    }

    for (i = 0; i < 4; i++)
    {
        R->Output[i] = 0;
        R->Period[i] = R->Count[i] = R->UpdateStep;
    }
    R->RNG = NG_PRESET;
    R->Output[3] = R->RNG & 1;

    SN76496_set_gain(chip, 0);  // gain ignorado — usa tabla fija

    return 0;
}