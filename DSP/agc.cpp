#include "agc.h"

namespace DSP {

Agc::Agc(float targetAmplitude, float maxGain, float windowSize, float biasWindowSize)
    : mWindowSize(windowSize)
    , mAvg(targetAmplitude)
    , mGain(1)
    , mMaxGain(maxGain)
    , mTargetAmplitude(targetAmplitude)
    , mBiasWindowSize(biasWindowSize)
    , mBias(0) {}

} // namespace DSP
