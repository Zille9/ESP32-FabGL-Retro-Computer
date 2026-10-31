/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : HiresRender.cpp
 *  Module : Hires lookup table, see HiresRender.h.
 * ============================================================
*/

#include "HiresRender.h"
#include <cstring>

namespace HiresRender
{

const uint8_t COLOR[16] =
{
	0, 12, 3, 15, 0, 9, 6, 15,
	0, 3, 12, 15, 0, 6, 9, 15
};

// [parity of the byte's first dot][left dot][byte] -> its 7 dots, padded
// to 8 so every entry is aligned
static uint8_t table[2][2][256][8];

void Init()
{
	for (int parity = 0; parity < 2; parity++)
		for (int left = 0; left < 2; left++)
			for (int b = 0; b < 256; b++)
			{
				int colorSet = (b >> 7) * 4;
				int pbit = left;
				int odd = parity ? 8 : 0;
				for (int i = 0; i < 7; i++)
				{
					int dot = (b >> i) & 1;
					table[parity][left][b][i] = COLOR[odd + colorSet + dot * 2 + pbit] * 0x11;
					pbit = dot;
					odd = odd ? 0 : 8;
				}
				table[parity][left][b][7] = 0;
			}
}

uint8_t DrawCell(uint8_t b0, uint8_t b1, uint8_t pbit, uint8_t* out)
{
	// b0's 7 dots start on an even column, so b1's start on an odd one
	memcpy(out, table[0][pbit][b0], 7);
	memcpy(out + 7, table[1][(b0 >> 6) & 1][b1], 7);
	return (b1 >> 6) & 1;
}

void DrawLine(const uint8_t* src, int* cache, uint8_t* out)
{
	for (int col = 0; col < 40; col += 2)
	{
		int left = col ? (src[col - 1] >> 6) & 1 : 0;
		int key = src[col] | (src[col + 1] << 8) | (left << 16);
		if (cache[col] == key)
			continue;
		cache[col] = key;
		DrawCell(src[col], src[col + 1], left, out + col * 7);
	}
}

}
