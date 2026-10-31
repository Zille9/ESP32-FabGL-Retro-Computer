/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : ssc_bus.h
 *  Module : Speech/Sound Cartridge — chip-to-glue wiring
 * ============================================================
*/

#ifndef SSC_BUS_H
#define SSC_BUS_H

#include <stdint.h>

// The cartridge's internal wiring: the TMS7040 ports and the SP0256 DRQ line
// call straight into the glue (ssc.cpp) instead of through std::function.
// Ports as in MAME's coco_ssc.cpp: A = host command byte, B = RAM A0-A7,
// C = bus control, D = data bus.
uint8_t ssc_bus_porta_read(void);
void    ssc_bus_portb_write(uint8_t data);
uint8_t ssc_bus_portc_read(void);
void    ssc_bus_portc_write(uint8_t data);
uint8_t ssc_bus_portd_read(void);
void    ssc_bus_portd_write(uint8_t data);

// SP0256 DRQ (ready for the next allophone) -> TMS7040 INT1.
void    ssc_bus_speech_drq(bool ready);

#endif // SSC_BUS_H
