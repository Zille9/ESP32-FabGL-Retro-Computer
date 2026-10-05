// sms.h - Versión C pura
#ifndef _SMS_H_
#define _SMS_H_

#define TYPE_OVERSEAS   (0)
#define TYPE_DOMESTIC   (1)

/* SMS context */
typedef struct
{
    uint8 *dummy;
    uint8 ram[0x2000];
    uint8 *sram;
    uint8 fcr[4];
    uint8 paused;
    uint8 save;
    uint8 country;
    uint8 display;
    uint8 port_3F;
    uint8 port_F2;
    uint8 use_fm;
    uint8 irq;
    uint8 psg_mask;
    void* vga_display;      // Usar void* en lugar de clase C++
} t_sms;

/* Global data */
extern t_sms sms;

/* Function prototypes */
void sms_frame(int skip_render);
void sms_init(void);
void sms_reset(void);
int  sms_irq_callback(int param);
void sms_mapper_w(int address, int data);
void cpu_reset(void);

// VGA integration (C-compatible) - usar void* aquí también
void sms_set_vga_display(void* display);


#endif /* _SMS_H_ */
