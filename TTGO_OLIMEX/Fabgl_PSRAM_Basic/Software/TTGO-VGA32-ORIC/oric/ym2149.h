//
//   header file for ym2149 psg 
//

#include "ym2149_d.h"

#include "driver/adc.h"
#include "esp_adc_cal.h"

#undef THIS
#define THIS System.Psg

static const int8_t sinTable[257] = {
   0,    3,    6,    9,   12,   16,   19,   22,   25,   28,   31,   34,   37,   40,   43,   46,
  49,   51,   54,   57,   60,   63,   65,   68,   71,   73,   76,   78,   81,   83,   85,   88,
  90,   92,   94,   96,   98,  100,  102,  104,  106,  107,  109,  111,  112,  113,  115,  116,
 117,  118,  120,  121,  122,  122,  123,  124,  125,  125,  126,  126,  126,  127,  127,  127,
 127,  127,  127,  127,  126,  126,  126,  125,  125,  124,  123,  122,  122,  121,  120,  118,
 117,  116,  115,  113,  112,  111,  109,  107,  106,  104,  102,  100,   98,   96,   94,   92,
  90,   88,   85,   83,   81,   78,   76,   73,   71,   68,   65,   63,   60,   57,   54,   51,
  49,   46,   43,   40,   37,   34,   31,   28,   25,   22,   19,   16,   12,    9,    6,    3,
   0,   -3,   -6,   -9,  -12,  -16,  -19,  -22,  -25,  -28,  -31,  -34,  -37,  -40,  -43,  -46,
 -49,  -51,  -54,  -57,  -60,  -63,  -65,  -68,  -71,  -73,  -76,  -78,  -81,  -83,  -85,  -88,
 -90,  -92,  -94,  -96,  -98, -100, -102, -104, -106, -107, -109, -111, -112, -113, -115, -116,
-117, -118, -120, -121, -122, -122, -123, -124, -125, -125, -126, -126, -126, -127, -127, -127,
-127, -127, -127, -127, -126, -126, -126, -125, -125, -124, -123, -122, -122, -121, -120, -118,
-117, -116, -115, -113, -112, -111, -109, -107, -106, -104, -102, -100,  -98,  -96,  -94,  -92,
 -90,  -88,  -85,  -83,  -81,  -78,  -76,  -73,  -71,  -68,  -65,  -63,  -60,  -57,  -54,  -51,
 -49,  -46,  -43,  -40,  -37,  -34,  -31,  -28,  -25,  -22,  -19,  -16,  -12,   -9,   -6,   -3,
   0,
};

// Volume table dividido por 3 em vez de 6 (dobra o volume, sem clipping:
// no pior caso 3 canais somam 3 * 65535/3 = 65535, exatamente o range de 16 bits).
// O DAC de 8 bits do ESP32 usa so os 8 bits altos, entao um sinal amplo eh essencial.
const static WORD __volTab[] = { 0, 513/3, 828/3, 1239/3, 1923/3, 3238/3, 4926/3, 9110/3, 10344/3, 17876/3, 24682/3, 30442/3, 38844/3, 47270/3, 56402/3, 65535/3};
const static BYTE __envShape0[] = { 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 128+15 };
const static BYTE __envShape4[] = {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 0, 128+16 };

const static BYTE __envShape8[] = { 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 128+0 };
const static BYTE __envShapeA[] = { 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 128+0 };
const static BYTE __envShapeB[] = { 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 15, 128+16 };
const static BYTE __envShapeC[] = {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 128+0 };
const static BYTE __envShapeD[] = {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 128+15 };
const static BYTE __envShapeE[] = {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 128+0 };
const static BYTE *__envShapes[] = {  __envShape0, // 0000
                                __envShape0, // 0001
                                __envShape0, // 0010
                                __envShape0, // 0011
                                __envShape4, // 0100
                                __envShape4, // 0101
                                __envShape4, // 0110
                                __envShape4, // 0111
                                __envShape8, // 1000
                                __envShape0, // 1001
                                __envShapeA, // 1010
                                __envShapeB, // 1011
                                __envShapeC, // 1100
                                __envShapeD, // 1101
                                __envShapeE, // 1110
                                __envShape4};// 1111

void _ym2149DoSamples() {   // for real-time audio asynchronous with cpu
    WORD *buf = THIS.m_sampleBuffer;
    int ptr = 0;
    do {
        THIS.cyclesB += SOUND_FREQ;
        // psg state machine
        if (THIS.noiseCount >= THIS.noiseLimit) {
            BYTE rbit = (THIS.rnd & 1) ^ ((THIS.rnd >> 2) & 1);
            THIS.rnd = (THIS.rnd >> 1) | ((DWORD)(rbit) << 16);
            THIS.noiseCount = 0;
            THIS.noiseLevel ^= (rbit & 1);
        } else THIS.noiseCount++;
        if (THIS.toneCount0 >= THIS.toneLimit0) {
            THIS.toneCount0 = 0; THIS.toneLevel0 ^=  1;				// ...and invert the square wave output
        } else THIS.toneCount0++;
        if (THIS.toneCount1 >= THIS.toneLimit1) {
            THIS.toneCount1 = 0; THIS.toneLevel1 ^=  1;				// ...and invert the square wave output
        } else THIS.toneCount1++;
        if (THIS.toneCount2 >= THIS.toneLimit2) {
            THIS.toneCount2 = 0; THIS.toneLevel2 ^=  1;				// ...and invert the square wave output
        } else THIS.toneCount2++;        
        if (THIS.envCount >= THIS.envLimit) {
            THIS.envCount = 0;
            THIS.envPos++;										// Move to the next envelope position
            if ((THIS.envTab[THIS.envPos] & 0x80) != 0) 			// Reached the end of the envelope?
                THIS.envPos = THIS.envTab[THIS.envPos] & 0x7F;
            if ((THIS.amplitude0 & 0x10) != 0)  					// If the channel is using the envelope generator...
                THIS.toneVol0 = THIS.envTab[THIS.envPos];          // Recalculate its output volume
            if ((THIS.amplitude1 & 0x10) != 0)  					// If the channel is using the envelope generator...
                THIS.toneVol1 = THIS.envTab[THIS.envPos];          // Recalculate its output volume
            if ((THIS.amplitude2 & 0x10) != 0)  					// If the channel is using the envelope generator...
                THIS.toneVol2 = THIS.envTab[THIS.envPos];          // Recalculate its output volume
        } else THIS.envCount++;
        // samples generation
        while (THIS.cyclesB >= PSG_FREQ) {	// one data at samplerate Hz to output (is psg frequency >> 2)
            WORD dout = 32768;
            THIS.cyclesB -= PSG_FREQ;
            if ((THIS.toneBit0 | THIS.noiseBit0) != 0) {
                BYTE a = (THIS.toneBit0 & THIS.toneLevel0) | (THIS.noiseBit0 & THIS.noiseLevel);
                dout += (a != 0) ? __volTab[THIS.toneVol0] : -__volTab[THIS.toneVol0];
            }
            if ((THIS.toneBit1 | THIS.noiseBit1) != 0) {
                BYTE a = (THIS.toneBit1 & THIS.toneLevel1) | (THIS.noiseBit1 & THIS.noiseLevel);
                dout += (a != 0) ? __volTab[THIS.toneVol1] : -__volTab[THIS.toneVol1];
            }
            if ((THIS.toneBit2 | THIS.noiseBit2) != 0) {
                BYTE a = (THIS.toneBit2 & THIS.toneLevel2) | (THIS.noiseBit2 & THIS.noiseLevel);
                dout += (a != 0) ? __volTab[THIS.toneVol2] : -__volTab[THIS.toneVol2];
            }
            buf[ptr + (ptr & 1 ? -1 : 1)] = dout;
            ptr++;
            if (ptr == SOUND_SAMPLE_BUFFER_SIZE) break;   // leave
        }
    } while (ptr < SOUND_SAMPLE_BUFFER_SIZE);
}

void ym2149DoAudio() {  // for synchronous audio with cpu
    WORD dout = 0;
    BYTE a = 0;
    while (THIS.cyclesA >= THIS.audioCyclesA) {     // times 16 predivisor done
        THIS.cyclesA -= THIS.audioCyclesA;
        THIS.cyclesB += THIS.audioCyclesB;                        // this.audioCyclesB;
        
        if (THIS.noiseCount >= THIS.noiseLimit) {
            BYTE rbit = (THIS.rnd & 1) ^ ((THIS.rnd >> 2) & 1);
            THIS.rnd = (THIS.rnd >> 1) | ((DWORD)(rbit) << 16);
            THIS.noiseCount = 0;
            THIS.noiseLevel ^= (rbit & 1);
        } else THIS.noiseCount++;

        if (THIS.toneCount0 >= THIS.toneLimit0) {
            THIS.toneCount0 = 0; THIS.toneLevel0 ^=  1;				// ...and invert the square wave output
        } else THIS.toneCount0++;
        if (THIS.toneCount1 >= THIS.toneLimit1) {
            THIS.toneCount1 = 0; THIS.toneLevel1 ^=  1;				// ...and invert the square wave output
        } else THIS.toneCount1++;
        if (THIS.toneCount2 >= THIS.toneLimit2) {
            THIS.toneCount2 = 0; THIS.toneLevel2 ^=  1;				// ...and invert the square wave output
        } else THIS.toneCount2++;
        
        if (THIS.envCount >= THIS.envLimit) {
            THIS.envCount = 0;
            THIS.envPos++;										// Move to the next envelope position
            if ((THIS.envTab[THIS.envPos] & 0x80) != 0) 			// Reached the end of the envelope?
                THIS.envPos = THIS.envTab[THIS.envPos] & 0x7F;
            if ((THIS.amplitude0 & 0x10) != 0)  					// If the channel is using the envelope generator...
                THIS.toneVol0 = THIS.envTab[THIS.envPos];          // Recalculate its output volume
            if ((THIS.amplitude1 & 0x10) != 0)  					// If the channel is using the envelope generator...
                THIS.toneVol1 = THIS.envTab[THIS.envPos];          // Recalculate its output volume
            if ((THIS.amplitude2 & 0x10) != 0)  					// If the channel is using the envelope generator...
                THIS.toneVol2 = THIS.envTab[THIS.envPos];          // Recalculate its output volume
        } else THIS.envCount++;
    
        if (THIS.cyclesB >= PSG_FREQ) {	// one data at samplerate Hz to output (is psg frequency >> 2)
            THIS.cyclesB -= PSG_FREQ;
            dout = 32768;
            if ((THIS.toneBit0 | THIS.noiseBit0) != 0) {
                a = (THIS.toneBit0 & THIS.toneLevel0) | (THIS.noiseBit0 & THIS.noiseLevel);
                dout += (a != 0) ? __volTab[THIS.toneVol0] : -__volTab[THIS.toneVol0];
            }
            if ((THIS.toneBit1 | THIS.noiseBit1) != 0) {
                a = (THIS.toneBit1 & THIS.toneLevel1) | (THIS.noiseBit1 & THIS.noiseLevel);
                dout += (a != 0) ? __volTab[THIS.toneVol1] : -__volTab[THIS.toneVol1];
            }
            if ((THIS.toneBit2 | THIS.noiseBit2) != 0) {
                a = (THIS.toneBit2 & THIS.toneLevel2) | (THIS.noiseBit2 & THIS.noiseLevel);
                dout += (a != 0) ? __volTab[THIS.toneVol2] : -__volTab[THIS.toneVol2];
            }
            // dout += 0.1*Math.sin(this.bufferPtr*2.0*3.14159*440.0/44100.0);
            THIS.buffer[THIS.bufferWritePtr + (THIS.bufferWritePtr & 1 ? -1 : 1)] = dout;
            THIS.bufferWritePtr++;
            if (THIS.bufferWritePtr == THIS.bufferLength) THIS.bufferWritePtr = 0;         // loop back ;)
        }
    }
}

#ifdef ESP_IDF_VERSION
   #define FABGL_ESP_IDF_VERSION_VAL                      ESP_IDF_VERSION_VAL
   #define FABGL_ESP_IDF_VERSION                          ESP_IDF_VERSION
 #else
   #define FABGL_ESP_IDF_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))
   #define FABGL_ESP_IDF_VERSION                          FABGL_ESP_IDF_VERSION_VAL(0, 0, 0)
 #endif

// ============================================================================
// AUDIO OUTPUT: DAC DIRETO via esp_timer periodico
// ============================================================================
// O modo I2S DAC BUILT-IN nao funciona neste projeto (config OK, samples
// gerados, mas nada sai no GPIO25). Isso e o mesmo padrao usado pelo teu
// bootloader/CoCo/BBC nessa placa: dac_output_voltage() direto no GPIO25,
// disparado por hardware timer, sem I2S nenhum.
// ============================================================================

#include "driver/dac.h"

// Hardware timer para audio - roda em ISR real de hardware, sem starvation.
// hw_timer_t existe no IDF 4.4 via API Arduino do ESP32.
static hw_timer_t *s_audioTimer = nullptr;
static portMUX_TYPE s_audioMux = portMUX_INITIALIZER_UNLOCKED;

// Gera 1 tick da state machine do PSG. Se acumulou tempo suficiente,
// tambem gera 1 sample e escreve no DAC.
// Chamado pelo timer a PSG_FREQ Hz (62500 Hz) - igual ao PSG real do Oric.
static void IRAM_ATTR _ym2149DoOneSample() {
    // psg state machine - 1 tick por chamada @ PSG_FREQ
    if (THIS.noiseCount >= THIS.noiseLimit) {
        BYTE rbit = (THIS.rnd & 1) ^ ((THIS.rnd >> 2) & 1);
        THIS.rnd = (THIS.rnd >> 1) | ((DWORD)(rbit) << 16);
        THIS.noiseCount = 0;
        THIS.noiseLevel ^= (rbit & 1);
    } else THIS.noiseCount++;
    if (THIS.toneCount0 >= THIS.toneLimit0) {
        THIS.toneCount0 = 0; THIS.toneLevel0 ^= 1;
    } else THIS.toneCount0++;
    if (THIS.toneCount1 >= THIS.toneLimit1) {
        THIS.toneCount1 = 0; THIS.toneLevel1 ^= 1;
    } else THIS.toneCount1++;
    if (THIS.toneCount2 >= THIS.toneLimit2) {
        THIS.toneCount2 = 0; THIS.toneLevel2 ^= 1;
    } else THIS.toneCount2++;
    if (THIS.envCount >= THIS.envLimit) {
        THIS.envCount = 0;
        THIS.envPos++;
        if ((THIS.envTab[THIS.envPos] & 0x80) != 0)
            THIS.envPos = THIS.envTab[THIS.envPos] & 0x7F;
        if ((THIS.amplitude0 & 0x10) != 0) THIS.toneVol0 = THIS.envTab[THIS.envPos];
        if ((THIS.amplitude1 & 0x10) != 0) THIS.toneVol1 = THIS.envTab[THIS.envPos];
        if ((THIS.amplitude2 & 0x10) != 0) THIS.toneVol2 = THIS.envTab[THIS.envPos];
    } else THIS.envCount++;

    // Sample de saida: gerado a cada SOUND_FREQ/PSG_FREQ ticks.
    // cyclesB acumula SOUND_FREQ por tick; quando passa de PSG_FREQ, gera sample.
    THIS.cyclesB += SOUND_FREQ;
    if (THIS.cyclesB >= PSG_FREQ) {
        THIS.cyclesB -= PSG_FREQ;

        // mixing dos 3 canais - unsigned centrado em 32768
        int32_t dout = 32768;
        if ((THIS.toneBit0 | THIS.noiseBit0) != 0) {
            BYTE a = (THIS.toneBit0 & THIS.toneLevel0) | (THIS.noiseBit0 & THIS.noiseLevel);
            dout += (a != 0) ? __volTab[THIS.toneVol0] : -__volTab[THIS.toneVol0];
        }
        if ((THIS.toneBit1 | THIS.noiseBit1) != 0) {
            BYTE a = (THIS.toneBit1 & THIS.toneLevel1) | (THIS.noiseBit1 & THIS.noiseLevel);
            dout += (a != 0) ? __volTab[THIS.toneVol1] : -__volTab[THIS.toneVol1];
        }
        if ((THIS.toneBit2 | THIS.noiseBit2) != 0) {
            BYTE a = (THIS.toneBit2 & THIS.toneLevel2) | (THIS.noiseBit2 & THIS.noiseLevel);
            dout += (a != 0) ? __volTab[THIS.toneVol2] : -__volTab[THIS.toneVol2];
        }
        // clamp para 16 bits unsigned
        if (dout < 0) dout = 0;
        if (dout > 65535) dout = 65535;
        // extrai 8 bits altos para o DAC (0..255)
        uint8_t dacVal = (uint8_t)(dout >> 8);
        dac_output_voltage(DAC_CHANNEL_1, dacVal);   // DAC1 = GPIO25
    }
}

// Callback do timer - chamado a cada 1/SOUND_FREQ segundos (24kHz = ~42us)
// Roda em ISR de hardware real - imune a starvation por outras tasks/timers.
static void IRAM_ATTR _audioTimerCallback() {
    portENTER_CRITICAL_ISR(&s_audioMux);
    _ym2149DoOneSample();
    portEXIT_CRITICAL_ISR(&s_audioMux);
}

void _ym2149I2SInit() {
    Serial.println("[AUDIO] Init: DAC direto (nao I2S)");
    gpio_reset_pin(GPIO_NUM_25);
    dac_output_enable(DAC_CHANNEL_1);        // GPIO25
    dac_output_voltage(DAC_CHANNEL_1, 128);  // centro (silencio)

    // Ainda alocamos m_sampleBuffer para compatibilidade com o resto do codigo
    THIS.m_play = false;
    THIS.m_sampleRate = SOUND_FREQ;
    THIS.m_sampleBuffer = (uint16_t*) heap_caps_malloc(SOUND_SAMPLE_BUFFER_SIZE * sizeof(uint16_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    Serial.println("[AUDIO] DAC ready on GPIO25");
}

// Compatibilidade: era usado no modo I2S mas agora nao faz nada
bool _ym2149I2SActualPlaying() {
    return s_audioTimer != nullptr;
}

bool _ym2149I2SForcePlay(bool value) {
    if (value) {
        if (s_audioTimer == nullptr) {
            // Hardware timer 0, prescaler 80 (80MHz/80 = 1MHz = 1us ticks),
            // countUp=true. Alarme a cada (1000000/PSG_FREQ) ticks = 16us @ 62.5kHz.
            // Timer roda na taxa do PSG real (62.5kHz), NAO na taxa de sample.
            // O sample eh gerado internamente a cada ~2.6 ticks (via cyclesB),
            // resultando em SOUND_FREQ (24kHz) no DAC. Se o timer rodasse a
            // SOUND_FREQ, a state machine iria 2.6x mais devagar e o som ficaria
            // uma oitava e meia abaixo.
            s_audioTimer = timerBegin(0, 80, true);
            timerAttachInterrupt(s_audioTimer, &_audioTimerCallback, true);
            timerAlarmWrite(s_audioTimer, 1000000 / PSG_FREQ, true);
            timerAlarmEnable(s_audioTimer);
            Serial.printf("[AUDIO] hw_timer @ %d Hz (state machine), sample @ %d Hz\n", PSG_FREQ, SOUND_FREQ);
            THIS.m_state = Playing;
        }
    } else {
        if (s_audioTimer != nullptr) {
            timerAlarmDisable(s_audioTimer);
            timerDetachInterrupt(s_audioTimer);
            timerEnd(s_audioTimer);
            s_audioTimer = nullptr;
            dac_output_voltage(DAC_CHANNEL_1, 128);   // silencio
            THIS.m_state = Stop;
        }
    }
    return value;
}

void _ym2149I2SMutizeOutput() {
    dac_output_voltage(DAC_CHANNEL_1, 128);
}

bool ym2149I2SPlay(bool value) {
    THIS.m_play = value;
    return _ym2149I2SForcePlay(value);
}

void ym2149I2SStop() {
    ym2149I2SPlay(false);
}

int ym2149Delta() {
    int delta = THIS.bufferWritePtr - THIS.bufferReadPtr;
    if (delta < 0) delta += THIS.bufferLength;
    return delta;
}

void ym2149Init(BYTE mode) {
    
    THIS.audioEnable = mode;    // 2 real time, 1 synchronous
    // this.wl = wl;
    THIS.samplerate = SOUND_FREQ;
    THIS.cyclesA = THIS.cyclesB = 0;
    THIS.audioCyclesA = CPU_FREQ/PSG_FREQ;                  // psg frequency is 62500 Hz
    THIS.audioCyclesB = SOUND_FREQ;
    THIS.bufferLength = SOUND_SAMPLE_BUFFER_SIZE * 6;       // 6 buffers ?
    THIS.buffer = (WORD *)heap_caps_malloc(THIS.bufferLength * sizeof(SWORD), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);      // 3 slots sound buffers ?
    THIS.bufferWritePtr = 0;
    THIS.bufferReadPtr = 0;
    THIS.envelopePeriod = 0;
    THIS.envelopeShape = 0;
    THIS.envCount = THIS.envLimit = THIS.envPos = 0;
    THIS.envTab = (BYTE *) __envShapes[THIS.envelopeShape];   // NOT VERY EFFICIENT !!!!
    THIS.rnd = 1;
    THIS.toneBit0 = THIS.toneBit1 = THIS.toneBit2 = 0;
    THIS.toneCount0 = THIS.toneCount1 = THIS.toneCount2 = 0;
    THIS.tonePeriod0 = THIS.tonePeriod1 = THIS.tonePeriod2 = 0;
    THIS.toneLimit0 = THIS.toneLimit1 = THIS.toneLimit2 = 0;
    THIS.toneLevel0 = THIS.toneLevel1 = THIS.toneLevel2 = 0;
    THIS.amplitude0 = THIS.amplitude1 = THIS.amplitude2 = 0;
    THIS.noiseBit0 = THIS.noiseBit1 = THIS.noiseBit2 = 0;
    THIS.noisePeriod = THIS.noiseCount = THIS.noiseLimit = 0;
    //// Atomics.store(this.states, STATE.READ_INDEX, 0);
    //// Atomics.store(this.states, STATE.RING_BUFFER_LENGTH, this.bufferLength);
    //// this.wl.port.postMessage(this.sharedBuffers);       // initialize worklet
    _ym2149I2SInit();

    if (THIS.audioEnable == 2) {
        ym2149I2SPlay(true);
    }
}

BYTE ym2149Write(BYTE data) {
    THIS.regs[THIS.reg] = data;
    if (THIS.reg == 14) return 1;
    if (THIS.audioEnable == 1) ym2149DoAudio();
    switch(THIS.reg) {
        case 1:
            THIS.regs[1] &= 0xF;
        case 0:
            THIS.tonePeriod0 = (THIS.regs[1] << 8) | THIS.regs[0];
            THIS.toneLimit0 = (THIS.tonePeriod0 == 0) ? 0 : THIS.tonePeriod0 - 1;
            // console.log('PSG %04X %08d tone A period %d\n", System.Cpu.PC, psg->cyclesA, psg->tonePeriod[0]);
            break;
        case 3:
            THIS.regs[3] &= 0xF;
        case 2:
            THIS.tonePeriod1 = (THIS.regs[3] << 8) | THIS.regs[2];
            THIS.toneLimit1 = (THIS.tonePeriod1 == 0) ? 0 : THIS.tonePeriod1 - 1;
            break;
        case 5:
            THIS.regs[5] &= 0xF;
        case 4:
            THIS.tonePeriod2 = (THIS.regs[5] << 8) | THIS.regs[4];
            THIS.toneLimit2 = (THIS.tonePeriod2 == 0) ? 0 : THIS.tonePeriod2 - 1;
            break;
        case 6:
            THIS.regs[6] &= 0x1F;
            THIS.noisePeriod = THIS.regs[6];
            THIS.noiseLimit = (THIS.noisePeriod == 0) ? 0 : THIS.noisePeriod - 1;
            // console.log('noiseLimit ' + THIS.noiseLimit);
            break;
        case 7:
            THIS.noiseBit2 = ((data & 0x20) == 0) ? 1 : 0;
            THIS.noiseBit1 = ((data & 0x10) == 0) ? 1 : 0;
            THIS.noiseBit0 = ((data & 0x08) == 0) ? 1 : 0;
            THIS.toneBit2 = ((data & 0x04) == 0) ? 1 : 0;
            THIS.toneBit1 = ((data & 0x02) == 0) ? 1 : 0;
            THIS.toneBit0 = ((data & 0x01) == 0) ? 1 : 0;
            //console.log('Channels ' + (255 - data).toString(16));
            break;
        case 8:
            THIS.regs[8] &= 0x1F;
            THIS.amplitude0 = THIS.regs[8];
            THIS.toneVol0 = ((THIS.amplitude0 & 0x10) != 0) ? THIS.envTab[THIS.envPos] : THIS.amplitude0 & 0x0F;
            break;
        case 9:
            THIS.regs[9] &= 0x1F;
            THIS.amplitude1 = THIS.regs[9];
            THIS.toneVol1 = ((THIS.amplitude1 & 0x10) != 0) ? THIS.envTab[THIS.envPos] : THIS.amplitude1 & 0x0F;
            break;
        case 10:
            THIS.regs[10] &= 0x1F;
            THIS.amplitude2 = THIS.regs[10];
            THIS.toneVol2 = ((THIS.amplitude2 & 0x10) != 0) ? THIS.envTab[THIS.envPos] : THIS.amplitude2 & 0x0F;
            break;
        case 12:
        case 11:
            THIS.envelopePeriod = (THIS.regs[12] << 8) | THIS.regs[11];
            THIS.envLimit = (THIS.envelopePeriod == 0) ? 0 : THIS.envelopePeriod-1;
            // console.log('EP ' + THIS.envelopePeriod);
            break;
        case 13:
            THIS.regs[13] &= 0x0F;
            THIS.envelopeShape = THIS.regs[13];
            THIS.envTab = (BYTE *) __envShapes[THIS.envelopeShape];   // NOT VERY EFFICIENT !!!!
            THIS.envPos = 0;
            if ((THIS.amplitude0 & 0x10) != 0) THIS.toneVol0 = THIS.envTab[0];
            if ((THIS.amplitude1 & 0x10) != 0) THIS.toneVol1 = THIS.envTab[0];
            if ((THIS.amplitude2 & 0x10) != 0) THIS.toneVol2 = THIS.envTab[0];
            break;
    }
    return 0;
}