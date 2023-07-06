#ifndef DSP_METEORCOSTAS_H
#define DSP_METEORCOSTAS_H

#include <algorithm>

#include "pll.h"

namespace DSP {

class MeteorCostas : public PLL {
  private:
    static constexpr float cLockDetectionTreshold = 0.18;
    static constexpr float cUnLockDetectionTreshold = 0.22;
    static constexpr float cLockFilterCoeff = 0.00001f;

  public:
    enum Mode { QPSK, OQPSK };

  public:
    MeteorCostas(Mode mode, float bandWidth, float initPhase = 0.0f, float initFreq = 0.0f, float minFreq = -M_PI, float maxFreq = M_PI, bool brokenModulation = false);

    inline virtual complex process(const complex& sample) override {
        complex retval;
        retval = sample * complex(cosf(-mPhase), sinf(-mPhase));
        mError = errorFunction(retval);
        advance(mError);

        if(mMode == Mode::OQPSK) {
            float temp = retval.imag();
            retval = complex(retval.real(), mPrevI);
            mPrevI = temp;
        }
        return retval;
    }

    inline virtual void process(const complex* insamples, complex* outsampes, unsigned int count) {
        for(unsigned int i = 0; i < count; i++) {
            *outsampes++ = process(*insamples++);
        }
    }

  protected:
    float errorFunction(complex value) {
        float error;
        if(mBrokenModulation) {
            const float PHASE1 = 0.47439988279190737;
            const float PHASE2 = 2.1777839908413044;
            const float PHASE3 = 3.8682349942715186;
            const float PHASE4 = -0.29067248091319986;

            float phase = atan2f(value.imag(), value.real());
            float dp1 = normalizePhase(phase - PHASE1);
            float dp2 = normalizePhase(phase - PHASE2);
            float dp3 = normalizePhase(phase - PHASE3);
            float dp4 = normalizePhase(phase - PHASE4);
            float lowest = dp1;
            if(fabsf(dp2) < fabsf(lowest)) {
                lowest = dp2;
            }
            if(fabsf(dp3) < fabsf(lowest)) {
                lowest = dp3;
            }
            if(fabsf(dp4) < fabsf(lowest)) {
                lowest = dp4;
            }
            error = lowest * std::abs(value);
        } else {
            error = (step(value.real()) * value.imag()) - (step(value.imag()) * value.real());
        }

        mLockDetector = std::abs(error) * cLockFilterCoeff + mLockDetector * (1.0f - cLockFilterCoeff);

        if(mLockDetector < cLockDetectionTreshold && !mIsLocked) {
            mIsLocked = true;
            setBandWidth(mPllOriginalBandwidth / 5.0f);
        } else if(mLockDetector > cUnLockDetectionTreshold && mIsLocked) {
            mIsLocked = false;
            setBandWidth(mPllOriginalBandwidth);
        }

        if(mLockDetector < cLockDetectionTreshold) {
            mIsLockedOnce = true;
        }

        return std::clamp(error, -1.0f, 1.0f);
    }

    inline float step(float val) {
        return val > 0 ? 1.0f : -1.0f;
    }

  public:
    inline float getError() const {
        return mLockDetector;
    }
    inline bool isLocked() const {
        return mIsLocked;
    }
    inline bool isLockedOnce() const {
        return mIsLockedOnce;
    }

  protected:
    Mode mMode;
    float mPllOriginalBandwidth;
    bool mBrokenModulation;
    float mError;
    float mLockDetector;
    bool mIsLocked;
    bool mIsLockedOnce;
    float mPrevI;
};

} // namespace DSP

#endif // DSP_METEORCOSTAS_H
