#ifndef DSP_METEORCOSTAS_H
#define DSP_METEORCOSTAS_H

#include "pll.h"
#include <algorithm>

namespace DSP {

class MeteorCostas : public PLL
{
public:
    MeteorCostas(float bandWidth, float initPhase = 0.0f, float initFreq = 0.0f, float minFreq = -M_PI, float maxFreq = M_PI, bool brokenModulation = false);

    virtual complex process(const complex &sample) override {
        complex retval;
        retval = sample * complex(cosf(-mPhase), sinf(-mPhase));
        mError = errorFunction(retval);
        advance(mError);
        return retval;
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
            if (fabsf(dp2) < fabsf(lowest)) { lowest = dp2; }
            if (fabsf(dp3) < fabsf(lowest)) { lowest = dp3; }
            if (fabsf(dp4) < fabsf(lowest)) { lowest = dp4; }
            error = lowest * std::abs(value);
        } else {
            error = (step(value.real()) * value.imag()) - (step(value.imag()) * value.real());
        }
        return std::clamp(error, -1.0f, 1.0f);
    }

    inline float step(float val) {
        return val > 0 ? 1.0f : -1.0f;
    }

public:
    float getError() const {
        return mError;
    }

protected:
 bool mBrokenModulation;
 float mError;

};

} // namespace DSP

#endif // DSP_METEORCOSTAS_H
