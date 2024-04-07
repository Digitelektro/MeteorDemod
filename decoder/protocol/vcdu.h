#pragma once

#include <cstdint>
#include <vector>

namespace decoder {
namespace protocol {

class VCDU {
  public:
    VCDU(const uint8_t* cadu) {
        mVersion = cadu[4] >> 6;
        mSpaceCraftID = static_cast<uint16_t>(cadu[4] & 0b111111) << 2 | cadu[5] >> 6;
        mVCID = cadu[5] & 0x3F;
        mCounter = static_cast<uint32_t>(cadu[6]) << 16 | static_cast<uint32_t>(cadu[7]) << 8 | cadu[8];
        mReplayFlag = cadu[9] >> 7;
        mInsertZone = static_cast<uint32_t>(cadu[10]) << 16 | cadu[11];
    }

  public:
    uint8_t getVersion() const {
        return mVersion;
    }
    uint16_t getSpaceCraftID() const {
        return mSpaceCraftID;
    }
    uint8_t getVCID() const {
        return mVCID;
    }
    uint32_t getCounter() const {
        return mCounter;
    }
    bool getReplayFlag() const {
        return mReplayFlag;
    }

    static constexpr uint32_t size() {
        return 8;
    }

  private:
    uint8_t mVersion;
    uint16_t mSpaceCraftID;
    uint8_t mVCID;
    uint32_t mCounter;
    bool mReplayFlag;
    uint16_t mInsertZone;
};

} // namespace protocol
} // namespace decoder