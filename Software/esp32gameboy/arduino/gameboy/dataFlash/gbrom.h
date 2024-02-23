#ifndef _GB_ROM_H
 #define _GB_ROM_H

 //#include <stddef.h>
 #include "roms/romLastCrown.h"
 #include "roms/romMario.h"
 #include "roms/romonehour.h"
 #define max_list_rom 3


 //roms
 //Titulos
 static const char * gb_list_rom_title[max_list_rom]={
  "LastCrown",
  "Super Mario",
  "OneHour"
 };

 //Datos rom
 static const unsigned char * gb_list_rom_data[max_list_rom]={
  gb_rom_LastCrown,
  gb_rom_Mario,
  gb_rom_onehour
 };

 //Tamanio en bytes
 //static const int gb_list_rom_size[max_list_rom]={
 // 262144//,
 // 262144
 //};

#endif
