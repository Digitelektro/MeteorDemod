#include "phasecontrolloop.h"
#include <cmath>

namespace DSP {

PhaseControlLoop::PhaseControlLoop(float bandWidth, float phase, float minPhase, float maxPhase, float freq, float minFreq, float maxFreq, bool clampPhase)
    : mAlpha(0)
    , mBeta(0)
    , mFreq(freq)
    , mPhase(phase)
    , mMinPhase(minPhase)
    , mMaxPhase(maxPhase)
    , mMinFreq(minFreq)
    , mMaxFreq(maxFreq)
    , mPhaseDelta(maxPhase - mMinPhase)
    , mClampPhase(clampPhase)
{
    float damping = sqrtf(2.0f) / 2.0f;
    float denom = 1.0f + 2.0f * damping * bandWidth + bandWidth * bandWidth;
    mAlpha = (4 * damping * bandWidth) / denom;
    mBeta = (4 * bandWidth * bandWidth) / denom;

}

PhaseControlLoop::PhaseControlLoop(float alpha, float beta, float phase, float minPhase, float maxPhase, float freq, float minFreq, float maxFreq, bool clampPhase)
    : mAlpha(alpha)
    , mBeta(beta)
    , mFreq(freq)
    , mPhase(phase)
    , mMinPhase(minPhase)
    , mMaxPhase(maxPhase)
    , mMinFreq(minFreq)
    , mMaxFreq(maxFreq)
    , mPhaseDelta(maxPhase - mMinPhase)
    , mClampPhase(clampPhase)
{

}

void PhaseControlLoop::advance(float error)
{
    // Increment and clamp frequency
    mFreq += mBeta * error;
    clampFreq();

    // Increment and clamp phase
    mPhase += mFreq + (mAlpha * error);
    if(mClampPhase) {
        clampPhase();
    }
}

} //namespace DSP
