#include "bitio.h"

BitIOConst::BitIOConst(const uint8_t* bytes)
    : mpBytes(bytes)
    , mPos(0) {}

uint32_t BitIOConst::peekBits(int n) {
    uint32_t result = 0;
    for(int i = 0; i < n; i++) {
        int p = mPos + i;
        int bit = (mpBytes[p >> 3] >> (7 - (p & 7))) & 1;
        result = (result << 1) | bit;
    }
    return result;
}

void BitIOConst::advanceBits(int n) {
    mPos += n;
}

uint32_t BitIOConst::fetchBits(int n) {
    uint32_t result = peekBits(n);
    advanceBits(n);
    return result;
}
