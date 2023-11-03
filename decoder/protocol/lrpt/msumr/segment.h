#pragma once
#include <cstdint>

namespace decoder {
namespace protocol {
namespace lrpt {
namespace msumr {

class Segment {
  public:
    Segment(const uint8_t* data, uint16_t size);

    const uint8_t* getPayloadData() const {
        return mpData + 14;
    }
    uint16_t getPayloadSize() const {
        return mDataSize - 14;
    }
    uint8_t getID() const {
        return mMCUN;
    }
    uint8_t getQF() const {
        return mQF;
    }

    uint16_t getDay() const {
        return mDay;
    }

    uint32_t getMsec() const {
        return mMsec;
    }

    uint16_t getUsec() const {
        return mUsec;
    }

    bool isValid() const {
        // Note: These indexes are not used in meteor and QFM is always 0xFFF0
        return mQT == 0x00 && mDC == 0x00 && mDC == 0x00 && mQFM == 0xFFF0 && ((mDataSize - 14) > 0);
    }


  private:
    const uint8_t* mpData;
    uint16_t mDataSize;
    uint16_t mDay;
    uint32_t mMsec;
    uint16_t mUsec;

    uint8_t mMCUN;
    uint8_t mQT;
    uint8_t mDC;
    uint8_t mAC;
    uint16_t mQFM;
    uint8_t mQF;
};

} // namespace msumr
} // namespace lrpt
} // namespace protocol
} // namespace decoder