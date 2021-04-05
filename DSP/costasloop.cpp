#include "costasloop.h"
#include <cmath>

namespace DSP {

float CostasLoop::TAN_LOOKUP_TABLE[256];

CostasLoop::CostasLoop(float bandWidth, Mode mode)
    : mBandWidth(bandWidth)
    , mMode(mode)
    , mNcoPhase(0)
    , mNcoFreqency(COSTAS_INIT_FREQ)
    , mDamping(COSTAS_DAMP)
    , mMovingAvg(1)
    , mIsLocked(false)
{
    for (int i = 0; i < 256; i++) {
        TAN_LOOKUP_TABLE[i] = tanhf((i-128.0f));
    }

    recomputeCoeffs(COSTAS_DAMP, mBandWidth);

}

CostasLoop::complex CostasLoop::mix(const complex &sample)
{
    using namespace std::complex_literals;

    complex ncoOut;
    complex retval;

    ncoOut = std::exp(complex(-1.0if * mNcoPhase));
    retval = sample * ncoOut;
    mNcoPhase += mNcoFreqency;

    return retval;
}

float CostasLoop::delta(const CostasLoop::complex &sample, const CostasLoop::complex &cosamp)
{
    float error;

    error = (lutTanh(std::real(sample)) * std::imag(sample)) - (lutTanh(std::imag(cosamp)) * std::real(cosamp));

    return error / 50.0f;
}

void CostasLoop::recomputeCoeffs(float damping, float bw)
{
    float denom;

    denom = 1.0f + 2.0f * damping * bw + bw * bw;
    mAlpha = (4 * damping * bw) / denom;
    mBeta = (4 * bw * bw) / denom;
}

void CostasLoop::correctPhase(float err)
{
    err = floatClamp(err, 1.0f);
    mNcoPhase = std::fmod(mNcoPhase + mAlpha * err, static_cast<float>(2.0 * M_PI));
    mNcoFreqency = mNcoFreqency + mBeta * err;

    mMovingAvg = (mMovingAvg * (AVG_WINSIZE-1) + std::fabs(err)) / AVG_WINSIZE;

    // Detect whether the PLL is locked, and decrease the BW if it is
    if (mMode == OQPSK) {
        if (!mIsLocked && mMovingAvg < 0.86f) {
            recomputeCoeffs(mDamping, mBandWidth / 10.0f);
            mIsLocked = true;
        } else if (mIsLocked && mMovingAvg > 0.9f) {
            recomputeCoeffs(mDamping, mBandWidth);
            mIsLocked = false;
        }
    } else if (mMode == QPSK) {
        if (!mIsLocked && mMovingAvg < 0.77f) {
            recomputeCoeffs(mDamping, mBandWidth / 10.0f);
            mIsLocked = true;
        } else if (mIsLocked && mMovingAvg > 0.82f) {
            recomputeCoeffs(mDamping, mBandWidth);
            mIsLocked = false;
        }
    }


    // Limit frequency to a sensible range
    if (mNcoFreqency <= -FREQ_MAX) {
        mNcoFreqency = -FREQ_MAX/2;
    } else if (mNcoFreqency >= FREQ_MAX) {
        mNcoFreqency = FREQ_MAX/2;
    }
}

} //namespace DSP
