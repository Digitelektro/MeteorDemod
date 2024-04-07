#include "segment.h"

namespace decoder {
namespace protocol {
namespace lrpt {
namespace msumr {

Segment::Segment(const uint8_t* data, uint16_t size)
    : mpData(data)
    , mDataSize(size) {

    if(size >= 14) {
        mDay = static_cast<uint16_t>(data[0]) << 8 | data[1];
        mMsec = static_cast<uint32_t>(data[2]) << 24 | static_cast<uint32_t>(data[3]) << 16 | static_cast<uint32_t>(data[4]) << 8 | data[5];
        mUsec = static_cast<uint16_t>(data[6]) << 8 | data[7];
        mMCUN = data[8];
        mQT = data[9];
        mDC = data[10] & 0xF0 >> 4;
        mAC = data[10] & 0x0F;
        mQFM = static_cast<uint16_t>(data[11]) << 8 | data[12];
        mQF = data[13];
    }
}

} // namespace msumr
} // namespace lrpt
} // namespace protocol
} // namespace decoder