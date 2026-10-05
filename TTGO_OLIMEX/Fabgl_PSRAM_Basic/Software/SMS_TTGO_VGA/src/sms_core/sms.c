// sms.c - Versión original
#include "shared.h"

void ym2413_write(int chip, int offset, int data);

t_sms sms;

void sms_frame(int skip_render)
{
    if(input.system & INPUT_HARD_RESET) {
        system_reset();
    }

    if(input.system & INPUT_PAUSE) {
        if(!sms.paused) {
            sms.paused = 1;
            z80_set_nmi_line(ASSERT_LINE);
            z80_set_nmi_line(CLEAR_LINE);
        }
    } else {
         sms.paused = 0;
    }

    if(snd.log) snd.callback(0x00);

    for(vdp.line = 0; vdp.line < 262; vdp.line += 1) {
        vdp_run();
        if(!skip_render) render_line(vdp.line);
        z80_execute(227);
    }

    if(snd.enabled) {
        SN76496Update(0, snd.buffer, snd.bufsize, sms.psg_mask);
    }
}

void sms_init(void)
{
    cpu_reset();
    sms_reset();
}

void sms_reset(void)
{
    memset(sms.dummy, 0, 0x2000);
    memset(sms.ram, 0, 0x2000);
    sms.paused = sms.save = sms.port_3F = sms.port_F2 = sms.irq = 0x00;
    sms.psg_mask = 0xFF;
    sms.vga_display = NULL;

    cpu_readmap[0] = cart.rom + 0x0000;
    cpu_readmap[1] = cart.rom + 0x2000;
    cpu_readmap[2] = cart.rom + 0x4000;
    cpu_readmap[3] = cart.rom + 0x6000;
    cpu_readmap[4] = cart.rom + 0x0000;
    cpu_readmap[5] = cart.rom + 0x2000;
    cpu_readmap[6] = sms.ram;            
    cpu_readmap[7] = sms.ram;

    cpu_writemap[0] = sms.dummy;         
    cpu_writemap[1] = sms.dummy;
    cpu_writemap[2] = sms.dummy;         
    cpu_writemap[3] = sms.dummy;
    cpu_writemap[4] = sms.dummy;         
    cpu_writemap[5] = sms.dummy;
    cpu_writemap[6] = sms.ram;           
    cpu_writemap[7] = sms.ram;

    sms.fcr[0] = 0x00;
    sms.fcr[1] = 0x00;
    sms.fcr[2] = 0x01;
    sms.fcr[3] = 0x00;
}

void cpu_reset(void)
{
    z80_reset(0);
    z80_set_irq_callback(sms_irq_callback);
}

void cpu_writemem16(int address, int data)
{
    cpu_writemap[(address >> 13)][(address & 0x1FFF)] = data;
    if(address >= 0xFFFC) sms_mapper_w(address & 3, data);
}

void cpu_writeport(int port, int data)
{
    switch(port & 0xFF)
    {
        case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: break;
        case 0x06:
            if(snd.log) { snd.callback(0x04); snd.callback(data); }
            sms.psg_mask = (data & 0xFF);
            break;
        case 0x7E: case 0x7F:
            if(snd.log) { snd.callback(0x03); snd.callback(data); }
            if(snd.enabled) SN76496Write(0, data);
            break;
        case 0xBE: vdp_data_w(data); break;
        case 0xBD: case 0xBF: vdp_ctrl_w(data); break;
        case 0xF0: case 0xF1:
            if(snd.log) { snd.callback((port & 1) ? 0x06 : 0x05); snd.callback(data); }
            if(snd.enabled && sms.use_fm) ym2413_write(0, port & 1, data);
            break;
        case 0xF2:
            if(sms.use_fm) sms.port_F2 = (data & 1);
            break;
        case 0x3F:
            sms.port_3F = ((data & 0x80) | (data & 0x20) << 1) & 0xC0;
            if(sms.country == TYPE_DOMESTIC) sms.port_3F ^= 0xC0;
            break;
    }
}

int cpu_readport(int port)
{
    uint8 temp = 0xFF;
    switch(port & 0xFF)
    {
        case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: return 0x00;
        case 0x7E: return vdp_vcounter_r();
        case 0x7F: return vdp_hcounter_r();
        case 0x00:
            temp = 0xFF;
            if(input.system & INPUT_START) temp &= ~0x80;
            if(sms.country == TYPE_DOMESTIC) temp &= ~0x40;
            return temp;
        case 0xC0: case 0xDC:
            temp = 0xFF;
            if(input.pad[0] & INPUT_UP)      temp &= ~0x01;
            if(input.pad[0] & INPUT_DOWN)    temp &= ~0x02;
            if(input.pad[0] & INPUT_LEFT)    temp &= ~0x04;
            if(input.pad[0] & INPUT_RIGHT)   temp &= ~0x08;
            if(input.pad[0] & INPUT_BUTTON2) temp &= ~0x10;
            if(input.pad[0] & INPUT_BUTTON1) temp &= ~0x20;
            if(input.pad[1] & INPUT_UP)      temp &= ~0x40;
            if(input.pad[1] & INPUT_DOWN)    temp &= ~0x80;
            return temp;
        case 0xC1: case 0xDD:
            temp = 0xFF;
            if(input.pad[1] & INPUT_LEFT)    temp &= ~0x01;
            if(input.pad[1] & INPUT_RIGHT)   temp &= ~0x02;
            if(input.pad[1] & INPUT_BUTTON2) temp &= ~0x04;
            if(input.pad[1] & INPUT_BUTTON1) temp &= ~0x08;
            if(input.system & INPUT_SOFT_RESET) temp &= ~0x10;
            return ((temp & 0x3F) | (sms.port_3F & 0xC0));
        case 0xBE: return vdp_data_r();
        case 0xBD: case 0xBF: return vdp_ctrl_r();
        case 0xF2:
            if(sms.use_fm) return sms.port_F2;
            break;
    }
    return 0xFF;
}

void sms_mapper_w(int address, int data)
{
    uint8 page = (data % cart.pages);
    sms.fcr[address] = data;

    switch(address)
    {
        case 0:
            if(data & 8) {
                sms.save = 1;
                cpu_readmap[4]  = &sms.sram[(data & 4) ? 0x4000 : 0x0000];
                cpu_readmap[5]  = &sms.sram[(data & 4) ? 0x6000 : 0x2000];
                cpu_writemap[4] = &sms.sram[(data & 4) ? 0x4000 : 0x0000];
                cpu_writemap[5] = &sms.sram[(data & 4) ? 0x6000 : 0x2000];
            } else {
                cpu_readmap[4]  = &cart.rom[((sms.fcr[3] % cart.pages) << 14) + 0x0000];
                cpu_readmap[5]  = &cart.rom[((sms.fcr[3] % cart.pages) << 14) + 0x2000];
                cpu_writemap[4] = sms.dummy;
                cpu_writemap[5] = sms.dummy;
            }
            break;
        case 1:
            cpu_readmap[0] = &cart.rom[(page << 14) + 0x0000];
            cpu_readmap[1] = &cart.rom[(page << 14) + 0x2000];
            break;
        case 2:
            cpu_readmap[2] = &cart.rom[(page << 14) + 0x0000];
            cpu_readmap[3] = &cart.rom[(page << 14) + 0x2000];
            break;
        case 3:
            if(!(sms.fcr[0] & 0x08)) {
                cpu_readmap[4] = &cart.rom[(page << 14) + 0x0000];
                cpu_readmap[5] = &cart.rom[(page << 14) + 0x2000];
            }
            break;
    }
}

int sms_irq_callback(int param)
{
    return 0xFF;
}



void sms_set_vga_display(void* display) {
    sms.vga_display = display;
    if (display) {
        render_set_vga_mode(1);
        g_vga_display = display;
    }
}