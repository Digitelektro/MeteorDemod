#ifndef PHASECONTROLLOOP_H
#define PHASECONTROLLOOP_H

#include <complex>

namespace DSP {

class PhaseControlLoop
{
public:
    PhaseControlLoop(float bandWidth, float phase, float minPhase, float maxPhase, float freq, float minFreq, float maxFreq);
    PhaseControlLoop(float alpha, float beta, float phase, float minPhase, float maxPhase, float freq, float minFreq, float maxFreq);

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

private:


private:

};

} //namespace DSP

#endif // PHASECONTROLLOOP_H
