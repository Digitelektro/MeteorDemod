#ifndef BITIO_H
#define BITIO_H

// Based on: https://github.com/artlav/meteor_decoder/blob/master/alib/bitop.pas

#include <stdint.h>

class BitIOConst {
  public:
    BitIOConst(const uint8_t* bytes);

    uint32_t peekBits(int n);
    void advanceBits(int n);
    uint32_t fetchBits(int n);

  private:
    const uint8_t* mpBytes;
    int mPos;
};


#endif // BITIO_H
