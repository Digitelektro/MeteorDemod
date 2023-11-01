#include "decoder.h"

#include <iostream>

#include "msumr/segment.h"
#include "protocol/vcdu.h"

namespace decoder {
namespace protocol {
namespace lrpt {

void Decoder::process(const uint8_t* cadu) {
    VCDU vcdu(cadu);

    if(vcdu.getVersion() == 0) {
        return;
    }

    if(vcdu.getVCID() == cVCIDAVHRR) {
        cadu += 4; // Sync word
        cadu += VCDU::size();

        uint16_t firstHeaderPointer = static_cast<uint16_t>(cadu[0] & 0b111) << 8 | cadu[1];
        cadu += 2; // remove MPDU Header, now ptr points to MPDU Data

        if(firstHeaderPointer < cNoHeaderMark && firstHeaderPointer >= cPDUDataSize) {
            return; // invalid packet
        }

        if(vcdu.getCounter() == (mLastFrame + 1)) {
            if(mPartialPacket) {
                if(firstHeaderPointer == cNoHeaderMark) {
                    // Current frame doesn't have header
                    mCcsdsPayload.insert(mCcsdsPayload.end(), cadu, cadu + cPDUDataSize);
                } else {
                    mCcsdsPayload.insert(mCcsdsPayload.end(), cadu, cadu + firstHeaderPointer + 1);
                    parsePartial(mCcsdsPayload.data(), mCcsdsPayload.size());
                    mCcsdsPayload.clear();
                }
            }
        } else {
            // We lost synch, reset state
            mPartialPacket = false;
            mCcsdsPayload.clear();
            if(firstHeaderPointer == cNoHeaderMark) {
                // No header in this packet, cannot start collecting data
                return;
            }
        }

        mLastFrame = vcdu.getCounter();

        cadu += firstHeaderPointer;
        uint16_t dataSize = cPDUDataSize - firstHeaderPointer;
        while(dataSize > 0) {
            int usedBytes = parsePartial(cadu, dataSize);
            if(mPartialPacket) {
                // store partial data for next round
                mCcsdsPayload.insert(mCcsdsPayload.end(), cadu, cadu + dataSize);
                dataSize = 0;
            } else {
                dataSize -= usedBytes;
                cadu += usedBytes;
            }
        }
    }
}

uint16_t Decoder::parsePartial(const uint8_t* data, uint32_t length) {
    if(length < 6) {
        mPartialPacket = true;
        return 0; // no enough bytes to decode header yet
    }

    CCSDS ccsds(data);

    if((ccsds.size()) > length) {
        mPartialPacket = true;
        return 0;
    } else {
        processPacket(ccsds);
        mPartialPacket = false;
    }
    return ccsds.size();
}

void Decoder::processPacket(const CCSDS& ccsds) {
    if(ccsds.getAPID() == 64 || ccsds.getAPID() == 65 || ccsds.getAPID() == 66 || ccsds.getAPID() == 67 || ccsds.getAPID() == 68 || ccsds.getAPID() == 69) {
        msumr::Segment segment(ccsds.packetData(), ccsds.getPacketLength());
        if(segment.isValid()) {
            decode(ccsds.getAPID(), ccsds.getgetSequenceCount(), segment);

            /*DateTime onboardTime(2000, 12, 31);
            onboardTime = onboardTime.AddDays(segment.getDay());
            onboardTime = onboardTime.AddMicroseconds(segment.getMsec() * 1000 + segment.getUsec());
            std::cout << "Onboard time: " << onboardTime << std::endl;*/
        }
    } else if(ccsds.getAPID() == 70) {
        parse70(ccsds);
    }
}

void Decoder::parse70(const CCSDS& ccsds) {
    uint16_t h = ccsds.packetData()[16];
    uint16_t m = ccsds.packetData()[17];
    uint16_t s = ccsds.packetData()[18];
    uint16_t ms = static_cast<uint32_t>(ccsds.packetData()[19]) * 4;

    mLastTimeStamp = TimeSpan(0, h, m, s, ms * 1000);
    mLastHeightAtTimeStamp = getLastY();
    if(mFirstTime) {
        mFirstTime = false;
        mFirstTimeStamp = mLastTimeStamp;
        mFirstHeightAtTimeStamp = getLastY();
    }

    std::cout << "Onboard time: " << h << ":" << m << ":" << s << "." << ms << std::endl;
}

} // namespace lrpt
} // namespace protocol
} // namespace decoder