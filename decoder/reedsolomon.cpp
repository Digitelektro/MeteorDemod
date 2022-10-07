#include "reedsolomon.h"
#include <array>

ReedSolomon::ReedSolomon()
    : mpReedSolomon(nullptr)
{
     mpReedSolomon = correct_reed_solomon_create(cpolynomial, cFirstRoot, cRootGap, cNRoots);
}

ReedSolomon::~ReedSolomon()
{
    correct_reed_solomon_destroy(mpReedSolomon);
}

void ReedSolomon::deinterleave(const uint8_t *data, int pos, int n)
{
    for (int i = 0; i < 255; i++) {
        mWorkBuffer[i] = data[i * n + pos];
    }
}

void ReedSolomon::interleave(uint8_t *output, int pos, int n)
{
    for (int i = 0; i < 255; i++) {
        output[i * n + pos] = mResultBuffer[i];
    }
}

int ReedSolomon::decode()
{
    return correct_reed_solomon_decode(mpReedSolomon, mWorkBuffer, sizeof(mWorkBuffer), mResultBuffer);
}
