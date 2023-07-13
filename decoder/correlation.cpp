#include "correlation.h"

Correlation::Correlation(uint64_t syncWord, bool oqpsk)
    : mSyncWord(syncWord)
    , mOqpskMode(oqpsk) {
    initKernels();
}

void Correlation::correlate(const uint8_t* softBits, int64_t size, CorrelationCallback callback) {
    CorellationResult result{};

    for(uint32_t i = 0; i < size - 64; i++) {
        for(int n = 0; n < mKernels.size(); n++) {
            int score = 0;
            for(int k = 0; k < mKernels[n].size(); k++) {
                score += hardCorrelate(softBits[i + k], mKernels[n][k]);
            }
            result.pos = score > result.corr ? i : result.pos;
            result.corr = score > result.corr ? score : result.corr;

            if(result.corr >= CORRELATION_LIMIT) {
                i += callback(result, n);
                result.pos = 0;
                result.corr = 0;
                break;
            }
        }
    }
}

void Correlation::initKernels() {
    mKernels.resize(8);

    for(int i = 0; i < mKernels.size(); i++) {
        mKernels[i].resize(64);
        hardToSoft(rotate64(mSyncWord, i), mKernels[i].data());
    }

    if(mOqpskMode) {
        for(int i = 0; i < 8; i++) {
            mKernels.push_back(mKernels[i]);
            delayOQPSK(mKernels[i + 8].data(), mKernels[i + 8].size());
        }
    }
}

uint64_t Correlation::rotate64(uint64_t word, PhaseShift phaseShift) {

    switch(phaseShift) {
        case 0:
            break;
        case 1:
            word = word ^ 0x5555555555555555U;
            break;
        case 2:
            word = word ^ 0xFFFFFFFFFFFFFFFFU;
            break;
        case 3:
            word = word ^ 0xAAAAAAAAAAAAAAAAU;
            break;
        case 4:
            word = swapIQ(word);
            break;
        case 5:
            word = swapIQ(word) ^ 0x5555555555555555U;
            break;
        case 6:
            word = swapIQ(word) ^ 0xFFFFFFFFFFFFFFFFU;
            break;
        case_7:
            word = swapIQ(word) ^ 0xAAAAAAAAAAAAAAAAU;
            break;
        default:
            break;
    }
    return word;
}

void Correlation::rotateSoftIqInPlace(uint8_t* data, uint32_t length, PhaseShift phaseShift) {
    uint8_t b;

    switch(phaseShift) {
        case 0:
        case 8:
            for(uint32_t i = 0; i < length; i++) {
                data[i] ^= 0x7F;
            }
            break;
        case 1:
        case 9:
            for(uint32_t i = 0; i < length; i += 2) {
                data[i + 1] ^= 0xFF;

                data[i] ^= 0x7F;
                data[i + 1] ^= 0x7F;
            }
            break;
        case 2:
        case 10:
            for(uint32_t i = 0; i < length; i++) {
                data[i] ^= 0xFF;
                data[i] ^= 0x7F;
            }
            break;

        case 3:
        case 11:
            for(uint32_t i = 0; i < length; i += 2) {
                data[i] ^= 0xFF;

                data[i] ^= 0x7F;
                data[i + 1] ^= 0x7F;
            }
            break;

        case 4:
        case 12:
            for(uint32_t i = 0; i < length; i += 2) {
                b = data[i];
                data[i] = data[i + 1] ^ 0x7F;
                data[i + 1] = b ^ 0x7F;
            }
            break;

        case 5:
        case 13:
            for(uint32_t i = 0; i < length; i += 2) {
                b = data[i];
                data[i] = data[i + 1] ^ 0xFF;
                data[i + 1] = b;

                data[i] ^= 0x7F;
                data[i + 1] ^= 0x7F;
            }
            break;

        case 6:
        case 14:
            for(uint32_t i = 0; i < length; i += 2) {
                b = data[i];
                data[i] = data[i + 1] ^ 0xFF;
                data[i + 1] = b ^ 0xFF;

                data[i] ^= 0x7F;
                data[i + 1] ^= 0x7F;
            }
            break;

        case 7:
        case 15:
            for(uint32_t i = 0; i < length; i += 2) {
                b = data[i] ^ 0xFF;
                data[i] = data[i + 1];
                data[i + 1] = b;

                data[i] ^= 0x7F;
                data[i + 1] ^= 0x7F;
            }
            break;
    }

    // Correct OQPSK Delay
    if(phaseShift > 7) {
        uint8_t prev = 0;
        for(int i = (length / 2) - 1; i >= 0; i--) {
            uint8_t current = data[i * 2 + 1];
            data[i * 2 + 1] = prev;
            prev = current;
        }
    }
}
