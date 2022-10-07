#ifndef DSP_PLL_H
#define DSP_PLL_H

#include  "phasecontrolloop.h"

namespace DSP {

class PLL : public PhaseControlLoop
{
public:
    typedef std::complex<float> complex;

public:
    PLL(float bandWidth, float initPhase = 0.0f, float initFreq = 0.0f, float minFreq = -M_PI, float maxFreq = M_PI);

    virtual complex process(const complex &sample) {
        complex retval = complex(cosf(mPhase), sinf(mPhase));
        advance(normalizePhase(atan2f(sample.imag(), sample.real()) - mPhase));
        return retval;
    }
};

} // namespace DSP

#endif // DSP_PLL_H
