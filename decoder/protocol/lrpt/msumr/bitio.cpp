#include "bitio.h"

namespace decoder {
namespace protocol {
namespace lrpt {
namespace msumr {

BitIOConst::BitIOConst(const uint8_t* bytes, uint32_t size)
    : mpBytes(bytes)
    , mSize(size)
    , mPos(0) {}

uint32_t BitIOConst::peekBits(int n) {
    uint32_t result = 0;
    int bit = 0;
    for(int i = 0; i < n; i++) {
        int p = mPos + i;
        if((p >> 3) < mSize) {
            bit = (mpBytes[p >> 3] >> (7 - (p & 7))) & 1;
        } else {
            bit = 0;
        }
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

} // namespace msumr
} // namespace lrpt
} // namespace protocol
} // namespace decoder