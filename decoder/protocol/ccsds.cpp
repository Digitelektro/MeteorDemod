#include "ccsds.h"

namespace decoder {
namespace protocol {

CCSDS::CCSDS(const uint8_t* data)
    : mpData(data) {
    mVersion = data[0] >> 5;
    mType = (data[0] >> 4) & 0x01;
    mSeconderyHeaderPresent = (data[0] >> 3) & 1;
    mAPID = static_cast<uint16_t>(data[0] & 0x03) << 8 | data[1];
    mSequenceFlag = data[2] >> 6;
    mPacketSequenceCount = static_cast<uint16_t>(data[2] & 0x3F) << 8 | data[3];
    mPacketLength = static_cast<uint16_t>(data[4]) << 8 | data[5];
}

uint8_t CCSDS::getVersion() const {
    return mVersion;
}

bool CCSDS::getType() const {
    return mType;
}

bool CCSDS::isSecondaryHeaderPresent() const {
    return mSeconderyHeaderPresent;
}

uint16_t CCSDS::getAPID() const {
    return mAPID;
}

uint8_t CCSDS::getSequenceFlag() const {
    return mSequenceFlag;
}

uint16_t CCSDS::getgetSequenceCount() const {
    return mPacketSequenceCount;
}

uint16_t CCSDS::getPacketLength() const {
    return mPacketLength + 1; // Packet length in CCSDS is length - 1
}

} // namespace protocol
} // namespace decoder