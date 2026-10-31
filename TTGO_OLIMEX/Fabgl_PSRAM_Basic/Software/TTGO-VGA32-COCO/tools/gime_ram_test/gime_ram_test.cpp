/*
 * Host test: GIME address translation for each CoCo 3 RAM size (128 KB,
 * 512 KB, 1 MB, 2 MB). Checks the physical address tcc1014_mem_cycle()
 * produces, the CPU's direct page pointers, ROM select, and the video bank.
 *
 * Build and run with tools/gime_ram_test/run.sh.
 */
#define TCC1014_RENDER_TEST 1
#include "../../src/core/tcc1014.cpp"

#include <stdio.h>
#include <stdlib.h>
#include <initializer_list>

static uint8_t* ram;
static TCC1014 g;
static int g_fail = 0, g_checks = 0;
static const char* g_name = "";

#define CHECK_EQ(got, want) do { g_checks++; long a_ = (long)(got), b_ = (long)(want); if (a_ != b_) { g_fail++; \
    printf("  FAIL [%s] line %d: %s = $%lX, want $%lX\n", g_name, __LINE__, #got, a_, b_); } } while (0)

// Physical address the CPU address maps to, wrapped as machine.cpp does.
static uint32_t phys(uint16_t addr) {
    tcc1014_mem_cycle(&g, addr, true, 0, nullptr, nullptr);
    return g.Z & g.ram_mask;
}

static void setup(const char* name, uint32_t kb) {
    g_name = name;
    tcc1014_init(&g);
    tcc1014_reset(&g);
    tcc1014_set_ram(&g, ram, kb * 1024);
    tcc1014_write_register(&g, 0, 0x40);          // INIT0: MMU on
    tcc1014_set_sam_register(&g, 0x8000);         // TY: all-RAM, so reads reach the RAM under the ROM
}

int main() {
    ram = (uint8_t*)calloc(1, 2048 * 1024);

    // ---- reset mapping is the same top-of-512K layout on every size ----
    for (uint32_t kb : {128u, 512u, 1024u, 2048u}) {
        setup("reset mapping", kb);
        uint32_t top = (kb == 128) ? 0x10000u : 0x70000u;      // page $38 wrapped into RAM
        CHECK_EQ(phys(0x0000), top);
        CHECK_EQ(phys(0x2345), top + 0x2345);
        CHECK_EQ(g.wr_page[0] - ram, top);
        CHECK_EQ(g.bank_mask, kb == 2048 ? 0xFF : kb == 1024 ? 0x7F : 0x3F);
    }

    // ---- 128 KB: only 16 pages exist, every bank number mirrors onto them ----
    setup("128K mirrors", 128);
    tcc1014_write_mmu(&g, 0, 0x30);
    CHECK_EQ(phys(0x0000), 0x00000);
    tcc1014_write_mmu(&g, 0, 0x00);                // same RAM as $30
    CHECK_EQ(phys(0x0000), 0x00000);
    tcc1014_write_mmu(&g, 0, 0x3F);
    CHECK_EQ(phys(0x0010), 0x1E010);
    tcc1014_write_mmu(&g, 0, 0xFF);                // upper bits do not exist
    CHECK_EQ(tcc1014_read_mmu(&g, 0), 0x3F);

    // ---- 512 KB: 64 pages, upper register bits ignored ----
    setup("512K", 512);
    tcc1014_write_mmu(&g, 1, 0x00);
    CHECK_EQ(phys(0x2000), 0x00000);
    tcc1014_write_mmu(&g, 1, 0xC5);
    CHECK_EQ(tcc1014_read_mmu(&g, 1), 0x05);
    CHECK_EQ(phys(0x2001), 0x0A001);
    CHECK_EQ(g.rd_page[1] - ram, 0x0A000);

    // ---- 1 MB: bit 6 selects the second 512 KB ----
    setup("1M", 1024);
    tcc1014_write_mmu(&g, 2, 0x40);
    CHECK_EQ(tcc1014_read_mmu(&g, 2), 0x40);
    CHECK_EQ(phys(0x4000), 0x80000);
    CHECK_EQ(g.wr_page[2] - ram, 0x80000);
    tcc1014_write_mmu(&g, 2, 0xFF);                // bit 7 does not exist
    CHECK_EQ(tcc1014_read_mmu(&g, 2), 0x7F);
    CHECK_EQ(phys(0x4000), 0xFE000);

    // ---- 2 MB: bits 7-6 select one of four banks ----
    setup("2M", 2048);
    tcc1014_write_mmu(&g, 3, 0xC0);
    CHECK_EQ(phys(0x6000), 0x180000);
    tcc1014_write_mmu(&g, 3, 0xFF);
    CHECK_EQ(phys(0x7FFF), 0x1FFFFF);
    CHECK_EQ(g.rd_page[3] - ram, 0x1FE000);
    tcc1014_write_mmu(&g, 3, 0x80);
    CHECK_EQ(phys(0x6000), 0x100000);

    // ---- task register switches to the second set of eight ----
    tcc1014_write_mmu(&g, 8, 0x81);
    tcc1014_write_register(&g, 1, 0x01);
    CHECK_EQ(phys(0x0000), 0x102000);
    tcc1014_write_register(&g, 1, 0x00);

    // ---- MMU off, and the $FE00 page with MC3, always use bank 0's top pages ----
    tcc1014_write_mmu(&g, 7, 0xC0);
    tcc1014_write_register(&g, 0, 0x48);           // MMU on + MC3
    CHECK_EQ(phys(0xE000), 0x180000);
    CHECK_EQ(phys(0xFE00), 0x7FE00);
    tcc1014_write_register(&g, 0, 0x00);           // MMU off
    CHECK_EQ(phys(0xE000), 0x7E000);
    CHECK_EQ(phys(0x0000), 0x70000);

    // ---- ROM select ignores the extension bits ----
    setup("ROM select", 2048);
    tcc1014_set_sam_register(&g, 0);               // TY off: ROM mode
    tcc1014_write_mmu(&g, 4, 0x3C);
    tcc1014_mem_cycle(&g, 0x8000, true, 0, nullptr, nullptr);
    CHECK_EQ(g.RAS, 0);                            // ROM
    CHECK_EQ(g.rd_page[4] == nullptr, 1);
    CHECK_EQ(g.wr_page[4] - ram, 0x78000);         // writes still reach the RAM underneath
    tcc1014_write_mmu(&g, 4, 0xFC);                // bank 3, page $3C: still the ROM region
    tcc1014_mem_cycle(&g, 0x8000, true, 0, nullptr, nullptr);
    CHECK_EQ(g.RAS, 0);
    CHECK_EQ(g.wr_page[4] - ram, 0x1F8000);
    tcc1014_write_mmu(&g, 4, 0xC0);
    tcc1014_mem_cycle(&g, 0x8000, true, 0, nullptr, nullptr);
    CHECK_EQ(g.RAS, 1);

    // ---- video bank ($FF9B) ----
    setup("video bank 2M", 2048);
    CHECK_EQ(g.vram_base, 0);
    CHECK_EQ(g.vram_mask, 0x7FFFF);
    tcc1014_write_register(&g, 0x0B, 0x03);
    CHECK_EQ(g.vram_base, 0x180000);
    {
        uint8_t v = 0;
        bool reg = false;
        tcc1014_mem_cycle(&g, 0xFF9B, true, 0, &v, &reg);
        CHECK_EQ(v & 0x03, 0x03);
    }
    g.B = 0x60400;
    ram[0x180000 + 0x60400] = 0x5A;
    CHECK_EQ(*fetch_line_bytes(&g, 32), 0x5A);
    tcc1014_reset(&g);
    CHECK_EQ(g.vram_base, 0);                      // reset returns video to bank 0

    setup("video bank 1M", 1024);
    tcc1014_write_register(&g, 0x0B, 0x03);        // only one extra bank exists
    CHECK_EQ(g.vram_base, 0x80000);

    setup("video bank ignored at 512K", 512);
    tcc1014_write_register(&g, 0x0B, 0x03);
    CHECK_EQ(g.vram_base, 0);

    setup("128K video wraps", 128);
    CHECK_EQ(g.vram_mask, 0x1FFFF);
    g.B = 0x60400;                                 // the reset video address, above 128K
    ram[0x00400] = 0xA5;
    CHECK_EQ(*fetch_line_bytes(&g, 32), 0xA5);

    printf("gime_ram_test: %d checks, %d failed\n", g_checks, g_fail);
    printf(g_fail ? "FAILED\n" : "PASSED\n");
    return g_fail ? 1 : 0;
}
