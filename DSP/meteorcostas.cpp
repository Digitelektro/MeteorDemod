#include "meteorcostas.h"

namespace DSP {

MeteorCostas::MeteorCostas(float bandWidth, float initPhase, float initFreq, float minFreq, float maxFreq, bool brokenModulation)
    : PLL(bandWidth, initPhase, initFreq, minFreq, maxFreq)
    , mBrokenModulation(brokenModulation)
    , mError(0.0f)
    , mLockDetector(0.0f)
    , mIsLockedOnce(false)
{

}

} // namespace DSP
