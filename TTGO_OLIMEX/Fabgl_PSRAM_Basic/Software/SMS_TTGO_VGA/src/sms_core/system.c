/*
    Copyright (C) 1998, 1999, 2000  Charles Mac Donald

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
// system.c - Versión original (sin modificaciones)
#include "shared.h"

t_bitmap bitmap;
t_cart cart;                
t_snd snd;
t_input input;

void emu_system_init(int rate)
{
    vdp_init();
    sms_init();
    render_init();
    audio_init(rate);
    sms.save = 0;
    memset(&input, 0, sizeof(t_input));
}

void audio_init(int rate)
{
    memset(&snd, 0, sizeof(t_snd));
    snd.log = 0;
    snd.callback = NULL;

    if(!rate) return;

    snd.bufsize = rate == 15720 ? 262 : 312;
    
    snd.buffer[0] = (signed short int *)malloc(snd.bufsize * 2);
    snd.buffer[1] = (signed short int *)malloc(snd.bufsize * 2);
    
    if(!snd.buffer[0] || !snd.buffer[1]) return;
    
    memset(snd.buffer[0], 0, snd.bufsize * 2);
    memset(snd.buffer[1], 0, snd.bufsize * 2);
    
    SN76496_init(0, MASTER_CLOCK, 255, rate);
    
    snd.enabled = 1;
}

void system_shutdown(void)
{
}

void system_reset(void)
{
    cpu_reset();
    vdp_reset();
    sms_reset();
    render_reset();
    if(snd.enabled) {
        SN76496_init(0, MASTER_CLOCK, 255, snd.bufsize ? 15720 : 15600);
    }
}

void system_save_state(void *fd)
{
    fwrite(&vdp, sizeof(t_vdp), 1, fd);
    fwrite(&sms, sizeof(t_sms), 1, fd);
    fwrite(Z80_Context, sizeof(Z80_Regs), 1, fd);
    fwrite(&after_EI, sizeof(int), 1, fd);
}

void system_load_state(void *fd)
{
    int i;
    uint8 reg[0x40];

    cpu_reset();
    system_reset();

    fread(&vdp, sizeof(t_vdp), 1, fd);
    fread(&sms, sizeof(t_sms), 1, fd);
    fread(Z80_Context, sizeof(Z80_Regs), 1, fd);
    fread(&after_EI, sizeof(int), 1, fd);

    z80_set_irq_callback(sms_irq_callback);

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

    sms_mapper_w(3, sms.fcr[3]);
    sms_mapper_w(2, sms.fcr[2]);
    sms_mapper_w(1, sms.fcr[1]);
    sms_mapper_w(0, sms.fcr[0]);

    for(i = 0; i < PALETTE_SIZE; i += 1)
        palette_sync(i);
}

void ym2413_write(int chip, int offset, int data)
{
    // YM2413 support removed
}