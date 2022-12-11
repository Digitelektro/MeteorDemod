#include "pll.h"

namespace DSP {

PLL::PLL(float bandWidth, float initPhase, float initFreq, float minFreq, float maxFreq)
    : PhaseControlLoop(bandWidth, initPhase, -M_PI, M_PI, initFreq, minFreq, maxFreq) {}

} // namespace DSP
