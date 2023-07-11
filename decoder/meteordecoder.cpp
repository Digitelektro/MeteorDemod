#include "meteordecoder.h"


MeteorDecoder::MeteorDecoder(bool deInterleave, bool differentialDecode)
    : mDeInterleave(deInterleave)
    , mDifferentialDecode(differentialDecode)
    , mCorrelation(differentialDecode ? sSynchWordOQPSK : sSynchWordQPSK) {}

size_t MeteorDecoder::decode(uint8_t* softBits, size_t length) {
    size_t decodedPacketCounter = 0;
    size_t syncWordFound = 0;

    if(mDeInterleave) {
        std::cout << "Deinterleaving..." << std::endl;
        uint64_t outLen = 0;
        DeInterleaver::deInterleave(softBits, length, &outLen);
        length = outLen;
    }

    mCorrelation.correlate(softBits, length, [&softBits, length, &decodedPacketCounter, &syncWordFound, this](Correlation::CorellationResult correlationResult, Correlation::PhaseShift phaseShift) {
        bool packetOk;
        uint32_t processedBits = 0;

        do {
            syncWordFound++;

            if(length - (correlationResult.pos + processedBits) < 16384) {
                return processedBits;
            }

            memcpy(mDataTodecode, &softBits[correlationResult.pos + processedBits], 16384);

            mCorrelation.rotateSoftIqInPlace(mDataTodecode, 16384, phaseShift);

            mViterbi.decodeSoft(mDataTodecode, mViterbiResult, 16384);

            if(mDifferentialDecode) {
                differentialDecode(mViterbiResult, 1024);
            }

            uint32_t last_sync_ = *reinterpret_cast<uint32_t*>(mViterbiResult);

            for(int j = 0; j < 1024 - 4; j++) {
                mViterbiResult[j + 4] = mViterbiResult[j + 4] ^ PRAND[j % 255];
            }

            if(mViterbiResult[9] == 0xFF) {
                for(int i = 0; i < 1024; i++) {
                    mViterbiResult[i] ^= 0xFF;
                }
            }

            for(int i = 0; i < 4; i++) {
                mReedSolomon.deinterleave(mViterbiResult + 4, i, 4);
                rsResult[i] = mReedSolomon.decode();
                mReedSolomon.interleave(mDecodedPacket, i, 4);
            }

            std::cout << "SyncWordFound:" << syncWordFound << " | Decoded Packets:" << decodedPacketCounter << " | Current Pos:" << (correlationResult.pos + processedBits) << " | Phase:" << phaseShift << " | synch:" << std::hex
                      << last_sync_ << " | RS: (" << std::dec << rsResult[0] << ", " << rsResult[1] << ", " << rsResult[2] << ", " << rsResult[3] << ")"
                      << "\t\t\r";

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

void MeteorDecoder::differentialDecode(uint8_t* data, int64_t len) {
    uint8_t mask;
    uint8_t lastBit = 0;
    for(int i = 0; i < len; i++) {
        mask = ((data[i] >> 1) & 0x7F) | (lastBit << 7);
        lastBit = data[i] & 1;
        data[i] ^= mask;
    }
}