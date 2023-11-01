#pragma once

#include <cstdint>
#include <vector>

namespace decoder {
namespace protocol {

class CCSDS {
  public:
    CCSDS(const uint8_t* data);

  public:
    uint8_t getVersion() const;
    bool getType() const;
    bool isSecondaryHeaderPresent() const;
    uint16_t getAPID() const;
    uint8_t getSequenceFlag() const;
    uint16_t getgetSequenceCount() const;
    uint16_t getPacketLength() const;

    uint16_t size() const {
        return 6 + mPacketLength + 1; // header + length
    }
    uint16_t packetLength() const {
        return mPacketLength + 1;
    }

    const uint8_t* data() const {
        return mpData;
    }
    const uint8_t* packetData() const {
        return mpData + 6; // header
    }

  private:
    const uint8_t* mpData;
    uint8_t mVersion;
    bool mType;
    bool mSeconderyHeaderPresent;
    uint16_t mAPID;
    uint8_t mSequenceFlag;
    uint16_t mPacketSequenceCount;
    uint16_t mPacketLength;
};

} // namespace protocol
} // namespace decoder