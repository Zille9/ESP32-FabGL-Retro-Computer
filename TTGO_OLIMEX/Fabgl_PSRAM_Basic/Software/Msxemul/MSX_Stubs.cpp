/*
 * MSX_Stubs.cpp
 * fMSX에서 사용하는 스텁 함수들 (ESP32용)
 *
 * 주의: MSX.h를 include하면 Z80.h의 extern "C" 문제가 발생하므로
 * 필요한 타입만 직접 선언
 */

#include <Arduino.h>
#include <time.h>

typedef unsigned char byte;

extern "C" {

    // getcwd/chdir 스텁
    char *getcwd(char *buf, size_t size) {
        if(buf && size > 0) {
            strncpy(buf, "/", size);
            buf[size-1] = '\0';
        }
        return buf;
    }

    int chdir(const char *path) {
        return 0;
    }

    // access 스텁
    //int access(const char *pathname, int mode) {
    //    return 0;
   // }

    // feof와 fgetc는 ESP32_Port.cpp에서 arduino_feof, arduino_fgetc로 구현됨



} // extern "C"