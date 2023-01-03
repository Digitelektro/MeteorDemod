#include "meteordecoder.h"


MeteorDecoder::MeteorDecoder(bool deInterleave, bool differentialDecode)
    : mDeInterleave(deInterleave)
    , mDifferentialDecode(differentialDecode) {}

size_t MeteorDecoder::decode(uint8_t* softBits, size_t length) {
    size_t decodedPacketCounter = 0;

    if(mDeInterleave) {
        std::cout << "Deinterleaving..." << std::endl;
        uint64_t outLen = 0;
        DeInterleaver::deInterleave(softBits, length, &outLen);
        length = outLen;
    }

    if(mDifferentialDecode) {
        std::cout << "Dediffing..." << std::endl;
        differentialDecode(reinterpret_cast<int8_t*>(softBits), length);
    }


    mCorrelation.correlate(softBits, length, [&softBits, length, &decodedPacketCounter, this](Correlation::CorellationResult correlationResult, Correlation::PhaseShift phaseShift) {
        bool packetOk;
        uint32_t processedBits = 0;

        do {
            if(length - (correlationResult.pos + processedBits) < 16384) {
                return processedBits;
            }

            memcpy(mDataTodecode, &softBits[correlationResult.pos + processedBits], 16384);

            mCorrelation.rotateSoftIqInPlace(mDataTodecode, 16384, phaseShift);

            mViterbi.decodeSoft(mDataTodecode, mViterbiResult, 16384);

            uint32_t last_sync_ = *reinterpret_cast<uint32_t*>(mViterbiResult);

            if(Correlation::countBits(last_sync_ ^ 0xE20330E5) < Correlation::countBits(last_sync_ ^ 0x1DFCCF1A)) {
                for(int j = 4; j < 1024; j++) {
                    mViterbiResult[j] = mViterbiResult[j] ^ 0xFF;
                }
                last_sync_ = last_sync_ ^ 0xFFFFFFFF;
            }

            for(int j = 0; j < 1024 - 4; j++) {
                mViterbiResult[j + 4] = mViterbiResult[j + 4] ^ PRAND[j % 255];
            }

            for(int i = 0; i < 4; i++) {
                mReedSolomon.deinterleave(mViterbiResult + 4, i, 4);
                rsResult[i] = mReedSolomon.decode();
                mReedSolomon.interleave(mDecodedPacket, i, 4);
            }

            std::cout << "Pos:" << (correlationResult.pos + processedBits) << " | Phase:" << phaseShift << " | synch:" << std::hex << last_sync_ << " | RS: (" << std::dec << rsResult[0] << ", " << rsResult[1] << ", " << rsResult[2] << ", "
                      << rsResult[3] << ")"
                      << "\t\t\r" << std::flush;

            packetOk = (rsResult[0] != -1) && (rsResult[1] != -1) && (rsResult[2] != -1) && (rsResult[3] != -1);

            if(packetOk) {
                parseFrame(mDecodedPacket, 892);
                decodedPacketCounter++;
                processedBits += 16384;
            }
        } while(packetOk);

        return (processedBits > 0) ? processedBits - 1 : 0;
    });

    std::cout << std::endl;

    return decodedPacketCounter;
}

void MeteorDecoder::differentialDecode(int8_t* data, int64_t len) {
    int a;
    int b;
    int prevA;
    int prevB;

    if(len < 2) {
        return;
    }

    for(int i = 0; i < 32768; i++) {
        mIntSqrtTable[i] = std::round(std::sqrt(i));
    }

    prevA = data[0];
    prevB = data[1];
    data[0] = 0;
    data[1] = 0;
    for(int64_t i = 0; i < (len / 2); i++) {
        a = data[i * 2 + 0];
        b = data[i * 2 + 1];
        data[i * 2 + 0] = mean(a, prevA);
        data[i * 2 + 1] = mean(-b, prevB);
        prevA = a;
        prevB = b;
    }
}