#include "meteorcostas.h"

namespace DSP {

MeteorCostas::MeteorCostas(float bandWidth, float initPhase, float initFreq, float minFreq, float maxFreq, bool brokenModulation)
    : PLL(bandWidth, initPhase, initFreq, minFreq, mMaxFreq)
    , mBrokenModulation(brokenModulation)
{

}

} // namespace DSP
