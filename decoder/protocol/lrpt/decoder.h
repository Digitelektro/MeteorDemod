#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "DateTime.h"
#include "TimeSpan.h"
#include "msumr/image.h"
#include "protocol/ccsds.h"

namespace decoder {
namespace protocol {
namespace lrpt {

// http://planet.iitp.ru/retro/index.php?lang=en&page_type=spacecraft&page=meteor_m_n2_structure_2
class Decoder : public msumr::Image {
  public:
    static std::string serialNumberToSatName(uint8_t serialNumber);

  public:
    void process(const uint8_t* cadu);

  public:
    const TimeSpan getFirstTimeStamp() const {
        int64_t pixelTime = (mLastTimeStamp - mFirstTimeStamp).Ticks() / (mLastHeightAtTimeStamp - mFirstHeightAtTimeStamp);
        TimeSpan missingTime(pixelTime * mFirstHeightAtTimeStamp);

        return mFirstTimeStamp.Subtract(missingTime);
    }

    TimeSpan getLastTimeStamp() const {
        int64_t pixelTime = (mLastTimeStamp - mFirstTimeStamp).Ticks() / (mLastHeightAtTimeStamp - mFirstHeightAtTimeStamp);
        int missingPixelsTime = ((getLastY() + 8) - mLastHeightAtTimeStamp);
        TimeSpan missingTime(pixelTime * missingPixelsTime);

        return mLastTimeStamp.Add(missingTime);
    }

    uint8_t getSerialNumber() const {
        return mSerialNumber;
    }

  private:
    uint16_t parsePartial(const uint8_t* data, uint32_t length);
    void processPacket(const CCSDS& ccsds);
    void parse70(const CCSDS& ccsds);

  private:
    bool mHeaderFound = false;
    bool mPartialPacket = false;
    uint32_t mLastFrame = 0;
    std::vector<uint8_t> mCcsdsPayload;

    TimeSpan mFirstTimeStamp{0};
    TimeSpan mLastTimeStamp{0};
    int mFirstHeightAtTimeStamp = 0;
    int mLastHeightAtTimeStamp = 0;
    bool mFirstTime = true;
    uint8_t mSerialNumber = 0;

  private:
    static constexpr uint8_t cVCIDAVHRR = 5;
    static constexpr int cNoHeaderMark = 0x7FF;
    static constexpr uint16_t cPDUDataSize = 882;
};
} // namespace lrpt
} // namespace protocol
} // namespace decoder