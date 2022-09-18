#ifndef REEDSOLOMON_H
#define REEDSOLOMON_H

#include <stdint.h>
extern "C" {
#include "correct.h"
}

class ReedSolomon
{
public:
    ReedSolomon();
    ~ReedSolomon();

    void deinterleave(const uint8_t *data, int pos, int n);
    void interleave(uint8_t *output, int pos, int n);
    int decode();


private:
    correct_reed_solomon *mpReedSolomon;
    uint8_t mWorkBuffer[255];
    uint8_t mResultBuffer[255];

    static constexpr uint16_t cpolynomial = correct_rs_primitive_polynomial_ccsds;
    static constexpr uint8_t cFirstRoot = 112;
    static constexpr uint8_t cRootGap = 11;
    static constexpr uint8_t cNRoots = 32;
};

#endif // REEDSOLOMON_H
