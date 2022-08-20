#include "meteorcostas.h"

namespace DSP {

MeteorCostas::MeteorCostas(float bandWidth, float initPhase, float initFreq, float minFreq, float maxFreq, bool brokenModulation)
    : PLL(bandWidth, initPhase, initFreq, minFreq, maxFreq)
    , mPllOriginalBandwidth(bandWidth)
    , mBrokenModulation(brokenModulation)
    , mError(0.0f)
    , mLockDetector(0.0f)
    , mIsLocked(false)
    , mIsLockedOnce(false)
{

}

} // namespace DSP
