// ─── Custom hourglass cursor (22x13, RGBA2222 format) ────────────────────────
// Black (1) = 0xC0, White (0 interior) = 0xFF, Transparent (outside) = 0x00

#define _B  0xC0   // Black (frame and sand)
#define _W  0xFF   // White (empty interior of the hourglass)
#define _T  0x00   // Transparent (outside the hourglass)

static const uint8_t hourglassBitmap[22 * 13] = {
  // Row 0:  111111111111  (all black - top border)
  _B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,
  
  // Row 1:  111111111111  (all black - top border)
  _B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,
  
  // Row 2:  010000000001
  _T,_B,_W,_W,_W,_W,_W,_W,_W,_W,_W,_B,_T,
  
  // Row 3:  010000000001
  _T,_B,_W,_W,_W,_W,_W,_W,_W,_W,_W,_B,_T,
  
  // Row 4:  010000000001
  _T,_B,_W,_W,_W,_W,_W,_W,_W,_W,_W,_B,_T,
  
  // Row 5:  010100000101
  _T,_B,_W,_B,_W,_W,_W,_W,_W,_W,_W,_B,_T,
  
  // Row 6:  010010001001
  _T,_B,_W,_W,_B,_W,_W,_W,_B,_W,_W,_B,_T,
  
  // Row 7:  001001010010
  _T,_T,_B,_W,_W,_B,_W,_B,_W,_W,_B,_T,_T,
  
  // Row 8:  000100100100
  _T,_T,_T,_B,_W,_W,_B,_W,_W,_B,_T,_T,_T,
  
  // Row 9:  000010001000
  _T,_T,_T,_T,_B,_W,_W,_W,_B,_T,_T,_T,_T,
  
  // Row 10: 000001010000
  _T,_T,_T,_T,_T,_B,_W,_B,_T,_T,_T,_T,_T,
  
  // Row 11: 000001010000
  _T,_T,_T,_T,_T,_B,_W,_B,_T,_T,_T,_T,_T,
  
  // Row 12: 000010001000
  _T,_T,_T,_T,_B,_W,_W,_W,_B,_T,_T,_T,_T,
  
  // Row 13: 000100000100
  _T,_T,_T,_B,_W,_W,_W,_W,_W,_B,_T,_T,_T,
  
  // Row 14: 001000100010
  _T,_T,_B,_W,_W,_W,_B,_W,_W,_W,_B,_T,_T,
  
  // Row 15: 010001110001  (accumulated sand)
  _T,_B,_W,_W,_W,_B,_B,_B,_W,_W,_W,_B,_T,
  
  // Row 16: 010011111001
  _T,_B,_W,_W,_B,_B,_B,_B,_B,_W,_W,_B,_T,
  
  // Row 17: 010110111101
  _T,_B,_W,_B,_B,_W,_B,_B,_B,_B,_W,_B,_T,
  
  // Row 18: 010111111101
  _T,_B,_W,_B,_B,_B,_B,_B,_B,_B,_W,_B,_T,
  
  // Row 19: 010000000001
  _T,_B,_W,_W,_W,_W,_W,_W,_W,_W,_W,_B,_T,
  
  // Row 20: 111111111111
  _B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,

  // Row 21: 111111111111
  _B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,_B,
};

#undef _B
#undef _W
#undef _T