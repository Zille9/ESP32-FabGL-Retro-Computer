#pragma once

// Classic99 for ESP32: copied from Classic99 SpeechDll/ (BSD-3-Clause, see license.txt).
// Changed for GCC only: <cstring> and <iterator> added, debug_write takes a const string (it is the
// one in bus.cpp) and the verbose LOGMASKED variant, which relied on MSVC, is removed.

#include <cstdint>
#include <algorithm>
#include <cstring>
#include <iterator>

// this enables the LOGMASKED to all emit
//#define VERBOSE_DEBUG

// simple wrapper to help the mame code feel more at home

#define DECLARE_DEVICE_TYPE(x,y)
#define WRITE_LINE_MEMBER(name) 
#define fatalerror debug_write

#define LOGMASKED(x,...)

typedef uint32_t u32;
typedef int device_type;

typedef struct _mc {
    
    
    
} machine_config;
        
class device_t {
public:
    device_t(const machine_config &, int , const void *, device_t *, int clk) 
        : clock_rate(clk)
    { }

    int clock() { return clock_rate; }

private:
    int clock_rate;
};

class device_sound_interface {
public:
    device_sound_interface(const machine_config &, void *) { }
};

void debug_write(const char *s, ...);
