#ifndef PHASECONTROLLOOP_H
#define PHASECONTROLLOOP_H

#include <complex>

namespace DSP {

class MM;

class PhaseControlLoop
{
    friend class MM;

public:
    PhaseControlLoop(float bandWidth, float phase, float minPhase, float maxPhase, float freq, float minFreq, float maxFreq, bool clampPhase = true);
    PhaseControlLoop(float alpha, float beta, float phase, float minPhase, float maxPhase, float freq, float minFreq, float maxFreq, bool clampPhase = true);

    float normalizePhase(float diff) {
        if (diff > M_PI) {
            diff -= 2.0f * M_PI;
        }
        else if (diff <= -M_PI) {
            diff += 2.0f * M_PI;
        }
        return diff;
    }

    void advance(float err);

public:
    void setBandWidth(float bandWidth) {
        float damping = sqrtf(2.0f) / 2.0f;
        float denom = 1.0f + 2.0f * damping * bandWidth + bandWidth * bandWidth;
        mAlpha = (4 * damping * bandWidth) / denom;
        mBeta = (4 * bandWidth * bandWidth) / denom;
    }

public:
    float getPhase() const {
        return mPhase;
    }
    float getFrequency() const {
        return mFreq;
    }

protected:
    void clampFreq() {
        if (mFreq > mMaxFreq) {
            mFreq = mMaxFreq;
        } else if (mFreq < mMinFreq) {
            mFreq = mMinFreq;
        }
    }

    void clampPhase() {
        while (mPhase > mMaxPhase) {
            mPhase -= mPhaseDelta;
        }
        while (mPhase < mMinPhase) {
            mPhase += mPhaseDelta;
        }
    }

protected:
    float mAlpha;
    float mBeta;
    float mFreq;
    float mPhase;
    float mMinPhase;
    float mMaxPhase;
    float mMinFreq;
    float mMaxFreq;
    float mPhaseDelta;
    bool mClampPhase;
};

} //namespace DSP

#endif // PHASECONTROLLOOP_H
