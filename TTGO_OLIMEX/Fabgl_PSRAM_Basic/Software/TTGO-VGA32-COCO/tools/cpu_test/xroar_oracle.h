// C interface to XRoar's HD6309, used as a reference by cpu_test.cpp.
#ifndef XROAR_ORACLE_H
#define XROAR_ORACLE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

struct xo_regs {
    uint16_t pc, d, x, y, u, s, w, v;
    uint8_t  dp, cc, md;
};

typedef uint8_t (*xo_read_fn)(uint16_t addr);
typedef void    (*xo_write_fn)(uint16_t addr, uint8_t val);

void xo_init(xo_read_fn rd, xo_write_fn wr);
void xo_set(const struct xo_regs* r);
void xo_get(struct xo_regs* r);
// Execute one instruction (a whole TFM counts as one); returns bus cycles.
int  xo_step(void);

#ifdef __cplusplus
}
#endif
#endif
