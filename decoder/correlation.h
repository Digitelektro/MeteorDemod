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

    enum PhaseShift { PhaseShift_0 = 0, PhaseShift_1, PhaseShift_2, PhaseShift_3, PhaseShift_4, PhaseShift_5, PhaseShift_6, PhaseShift_7 };

  public:
    typedef std::function<uint32_t(CorellationResult&, PhaseShift)> CorrelationCallback;

  public:
    Correlation();

    void correlate(const uint8_t* softBits, int64_t size, CorrelationCallback callback);
    uint8_t rotateIQ(uint8_t data, PhaseShift phaseShift);
    uint64_t rotate64(uint64_t word, PhaseShift phaseShift);

  private:
    void initKernels();
    uint32_t hardCorrelate(uint8_t dataByte, uint8_t wordByte);

    inline uint64_t swapIQ(uint64_t word) const {
        uint64_t i = word & 0xaaaaaaaaaaaaaaaa;
        uint64_t q = word & 0x5555555555555555;
        return (i >> 1) | (q << 1);
    }

  private:
    uint8_t mRotateIqTable[256];
    uint8_t mRotateIqTableInverted[256];
    uint8_t mKernelUW0[64];
    uint8_t mKernelUW1[64];
    uint8_t mKernelUW2[64];
    uint8_t mKernelUW3[64];
    uint8_t mKernelUW4[64];
    uint8_t mKernelUW5[64];
    uint8_t mKernelUW6[64];
    uint8_t mKernelUW7[64];

  private:
    static constexpr uint64_t sSynchWord = 0xFCA2B63DB00D9794;
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
