/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : hires_test.cpp
 *  Module : Host-side hires renderer test: the lookup-table
 *           HiresRender::DrawCell against the per-dot loop
 *           AppleVideo::RenderHires used before it, for every pair
 *           of bytes and both left neighbours; and DrawLine, the
 *           cached line renderer built on it.
 *           Driven by tests/host/run-hires-tests.sh.
 * ============================================================
*/

#include <cstdio>
#include <cstring>
#include <string>

#include "HiresRender.h"

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
	checks++;
	if (!ok)
	{
		printf("  FAIL %s\n", what.c_str());
		failures++;
	}
}

// AppleVideo::RenderHires's inner loop as it was before the table, for one
// 14-dot cell. Returns the cell's last dot.
static uint8_t OldCell(uint8_t b0, uint8_t b1, uint8_t pbit, uint8_t* out)
{
	static const uint8_t hiresColor[16] =
	{
		0, 12, 3, 15, 0, 9, 6, 15,
		0, 3, 12, 15, 0, 6, 9, 15
	};
	uint16_t word = ((uint16_t)b1 << 8) | b0;
	uint8_t bits[16];
	for (int bit = 0; bit < 16; bit++)
		bits[bit] = (word >> bit) & 1;

	int x = 0;
	uint8_t colorSet = bits[7] * 4;
	uint8_t even = 0;
	int bit = 0;
	while (bit < 15)
	{
		if (bit == 7)
		{
			colorSet = bits[15] * 4;
			bit++;
		}
		out[x++] = hiresColor[even + colorSet + (bits[bit] << 1) + pbit] * 0x11;
		pbit = bits[bit++];
		even = even ? 0 : 8;
	}
	return pbit;
}

static void TestEveryCell()
{
	HiresRender::Init();
	int bad = 0;
	for (int pbit = 0; pbit < 2; pbit++)
		for (int w = 0; w < 0x10000; w++)
		{
			uint8_t want[14], got[16];
			memset(got, 0xAB, sizeof(got));
			uint8_t wantLast = OldCell(w & 0xFF, w >> 8, pbit, want);
			uint8_t gotLast = HiresRender::DrawCell(w & 0xFF, w >> 8, pbit, got);
			if (memcmp(want, got, 14) != 0 || wantLast != gotLast || got[14] != 0xAB)
			{
				if (bad < 5)
					printf("  mismatch: bytes %02X %02X, left dot %d\n", w & 0xFF, w >> 8, pbit);
				bad++;
			}
		}
	expect(bad == 0, "every cell matches the old renderer, and nothing past its 14 bytes is written");
}

// A whole line as the screen should show it: every cell drawn fresh, each
// taking its left dot from the byte before it
static void FreshLine(const uint8_t* src, uint8_t* out)
{
	for (int col = 0; col < 40; col += 2)
		HiresRender::DrawCell(src[col], src[col + 1], col ? (src[col - 1] >> 6) & 1 : 0, out + col * 7);
}

static void TestDrawLine()
{
	HiresRender::Init();
	uint8_t src[40];
	for (int i = 0; i < 40; i++)
		src[i] = (uint8_t)(i * 37 + 11);
	int cache[40];
	for (int i = 0; i < 40; i++)
		cache[i] = -1;
	uint8_t out[280], want[280];

	HiresRender::DrawLine(src, cache, out);
	FreshLine(src, want);
	expect(memcmp(out, want, 280) == 0, "DrawLine: a fresh line matches cell-by-cell drawing");

	// the F2 overlay scribbles on the right-hand cells and marks them dirty;
	// the redraw must give back exactly what was there
	memset(out + 30 * 7, 0x55, 10 * 7);
	for (int col = 30; col < 40; col += 2)
		cache[col] = -1;
	HiresRender::DrawLine(src, cache, out);
	expect(memcmp(out, want, 280) == 0, "DrawLine: cells under the overlay come back with the right colours");

	// only the left neighbour's last dot changes: the next cell redraws too
	src[29] ^= 0x40;
	HiresRender::DrawLine(src, cache, out);
	FreshLine(src, want);
	expect(memcmp(out, want, 280) == 0, "DrawLine: a changed last dot recolours the next cell");

	// nothing changed: nothing is drawn
	memset(out, 0x77, sizeof(out));
	HiresRender::DrawLine(src, cache, out);
	bool untouched = true;
	for (uint8_t b : out)
		untouched &= (b == 0x77);
	expect(untouched, "DrawLine: an unchanged line draws nothing");
}

int main()
{
	TestEveryCell();
	TestDrawLine();
	if (failures)
	{
		printf("FAIL %d of %d checks\n", failures, checks);
		return 1;
	}
	printf("PASS %d checks\n", checks);
	return 0;
}
