#pragma once

// Based on: https://github.com/artlav/meteor_decoder/blob/master/alib/bitop.pas

#include <stdint.h>

namespace decoder {
namespace protocol {
namespace lrpt {
namespace msumr {

class BitIOConst {
  public:
    BitIOConst(const uint8_t* bytes, uint32_t size);

    uint32_t peekBits(int n);
    void advanceBits(int n);
    uint32_t fetchBits(int n);

  private:
    const uint8_t* mpBytes;
    uint32_t mSize;
    uint32_t mPos;
};


} // namespace msumr
} // namespace lrpt
} // namespace protocol
} // namespace decoder
