// Wraps XRoar's hd6309.c (unmodified, from an XRoar source tree) behind a
// tiny C API so cpu_test.cpp can compare our HD6309 against it instruction by
// instruction. Built only when run.sh is given XROAR_SRC.
//
// hd6309.c is #included rather than linked so its static state and register
// macros are reachable; the handful of XRoar services it references but
// never needs for plain execution are stubbed below.

#include "mc6809/hd6309.c"

#include "xroar_oracle.h"

// ---- stubs for XRoar services unused here ----
struct logging logging;
void *part_new(size_t psize) { return calloc(1, psize); }
bool mc6809_is_a(struct part *p, const char *name) { (void)p; (void)name; return 0; }
bool mc6809_get_flag(void *sptr, int flag) { (void)sptr; (void)flag; return 0; }
void mc6809_set_flag(void *sptr, int flag, bool value) { (void)sptr; (void)flag; (void)value; }
const struct ser_struct_data mc6809_ser_struct_data;
const struct debug_feature m6809_core_feature;
const struct debug_feature_type debug_feature_type_uint8, debug_feature_type_uint16;
void delegate_void_default__Bool_uint16(void *sptr, bool rnw, uint16_t a) { (void)sptr; (void)rnw; (void)a; }
void ser_set_error(struct ser_handle *sh, int error) { (void)sh; (void)error; }
void ser_write_vuint32(struct ser_handle *sh, int tag, uint32_t v) { (void)sh; (void)tag; (void)v; }
uint32_t ser_read_vuint32(struct ser_handle *sh) { (void)sh; return 0; }

// ---- oracle ----
static struct HD6309 *xo_hcpu;
static xo_read_fn xo_rd;
static xo_write_fn xo_wr;
static int xo_cycles;
static int xo_fetches;

static void xo_mem_cycle(void *sptr, bool rnw, uint16_t a) {
	struct MC6809 *cpu = sptr;
	xo_cycles++;
	if (rnw) cpu->D = xo_rd(a);
	else xo_wr(a, cpu->D);
}

// Called before each opcode fetch: let the first through, stop at the second.
static void xo_instruction_hook(void *sptr, uint32_t pc) {
	struct MC6809 *cpu = sptr;
	(void)pc;
	if (xo_fetches++ > 0) cpu->running = 0;
}

void xo_init(xo_read_fn rd, xo_write_fn wr) {
	xo_rd = rd;
	xo_wr = wr;
	xo_hcpu = (struct HD6309 *)hd6309_allocate();
	struct MC6809 *cpu = &xo_hcpu->mc6809;
	cpu->mem_cycle = DELEGATE_AS2(void, bool, uint16, xo_mem_cycle, cpu);
	cpu->instruction_hook = DELEGATE_AS1(void, uint32, xo_instruction_hook, cpu);
}

void xo_set(const struct xo_regs *r) {
	struct HD6309 *hcpu = xo_hcpu;
	struct MC6809 *cpu = &hcpu->mc6809;
	REG_PC = r->pc; REG_D = r->d; REG_X = r->x; REG_Y = r->y;
	REG_U = r->u; REG_S = r->s; REG_W = r->w; REG_V = r->v;
	REG_DP = r->dp; REG_CC = r->cc; REG_MD = r->md;
	REG_M = 0;
	cpu->halt = cpu->nmi = cpu->firq = cpu->irq = 0;
	cpu->nmi_latch = cpu->firq_latch = cpu->irq_latch = 0;
	cpu->nmi_active = cpu->firq_active = cpu->irq_active = 0;
	cpu->nmi_armed = 0;
	hcpu->state = hd6309_state_label_a;
}

void xo_get(struct xo_regs *r) {
	struct HD6309 *hcpu = xo_hcpu;
	struct MC6809 *cpu = &hcpu->mc6809;
	r->pc = REG_PC; r->d = REG_D; r->x = REG_X; r->y = REG_Y;
	r->u = REG_U; r->s = REG_S; r->w = REG_W; r->v = REG_V;
	r->dp = REG_DP; r->cc = REG_CC; r->md = REG_MD;
}

int xo_step(void) {
	struct MC6809 *cpu = &xo_hcpu->mc6809;
	xo_cycles = 0;
	xo_fetches = 0;
	cpu->running = 1;
	cpu->run(cpu);
	return xo_cycles;
}
