#ifndef _TMS9918A_H
#define _TMS9918A_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
void tms9918a_rasterize(struct tms9918a *vdp);
void tms9918a_reset(struct tms9918a *vdp);
void tms9918a_write(struct tms9918a *vdp, uint8_t addr, uint8_t val);
uint8_t tms9918a_read(struct tms9918a *vdp, uint8_t addr);
int tms9918a_irq_pending(struct tms9918a *vdp);
void flip_buffer(struct tms9918a *vdp);
struct tms9918a *tms9918a_create(void);
#endif

struct tms9918a {
  // --- [保存が必要なメンバ] ---
  uint8_t reg[8];
  uint8_t status;
  unsigned int latch;
  unsigned int read;
  uint16_t addr;
  uint16_t memmask;
  int trace;
	int limit_sprites;

	// --- [保存不要・またはポインタなので別途管理するメンバ] ---
  uint8_t *framebuffer;   // ポインタなので退避が必要
  uint8_t *active_raster; // ポインタなので退避が必要
  uint8_t *visible_raster;// ポインタなので退避が必要
  uint32_t *colourmap;    // ポインタなので退避が必要

  // 巨大な実体バッファ（保存対象から外したい）
  uint8_t rasterbuffer0[256 * 192]; 
  uint8_t rasterbuffer1[256 * 192];
	uint8_t sprite_line_list[192][32]; // 各行に並ぶスプライトのインデックス
	uint8_t sprite_line_count[192];    // 各行に何枚あるか
	uint8_t colbuf[256 + 64]; // 衝突用の一時バッファなので保存不要
};
extern uint8_t sram_framebuffer[];

#ifdef __cplusplus
}
#endif
#endif