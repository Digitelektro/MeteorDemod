#ifndef CORRELATION_H
#define CORRELATION_H

#include <stdint.h>

#include <functional>

class Correlation {
  public:
    struct CorellationResult {
        uint32_t corr;
        uint32_t pos;
    };

    using PhaseShift = uint16_t;

  public:
    typedef std::function<uint32_t(CorellationResult&, PhaseShift)> CorrelationCallback;

  public:
    Correlation(uint64_t syncWord, bool oqpsk);

    void correlate(const uint8_t* softBits, int64_t size, CorrelationCallback callback);
    uint64_t rotate64(uint64_t word, PhaseShift phaseShift);

  private:
    void initKernels();
    inline uint32_t hardCorrelate(uint8_t dataByte, uint8_t wordByte) const {
        return (dataByte >= 127 & wordByte == 255) | (dataByte < 127 & wordByte == 0);
    }

    inline void hardToSoft(uint64_t UW, uint8_t* const result) {
        for(int i = 0; i < 64; i++) {
            result[i] = (UW >> (64 - i - 1)) & 1 ? 0xFF : 0x00;
        }
    }

    inline static void delayOQPSK(uint8_t* const data, int size) {
        uint8_t last = 0;
        for(int i = 0; i < size / 2; i += 1) {
            uint8_t back = data[i * 2 + 1];
            data[i * 2 + 1] = last;
            last = back;
        }
    }

    inline uint64_t swapIQ(uint64_t word) const {
        uint64_t i = word & 0xaaaaaaaaaaaaaaaa;
        uint64_t q = word & 0x5555555555555555;
        return (i >> 1) | (q << 1);
    }

  private:
    uint64_t mSyncWord;
    bool mOqpskMode;
    std::vector<std::vector<uint8_t>> mKernels;

  private:
    static constexpr uint8_t CORRELATION_LIMIT = 54;

  public:
    static void rotateSoftIqInPlace(uint8_t* data, uint32_t length, PhaseShift phaseShift);

    static int countBits(uint32_t i) {
        i = i - ((i >> 1) & 0x55555555);
        i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
        return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
    }
};

#endif // CORRELATION_H
