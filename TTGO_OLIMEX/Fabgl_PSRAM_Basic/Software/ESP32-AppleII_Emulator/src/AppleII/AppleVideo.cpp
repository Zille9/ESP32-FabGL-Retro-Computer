/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : AppleVideo.cpp
 *  Module : Apple II video. Renders 40- and 80-column text, lores,
 *           hires and double hires from emulated memory into the 640x200 VGA
 *           framebuffer as palette indices: a 560x192 screen, each
 *           40-column dot two pixels wide, centred in a black
 *           border. Per-cell dirty caches skip unchanged cells;
 *           hires dots come from HiresRender's lookup table.
 *           Also draws the F2 FPS overlay.
 * ============================================================
*/

#include "Predef.h"
#include "AppleVideo.h"
#include "AppleMem.h"
#include "Apple2Device.h"
#include "../VGA/VGA.h"
#include "FramePacer.h"
#include "HiresRender.h"

// The Apple screen, 280x192 dots at double width, centred on the framebuffer.
// X0 is even, so every 40-column dot is one whole framebuffer byte.
#define SCREEN_X0   40
#define SCREEN_Y0   4
#define CELL_W      (FONT_X * 2)             // a 40-column text or lores cell

const int offsetGR[24] = {                                                    // helper for TEXT and GR video generation
  0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380,                     // lines 0-7
  0x028, 0x0A8, 0x128, 0x1A8, 0x228, 0x2A8, 0x328, 0x3A8,                     // lines 8-15
  0x050, 0x0D0, 0x150, 0x1D0, 0x250, 0x2D0, 0x350, 0x3D0 };                    // lines 16-23


const int offsetHGR[192] = {                                                  // helper for HGR video generation
	0x0000, 0x0400, 0x0800, 0x0C00, 0x1000, 0x1400, 0x1800, 0x1C00,             // lines 0-7
	0x0080, 0x0480, 0x0880, 0x0C80, 0x1080, 0x1480, 0x1880, 0x1C80,             // lines 8-15
	0x0100, 0x0500, 0x0900, 0x0D00, 0x1100, 0x1500, 0x1900, 0x1D00,             // lines 16-23
	0x0180, 0x0580, 0x0980, 0x0D80, 0x1180, 0x1580, 0x1980, 0x1D80,
	0x0200, 0x0600, 0x0A00, 0x0E00, 0x1200, 0x1600, 0x1A00, 0x1E00,
	0x0280, 0x0680, 0x0A80, 0x0E80, 0x1280, 0x1680, 0x1A80, 0x1E80,
	0x0300, 0x0700, 0x0B00, 0x0F00, 0x1300, 0x1700, 0x1B00, 0x1F00,
	0x0380, 0x0780, 0x0B80, 0x0F80, 0x1380, 0x1780, 0x1B80, 0x1F80,
	0x0028, 0x0428, 0x0828, 0x0C28, 0x1028, 0x1428, 0x1828, 0x1C28,
	0x00A8, 0x04A8, 0x08A8, 0x0CA8, 0x10A8, 0x14A8, 0x18A8, 0x1CA8,
	0x0128, 0x0528, 0x0928, 0x0D28, 0x1128, 0x1528, 0x1928, 0x1D28,
	0x01A8, 0x05A8, 0x09A8, 0x0DA8, 0x11A8, 0x15A8, 0x19A8, 0x1DA8,
	0x0228, 0x0628, 0x0A28, 0x0E28, 0x1228, 0x1628, 0x1A28, 0x1E28,
	0x02A8, 0x06A8, 0x0AA8, 0x0EA8, 0x12A8, 0x16A8, 0x1AA8, 0x1EA8,
	0x0328, 0x0728, 0x0B28, 0x0F28, 0x1328, 0x1728, 0x1B28, 0x1F28,
	0x03A8, 0x07A8, 0x0BA8, 0x0FA8, 0x13A8, 0x17A8, 0x1BA8, 0x1FA8,
	0x0050, 0x0450, 0x0850, 0x0C50, 0x1050, 0x1450, 0x1850, 0x1C50,
	0x00D0, 0x04D0, 0x08D0, 0x0CD0, 0x10D0, 0x14D0, 0x18D0, 0x1CD0,
	0x0150, 0x0550, 0x0950, 0x0D50, 0x1150, 0x1550, 0x1950, 0x1D50,
	0x01D0, 0x05D0, 0x09D0, 0x0DD0, 0x11D0, 0x15D0, 0x19D0, 0x1DD0,
	0x0250, 0x0650, 0x0A50, 0x0E50, 0x1250, 0x1650, 0x1A50, 0x1E50,
	0x02D0, 0x06D0, 0x0AD0, 0x0ED0, 0x12D0, 0x16D0, 0x1AD0, 0x1ED0,             // lines 168-183
	0x0350, 0x0750, 0x0B50, 0x0F50, 0x1350, 0x1750, 0x1B50, 0x1F50,             // lines 176-183
	0x03D0, 0x07D0, 0x0BD0, 0x0FD0, 0x13D0, 0x17D0, 0x1BD0, 0x1FD0 };            // lines 184-191


// The palette while the emulator owns the screen: the 16 lores colours in
// Apple order, so a lores nibble is its own index. Hires and text use
// entries from the same set.
static const VGAColor applePalette[16] =
{
	{   0,   0,   0 }, { 226,  57,  86 }, {  28, 116, 205 }, { 126, 110, 173 },
	{  31, 129, 128 }, { 137, 130, 122 }, {  86, 168, 228 }, { 144, 178, 223 },
	{ 151,  88,  34 }, { 234, 108,  21 }, { 158, 151, 143 }, { 255, 206, 240 },
	{ 144, 192,  49 }, { 255, 253, 166 }, { 159, 210, 213 }, { 255, 255, 255 }
};

#define A2_BLACK    0
#define A2_VIOLET   3
#define A2_BLUE     6
#define A2_ORANGE   9
#define A2_GREEN    12
#define A2_WHITE    15

AppleVideo::AppleVideo()
{
	vga = NULL;
	// the first frame loads the palette and clears the border
	fullRepaint = true;
	charRom = false;
	lastMode = -1;
	Reset();
}

void AppleVideo::Create()
{
	font.Create();
	HiresRender::Init();
}

void AppleVideo::LoadCharRom(const BYTE* rom)
{
	font.LoadCharRom(rom);
	charRom = true;
}

void AppleVideo::Reset()
{
	// after a reset the screen still shows the old picture: every cell has
	// to be drawn again, whatever memory now holds
	InvalidateCells();
}

void AppleVideo::InvalidateCells()
{
	memset(LoResCache, 0xFF, sizeof(LoResCache));
	memset(TextCache, 0xFF, sizeof(TextCache));
	memset(HiResCache, 0xFF, sizeof(HiResCache));
}

void AppleVideo::InvalidateRenderCache()
{
	InvalidateCells();
	// whoever drew over us changed the palette and the border as well
	fullRepaint = true;
}

/*
	TEXT 40x24 ( 7x8 Font ), IIe 80x24
	LORES : 40x24 (MIX 40x20)
	HIRES : 280×192 (MIX 280×160)
	In MIX mode the bottom is TEXT ( 4 Line : 32 pixel )
*/
// F2 overlay: 9 text cells in the top right corner ("999FPS 1X").
// FPS_COL is where the glyphs go; hires caches at a 2-byte (14 dot)
// granularity, so the invalidated span starts one byte column earlier.
#define FPS_COL			31
#define FPS_LEN			9
#define FPS_HIRES_COL	30

void AppleVideo::InvalidateFpsOverlayRegion()
{
	for (int col = FPS_HIRES_COL; col < SCREENTEXT_X; col++)
	{
		if (col >= FPS_COL)
		{
			TextCache[0][col] = -1;
			LoResCache[0][col] = -1;
		}
		for (int line = 0; line < FONT_Y; line++)
			HiResCache[line][col] = -1;
	}
	// the same corner in 80-column text
	for (int col = FPS_COL * 2; col < 80; col++)
		TextCache[0][col] = -1;
}

void AppleVideo::RenderFpsOverlay(int fps, const char* speed)
{
	char text[FPS_LEN + 1];
	if (fps < 0)   fps = 0;
	if (fps > 999) fps = 999;
	snprintf(text, sizeof(text), "%3dFPS %s", fps, speed);

	// RenderFont paints the whole cell, so the black background still
	// covers whatever the emulator drew underneath. The //e character ROM
	// is indexed by screen code, where plain ASCII is inverse video or
	// MouseText: its normal glyphs are the codes with bit 7 set. The ][+
	// font is indexed by ASCII.
	BYTE normal = charRom ? 0x80 : 0x00;
	for (int i = 0; i < FPS_LEN; i++)
		font.RenderFont(vga, (BYTE)text[i] | normal, SCREEN_X0 + (FPS_COL + i) * CELL_W, SCREEN_Y0,
		                false, A2_GREEN, A2_BLACK);
}

void AppleVideo::Render(Memory& mem, const Apple2Device& dev, int frame, VGA* vgaOut)
{
	vga = vgaOut;
	if (vga == NULL)
		return;

	// Power-up, or back from the supervisor with its palette and chrome on
	// screen: restore our palette and a black border. The caches were
	// invalidated with this, so every cell repaints below.
	if (fullRepaint)
	{
		vga->setPalette(applePalette);
		vga->clear(A2_BLACK);
		fullRepaint = false;
	}

	// 80STORE turns PAGE2 into a main/aux memory switch; the display then
	// stays on page 1
	int page = mem.store80 ? 1 : dev.videoPage;
	bool col80 = dev.col80 && mem.auxRam != NULL;
	bool dhgr = col80 && dev.dhires && dev.hires_Mode;

	// After a change of video mode nothing in the caches describes what is
	// on screen, so repaint every cell rather than leave stale ones until
	// the periodic refresh.
	int mode = (dev.textMode ? 1 : 0) | (dev.mixedMode ? 2 : 0) | (dev.hires_Mode ? 4 : 0) |
	           (page << 3) | (col80 ? 32 : 0) | (dev.altCharset ? 64 : 0) | (dhgr ? 128 : 0);
	if (mode != lastMode)
	{
		lastMode = mode;
		InvalidateCells();
	}

	// the overlay scribbles over cells the caches believe are up to date,
	// so give them back to the emulator before it paints this frame
	if (dev.fpsOverlay)
		InvalidateFpsOverlayRegion();

	// TEXT wins over the graphics switches; MIXED keeps the bottom four
	// text lines under lores or hires
	if (!dev.textMode)
	{
		if (dhgr)
			RenderDoubleHires(mem, page, dev.mixedMode ? 160 : 192);
		else if (dev.hires_Mode)
			RenderHires(mem, page, dev.mixedMode ? 160 : 192);
		else
			RenderLores(mem, page, dev.mixedMode ? 20 : 24);
	}
	if (dev.textMode || dev.mixedMode)
		RenderText(mem, dev, page, dev.textMode ? 0 : 20, frame, col80);

	// drawn last: the overlay sits on top of the emulated screen
	if (dev.fpsOverlay)
		RenderFpsOverlay(dev.fpsValue, FramePacer::Label((FramePacer::Speed)dev.speedMode));
}

// The video circuit reads main and aux RAM directly, whatever RAMRD or the
// other MMU switches say, so the screen comes from mem.ram / mem.auxRam
// rather than through ReadByte.
void AppleVideo::RenderText(Memory& mem, const Apple2Device& dev, int page, int firstLine, int frame, bool col80)
{
	WORD base = page * 0x0400;
	int cols   = col80 ? 80 : SCREENTEXT_X;
	int cellW  = col80 ? FONT_X : CELL_W;
	int scaleX = col80 ? 1 : 2;
	bool flashNormal = frame < 15;           // flashing cells show normal video half the time

	for (int line = firstLine; line < SCREENTEXT_Y; line++)
	{
		int row = base + offsetGR[line];
		for (int c = 0; c < cols; c++)
		{
			// 80 columns: aux memory holds the even columns, main the odd
			BYTE code;
			if (col80)
				code = (c & 1) ? mem.ram[row + (c >> 1)] : mem.auxRam[row + (c >> 1)];
			else
				code = mem.ram[row + c];

			int glyph;
			bool inverse = false;
			if (charRom)
			{
				// IIe video ROM: the screen byte is the glyph, inverse video
				// included, except $40-$7F in the primary set, which flash
				// between the inverse and normal forms of $00-$3F. The
				// alternate set shows MouseText and inverse lowercase there.
				glyph = code;
				if (!dev.altCharset && code >= 0x40 && code < 0x80)
					glyph = (code & 0x3F) | (flashNormal ? 0x80 : 0);
			}
			else
			{
				// ][+: bit 7 set is normal, $40-$7F flash, the rest inverse
				int fontattr = 0;
				if (code > 0x7F)
					fontattr = FONT_NORMAL;
				else if (code < 0x40)
					fontattr = FONT_INVERSE;
				else
					fontattr = FONT_FLASH;

				glyph = code & 0x7F; // unset bit 7
				if (glyph > 0x5F) glyph &= 0x3F; // shifts to match
				if (glyph < 0x20) glyph |= 0x40; // the ASCII codes

				inverse = !(fontattr == FONT_NORMAL || (fontattr == FONT_FLASH && flashNormal));
			}

			// only redraw a cell whose glyph or flash phase actually changed
			int drawn = glyph | (inverse ? 0x100 : 0);
			if (TextCache[line][c] != drawn)
			{
				TextCache[line][c] = drawn;
				font.RenderFont(vga, glyph, SCREEN_X0 + c * cellW, SCREEN_Y0 + line * FONT_Y,
				                inverse, A2_WHITE, A2_BLACK, scaleX);
			}
		}
	}
}

// A lores cell is two 14x4 blocks, the top one from the low nibble. The
// nibble is the palette index.
void AppleVideo::RenderLores(Memory& mem, int page, int lines)
{
	WORD base = page * 0x0400;

	for (int line = 0; line < lines; line++)
	{
		for (int col = 0; col < SCREENTEXT_X; col++)
		{
			BYTE glyph = mem.ram[base + offsetGR[line] + col];
			if (LoResCache[line][col] == glyph)
				continue;
			LoResCache[line][col] = glyph;

			int byteX = (SCREEN_X0 + col * CELL_W) >> 1;
			int y = SCREEN_Y0 + line * FONT_Y;
			BYTE top    = (glyph & 0x0F) * 0x11;          // both nibbles of the byte
			BYTE bottom = (glyph >> 4) * 0x11;
			for (int r = 0; r < FONT_Y; r++)
				memset(vga->row(y + r) + byteX, r < 4 ? top : bottom, CELL_W / 2);
		}
	}
}

// Double hires (IIe: HIRES + 80COL + AN3 off): 560 dots a line, 7 bits at a
// time from aux then main memory. Every 4 dots from the left edge make one
// of 140 colour pixels, the 4-bit pattern (leftmost dot in bit 0) indexing
// the 16 lores colours. The cache works on 4 bytes at once - 28 dots, exactly
// 7 colour pixels - so no pixel straddles two cache entries.
void AppleVideo::RenderDoubleHires(Memory& mem, int page, int lines)
{
	WORD base = page * 0x2000;

	for (int line = 0; line < lines; line++)
	{
		// one framebuffer pixel per dot, so 2 dots per byte
		uint8_t* out = vga->row(SCREEN_Y0 + line) + (SCREEN_X0 >> 1);
		const BYTE* mainRow = mem.ram + base + offsetHGR[line];
		const BYTE* auxRow  = mem.auxRam + base + offsetHGR[line];

		for (int col = 0; col < SCREENTEXT_X; col += 2)
		{
			uint32_t dots =  (uint32_t)(auxRow[col]      & 0x7F)
			              | ((uint32_t)(mainRow[col]     & 0x7F) << 7)
			              | ((uint32_t)(auxRow[col + 1]  & 0x7F) << 14)
			              | ((uint32_t)(mainRow[col + 1] & 0x7F) << 21);
			if (HiResCache[line][col] == (int)dots)
				continue;
			HiResCache[line][col] = dots;

			uint8_t* p = out + col * 7;              // 28 dots = 14 bytes
			for (int g = 0; g < 7; g++, dots >>= 4)
			{
				BYTE c = (dots & 0x0F) * 0x11;
				p[2 * g] = c;
				p[2 * g + 1] = c;
			}
		}
	}
}

void AppleVideo::RenderHires(Memory& mem, int page, int lines)
{
	WORD base = page * 0x2000;

	// one framebuffer byte per Apple dot; HiresRender redraws only the cells
	// whose bytes, or the dot to their left, changed
	for (int line = 0; line < lines; line++)
		HiresRender::DrawLine(mem.ram + base + offsetHGR[line], HiResCache[line],
		                      vga->row(SCREEN_Y0 + line) + (SCREEN_X0 >> 1));
}
