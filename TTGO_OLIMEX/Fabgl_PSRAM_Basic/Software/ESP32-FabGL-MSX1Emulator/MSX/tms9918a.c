#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <esp_attr.h>  // これで IRAM_ATTR が使えるようになります
#include <string.h>    // memsetなどを使っている場合
#include "tms9918a.h"

uint8_t sram_framebuffer[16384] DRAM_ATTR __attribute__((aligned(4)));
/*
 *	Sprites
 */

/*
 *	Draw a horizontal slice of a sprite into the render buffer. If it
 *	needs magnifying then do the magnification as we render. We use a
 *	simple line buffer to detect collisions
 */
// 4ビットのパターンを32bitの色データに展開するヘルパー
static inline uint32_t expand_bits_to_color(uint8_t nibble, uint8_t color) {
	uint32_t res = 0;
	// リトルエンディアン(ESP32)を想定した並び
	if (nibble & 0x08) res |= (uint32_t)color;
	if (nibble & 0x04) res |= (uint32_t)color << 8;
	if (nibble & 0x02) res |= (uint32_t)color << 16;
	if (nibble & 0x01) res |= (uint32_t)color << 24;
	return res;
}

// 拡大用：2ビット(拡大後4px分)を32bitカラーデータに展開
static inline uint32_t expand_mag_bits_to_color(uint8_t two_bits, uint8_t color) {
	uint32_t res = 0;
	// bit1 (左側) が 1 なら 0,1ピクセル目を塗る
	if (two_bits & 0x02) {
		res |= (uint32_t)color;
		res |= (uint32_t)color << 8;
	}
	// bit0 (右側) が 1 なら 2,3ピクセル目を塗る
	if (two_bits & 0x01) {
		res |= (uint32_t)color << 16;
		res |= (uint32_t)color << 24;
	}
	return res;
}

static IRAM_ATTR void tms9918a_render_slice(struct tms9918a *vdp, int y, uint8_t *sprat, uint16_t bits, unsigned int width) {
	int x = sprat[1];
	if (sprat[3] & 0x80) x -= 32;

	// 拡大フラグの取得
	int mag = vdp->reg[1] & 0x01;
	// 拡大時は描画幅が2倍になる
	int real_width = mag ? (width << 1) : width;

	// 画面外の完全なクリッピング

	uint8_t foreground = sprat[3] & 0x0F;

	if (width == 8) bits &= 0xFF00;
	uint8_t *out = &vdp->active_raster[256 * y];

	if (!mag) {
		for (int i = 0; i < (int)width; i += 4) {
			int target_x = x + i;
			uint8_t nibble = (uint8_t)((bits >> (12 - i)) & 0x0F);
			if (nibble == 0) continue;

			if (target_x >= 0 && target_x <= 252) {
				// 4ピクセル一括（余計な if(out==0) は入れない！）
				if (nibble == 0x0F) {
					out[target_x + 0] = foreground;
					out[target_x + 1] = foreground;
					out[target_x + 2] = foreground;
					out[target_x + 3] = foreground;
				} else {
					for (int b = 0; b < 4; b++) {
						if ((nibble >> (3 - b)) & 1) out[target_x + b] = foreground;
					}
				}
			} else {
				// 画面端
				for (int b = 0; b < 4; b++) {
					if ((nibble >> (3 - b)) & 1) {
						int tx = target_x + b;
						if (tx >= 0 && tx < 256) out[tx] = foreground;
					}
				}
			}
		}
	} else {
		// --- Magnify（2倍拡大）モード ---
		// widthが8(拡大後16px)なら4回、16(拡大後32px)なら8回ループ
		for (int i = 0; i < (int)width; i += 2) {
			// 拡大モードは1ビットを2ピクセルに広げるため、 target_x は i*2 で進む
			int target_x = x + (i * 2);

			// bits(16bit)のうち、左から2ビットずつ取り出す
			// i=0: >>14, i=2: >>12, i=4: >>10... とシフトしていく
			uint8_t two_bits = (uint8_t)((bits >> (14 - i)) & 0x03);

			if (two_bits == 0) continue;

			// 中央描画（高速化エリア：x=0〜252、かつ拡大後幅16px〜32pxが収まる範囲）
			// 拡大後の最大幅は32pxなので、target_x + 31 までが画面内にあるかを判定
			// ここでは安全を見て以前の 252 判定を使いつつ、4ピクセル単位で処理
			if (target_x >= 0 && target_x <= (256 - 4)) {

				if (two_bits == 0x03) {
					// ★ 2ビットとも立っている（＝拡大後4ピクセル連続塗り）
					// 逆順描画なので、if(out==0) の判定なしで無条件上書き！
					uint8_t *p = out + target_x;
					p[0] = foreground;
					p[1] = foreground;
					p[2] = foreground;
					p[3] = foreground;
				} else {
					// 透明が混ざっている時は2ピクセル単位（元の1ビット分）で判定
					// これも上書きで背景を保護
					for (int b = 0; b < 2; b++) {
						if ((two_bits >> (1 - b)) & 1) {
							int tx = target_x + (b * 2);
							// 拡大なので2ピクセル連続で塗る
							out[tx] = foreground;
							out[tx + 1] = foreground;
						}
					}
				}
			} else {
				// 境界付近（画面端）の安全な描画
				// 1ピクセルごとに画面内（0〜255）かをチェックして塗る
				for (int b = 0; b < 2; b++) {
					if ((two_bits >> (1 - b)) & 1) {
						int tx = target_x + (b * 2);
						// 左側のピクセル
						if (tx >= 0 && tx < 256) out[tx] = foreground;
						// 右側のピクセル（拡大により生まれたピクセル）
						if (tx + 1 >= 0 && tx + 1 < 256) out[tx + 1] = foreground;
					}
				}
			}
		}
	}
}

static IRAM_ATTR void tms9918a_render_sprite(struct tms9918a *vdp, int y, uint8_t *sprat, uint8_t *spdat) {
	int row = *sprat;
	uint16_t bits;
	unsigned int width = 8;
	unsigned int mag = vdp->reg[1] & 0x01;

	if (row > 0xE0)
		row -= 0x100;
	row += 1;
	row = y - row;
	if (mag)
		row >>= 1;

	if ((vdp->reg[1] & 0x02) == 0) {
		spdat += row;
		width = 8;
		bits = *spdat << 24;
	} else {
		spdat += row;
		bits = *spdat << 8;
		bits |= spdat[16];
		width = 16;
	}
	tms9918a_render_slice(vdp, y, sprat, bits, width);
}

void prepare_sprites(struct tms9918a *vdp) {
	memset(vdp->sprite_line_count, 0, 192);
	uint8_t *sprat = vdp->framebuffer + ((vdp->reg[5] & 0x7F) << 7);
	int spheight = vdp->reg[1] & 0x02 ? 16 : 8;
	int mag = vdp->reg[1] & 0x01;
	if (mag) spheight <<= 1;

	for (int i = 0; i < 32; i++) {
		int ypos = sprat[i * 4];
		if (ypos == 0xD0) break;
		if (ypos > 0xE0) ypos -= 256;
		ypos += 1;

		// このスプライトが存在する行の範囲だけリストに追加
		int start_y = (ypos < 0) ? 0 : ypos;
		int end_y = (ypos + spheight > 192) ? 192 : ypos + spheight;

		for (int y = start_y; y < end_y; y++) {
			if (vdp->sprite_line_count[y] < 32) {
				vdp->sprite_line_list[y][vdp->sprite_line_count[y]++] = i;
			}
		}
	}
}

static IRAM_ATTR void tms9918a_sprite_line(struct tms9918a *vdp, int y) {
	uint8_t *sprat_base = vdp->framebuffer + ((vdp->reg[5] & 0x7F) << 7);
	uint8_t *spdat = vdp->framebuffer + ((vdp->reg[6] & 0x07) << 11);
	unsigned int spshft = 3;

	// プリスキャン済みのリストから枚数を取得
	int count = vdp->sprite_line_count[y];

	// --- スプライト描画のループ（逆順にする） ---
	if (vdp->limit_sprites && count > 4) {
		uint8_t fifth_idx = vdp->sprite_line_list[y][4];
		vdp->status |= 0x40 | (fifth_idx & 0x1F);
		count = 4;  // 描画は4枚で打ち切り
	}

	// 番号が大きい順（奥から手前）に描画
	for (int n = count - 1; n >= 0; n--) {
		uint8_t i = vdp->sprite_line_list[y][n];
		uint8_t *sprat = sprat_base + (i * 4);

		// 色番号0（透明）なら関数自体呼ばない
		uint8_t foreground = sprat[3] & 0x0F;
		if (foreground == 0) continue;

		tms9918a_render_sprite(vdp, y, sprat, spdat + (sprat[2] << spshft));
	}
}

static IRAM_ATTR void tms9918a_raster_sprites(struct tms9918a *vdp) {
	// ★最適化: 衝突判定バッファのクリアは1フレームに1回で十分！
	memset(vdp->colbuf, 0xFF, sizeof(vdp->colbuf));

	unsigned int i;
	for (i = 0; i < 192; i++)
		tms9918a_sprite_line(vdp, i);
}

/* ★大改修：スキャンライン単位＆32bit一括書き込み G1モード */
static void IRAM_ATTR tms9918a_rasterize_g1(struct tms9918a *vdp) {
	uint8_t *p_base = vdp->framebuffer + ((vdp->reg[2] & 0x0F) << 10);
	uint8_t *pattern = vdp->framebuffer + ((vdp->reg[4] & 0x07) << 11);
	uint8_t *colour = vdp->framebuffer + (vdp->reg[3] << 6);
	uint8_t *out = vdp->active_raster;

	for (int char_row = 0; char_row < 24; char_row++) {
		for (int scanline = 0; scanline < 8; scanline++) {
			uint8_t *p_line = p_base + char_row * 32;
			for (int x = 0; x < 32; x++) {
				uint8_t code = *p_line++;
				uint8_t b = pattern[(code << 3) + scanline];
				uint8_t c = colour[code >> 3];
				uint8_t c_arr[2] = { c & 0x0F, c >> 4 };

				// 分岐なし！4ピクセルずつ32bitでメモリに叩き込む
				*((uint32_t *)out) = c_arr[(b >> 7) & 1] | (c_arr[(b >> 6) & 1] << 8) | (c_arr[(b >> 5) & 1] << 16) | (c_arr[(b >> 4) & 1] << 24);
				*((uint32_t *)(out + 4)) = c_arr[(b >> 3) & 1] | (c_arr[(b >> 2) & 1] << 8) | (c_arr[(b >> 1) & 1] << 16) | (c_arr[b & 1] << 24);
				out += 8;
			}
		}
	}
	tms9918a_raster_sprites(vdp);
}

/* ★大改修：スキャンライン単位＆32bit一括書き込み G2モード（MSXで一番重いモード） */
static IRAM_ATTR void tms9918a_rasterize_g2(struct tms9918a *vdp) {
	uint8_t *p_base = vdp->framebuffer + ((vdp->reg[2] & 0x0F) << 10);
	uint8_t *pattern0 = vdp->framebuffer + ((vdp->reg[4] & 0x04) << 11);
	uint8_t *colour0 = vdp->framebuffer + ((vdp->reg[3] & 0x80) << 6);
	uint8_t *out = vdp->active_raster;

	for (int char_row = 0; char_row < 24; char_row++) {
		uint8_t *pattern = pattern0;
		uint8_t *colour = colour0;

		// 画面の上下で参照アドレスを切り替える
		if (char_row >= 8 && char_row < 16) {
			if (vdp->reg[4] & 0x01) pattern += 0x0800;
			if (vdp->reg[3] & 0x20) colour += 0x0800;
		} else if (char_row >= 16) {
			if (vdp->reg[4] & 0x02) pattern += 0x1000;
			if (vdp->reg[3] & 0x40) colour += 0x1000;
		}

		for (int scanline = 0; scanline < 8; scanline++) {
			uint8_t *p_line = p_base + char_row * 32;
			for (int x = 0; x < 32; x++) {
				uint8_t code = *p_line++;
				uint8_t b = pattern[(code << 3) + scanline];
				uint8_t c = colour[(code << 3) + scanline];
				uint8_t c_arr[2] = { c & 0x0F, c >> 4 };

				// 分岐排除＆32bitシーケンシャル書き込み
				*((uint32_t *)out) = c_arr[(b >> 7) & 1] | (c_arr[(b >> 6) & 1] << 8) | (c_arr[(b >> 5) & 1] << 16) | (c_arr[(b >> 4) & 1] << 24);
				*((uint32_t *)(out + 4)) = c_arr[(b >> 3) & 1] | (c_arr[(b >> 2) & 1] << 8) | (c_arr[(b >> 1) & 1] << 16) | (c_arr[b & 1] << 24);
				out += 8;
			}
		}
	}
	tms9918a_raster_sprites(vdp);
}

/* ★大改修：スキャンライン単位 MCモード */
static void IRAM_ATTR tms9918a_rasterize_mc(struct tms9918a *vdp) {
	uint8_t *p_base = vdp->framebuffer + ((vdp->reg[2] & 0x0F) << 10);
	uint8_t *pattern = vdp->framebuffer + ((vdp->reg[4] & 0x07) << 11);
	uint8_t *out = vdp->active_raster;

	for (int char_row = 0; char_row < 24; char_row++) {
		for (int scanline = 0; scanline < 8; scanline++) {
			uint8_t *p_line = p_base + char_row * 32;
			int block_y = (scanline >> 2) & 1;
			int pat_offset = (char_row & 3) << 1;

			for (int x = 0; x < 32; x++) {
				uint8_t code = *p_line++;
				uint8_t c = pattern[(code << 3) + pat_offset + block_y];
				uint8_t c_arr[2] = { c >> 4, c & 0x0F };

				uint32_t p1 = c_arr[0] | (c_arr[0] << 8) | (c_arr[0] << 16) | (c_arr[0] << 24);
				uint32_t p2 = c_arr[1] | (c_arr[1] << 8) | (c_arr[1] << 16) | (c_arr[1] << 24);
				*((uint32_t *)out) = p1;
				*((uint32_t *)(out + 4)) = p2;
				out += 8;
			}
		}
	}
	tms9918a_raster_sprites(vdp);
}

/* ★大改修：スキャンライン単位 TEXTモード */
static void IRAM_ATTR tms9918a_rasterize_text(struct tms9918a *vdp) {
	uint8_t *p_base = vdp->framebuffer + ((vdp->reg[2] & 0x0F) << 10);
	uint8_t *pattern = vdp->framebuffer + ((vdp->reg[4] & 0x07) << 11);
	uint8_t *out = vdp->active_raster;
	uint8_t bg = vdp->reg[7] & 0x0F;
	uint8_t fg = vdp->reg[7] >> 4;
	uint8_t c_arr[2] = { bg, fg };
	uint32_t bg_block = bg | (bg << 8) | (bg << 16) | (bg << 24);

	for (int char_row = 0; char_row < 24; char_row++) {
		for (int scanline = 0; scanline < 8; scanline++) {
			uint8_t *p_line = p_base + char_row * 40;

			// Left Border (8px)
			*((uint32_t *)out) = bg_block;
			out += 4;
			*((uint32_t *)out) = bg_block;
			out += 4;

			for (int x = 0; x < 40; x++) {
				uint8_t code = *p_line++;
				uint8_t b = pattern[(code << 3) + scanline];
				*out++ = c_arr[(b >> 7) & 1];
				*out++ = c_arr[(b >> 6) & 1];
				*out++ = c_arr[(b >> 5) & 1];
				*out++ = c_arr[(b >> 4) & 1];
				*out++ = c_arr[(b >> 3) & 1];
				*out++ = c_arr[(b >> 2) & 1];
			}

			// Right Border (8px)
			*((uint32_t *)out) = bg_block;
			out += 4;
			*((uint32_t *)out) = bg_block;
			out += 4;
		}
	}
}

void IRAM_ATTR tms9918a_rasterize(struct tms9918a *vdp) {
	prepare_sprites(vdp);
	unsigned int mode = (vdp->reg[1] >> 2) & 0x06;
	mode |= (vdp->reg[0] & 0x02) >> 1;

	if ((vdp->reg[1] & 0x40) == 0)
		memset(vdp->active_raster, 0, sizeof(vdp->active_raster));  // 画面OFF時
	else {
		switch (mode) {
			case 0: tms9918a_rasterize_g1(vdp); break;
			case 1: tms9918a_rasterize_g2(vdp); break;
			case 2: tms9918a_rasterize_mc(vdp); break;
			case 4: tms9918a_rasterize_text(vdp); break;
			default: memset(vdp->active_raster, 0, sizeof(vdp->active_raster));
		}
	}
	// if (vdp->trace)
	// 	fprintf(stderr, "vdp: frame done.\n");
	vdp->status |= 0x80;
}

void IRAM_ATTR tms9918a_write(struct tms9918a *vdp, uint8_t addr, uint8_t val) {
	switch (addr & 1) {
		case 0:
			vdp->framebuffer[vdp->addr] = val;
			vdp->addr++;
			vdp->addr &= vdp->memmask;
			vdp->latch = 0;
			break;
		case 1:
			if (vdp->latch == 0) {
				vdp->addr &= 0xFF00;
				vdp->addr |= val;
				vdp->latch = 1;
				return;
			}
			vdp->latch = 0;
			switch (val & 0xC0) {
				case 0x00:  // Read set up
					vdp->addr &= 0xFF;
					vdp->addr |= (val << 8);
					vdp->read = 1;
					vdp->addr &= vdp->memmask;
					break;
				case 0x40:  // Write set up
					vdp->addr &= 0xFF;
					vdp->addr |= (val << 8);
					vdp->addr &= vdp->memmask;
					vdp->read = 0;
					break;
				case 0x80:  // Reg write
					vdp->reg[val & 7] = vdp->addr & 0xFF;
					break;
			}
	}
}

uint8_t IRAM_ATTR tms9918a_read(struct tms9918a *vdp, uint8_t addr) {
	uint8_t r;
	if ((addr & 1) == 0) {
		r = vdp->framebuffer[vdp->addr++ & vdp->memmask];
	} else {
		r = vdp->status;
		vdp->status = 0;  // Read status clears it
	}
	vdp->latch = 0;
	return r;
}

void tms9918a_reset(struct tms9918a *vdp) {
	memset(vdp->reg, 0, sizeof(vdp->reg));
	vdp->addr = 0;
	vdp->latch = 0;
	vdp->read = 0;
	vdp->status = 0;
}

struct tms9918a *tms9918a_create(void) {
	// ps_malloc が使えない環境（PSRAMなし等）を考慮し、
	// 汎用的な malloc を使用。必要に応じて ps_malloc に戻してください。
	struct tms9918a *vdp = (struct tms9918a *)ps_malloc(sizeof(struct tms9918a));
	if (vdp) {
		memset(vdp, 0, sizeof(struct tms9918a));
		vdp->framebuffer = sram_framebuffer;
		vdp->memmask = 0x3FFF;
		vdp->limit_sprites = 1;
	}
	return vdp;
}

int tms9918a_irq_pending(struct tms9918a *vdp) {
	if (vdp->reg[1] & 0x20)
		return vdp->status & 0x80;
	return 0;
}

void IRAM_ATTR flip_buffer(struct tms9918a *vdp) {
	// ポインタの入れ替えによるダブルバッファリング
	vdp->visible_raster = vdp->active_raster;
	if (vdp->active_raster == vdp->rasterbuffer0) {
		vdp->active_raster = vdp->rasterbuffer1;
	} else {
		vdp->active_raster = vdp->rasterbuffer0;
	}
}