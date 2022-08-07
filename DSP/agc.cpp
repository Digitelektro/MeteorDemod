#include "agc.h"

namespace DSP {

Agc::Agc()
    : mWindowSize(AGC_WINSIZE)
    , mAvg(AGC_TARGET)
    , mGain(1)
    , mTargetAmplitude(AGC_TARGET)
    , mBias(0)
{

}

} //namespace DSP
