#include "mm.h"

#include <algorithm>
#include <numeric>

#include "global.h"
#include "window.h"
#include "windowedsinc.h"

namespace DSP {

MM::MM(float omega, float omegaGain, float muGain, float omegaRelLimit, int interpPhaseCount, int interpTapCount)
    : mInterpPhaseCount(interpPhaseCount)
    , mInterpTapCount(interpTapCount)
    , mPcl(muGain, omegaGain, 0.0f, 0.0f, 1.0f, omega, omega * (1.0f - omegaRelLimit), omega * (1.0f + omegaRelLimit), false)
    , mp0T(0.0f, 0.0f)
    , mp1T(0.0f, 0.0f)
    , mp2T(0.0f, 0.0f)
    , mc0T(0.0f, 0.0f)
    , mc1T(0.0f, 0.0f)
    , mc2T(0.0f, 0.0f)
    , mOffset(0)
    , mBuffer(new complex[interpTapCount + (STREAM_CHUNK_SIZE)])
    , mpBufStart(&mBuffer[interpTapCount - 1])
    , mInterpBank() {
    std::fill(mBuffer.get(), mBuffer.get() + (interpTapCount + STREAM_CHUNK_SIZE), 0);
    generateInterpTaps();
}

int MM::process(int count, const complex* in, std::function<void(complex)> callback) {
    // Copy data to work buffer
    std::copy(in, in + count, mpBufStart);

    // Process all samples
    int outCount = 0;
    while(mOffset < count) {
        float error;
        complex outVal;

        // Calculate new output value
        int phase = std::clamp<int>(floorf(mPcl.getPhase() * (float)mInterpPhaseCount), 0, mInterpPhaseCount - 1);
        outVal = mInterpBank.process(&mBuffer[mOffset], phase);
        callback(outVal);

        // Calculate symbol phase error
        // Propagate delay
        mp2T = mp1T;
        mp1T = mp0T;
        mc2T = mc1T;
        mc1T = mc0T;

        // Update the T0 values
        mp0T = outVal;
        mc0T = step(outVal);

        // Error
        error = (((mp0T - mp2T) * std::conj(mc1T)) - ((mc0T - mc2T) * std::conj(mp1T))).real();

        // Clamp symbol phase error
        error = std::clamp(error, -1.0f, 1.0f);

        // Advance symbol mOffset and phase
        mPcl.advance(error);
        float delta = floorf(mPcl.mPhase);
        mOffset += delta;

        mPcl.mPhase -= delta;
    }
    mOffset -= count;

    // Update delay buffer
    // memmove(mpBuffer, &mpBuffer[count], (mInterpTapCount - 1) * sizeof(complex));
    std::move(&mBuffer[count], &mBuffer[count + mInterpTapCount], mBuffer.get());

    return outCount;
}

void MM::generateInterpTaps() {
    double bw = 0.5 / (double)mInterpPhaseCount;
    std::vector<float> lp = DSP::TAPS::windowedSinc<float>(mInterpPhaseCount * mInterpTapCount, DSP::TAPS::hzToRads(bw, 1.0), DSP::WINDOW::nuttall, mInterpPhaseCount);
    mInterpBank.buildPolyphaseBank(mInterpPhaseCount, lp.data(), lp.size());
}

} // namespace DSP
