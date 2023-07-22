#include "filter.h"

#include <cmath>

namespace DSP {

FilterBase::FilterBase(int taps)
    : mTaps(taps) {}

RRCFilter::RRCFilter(int taps, float beta, float symbolrate, float samplerate)
    : FilterBase(taps) {
    computeCoeffs(taps, beta, samplerate / symbolrate);
    mMemory.resize(mTaps, 0);
}

void RRCFilter::computeCoeffs(int taps, float beta, float Ts) {
    float coeff;
    float half = taps / 2.0f;
    float limit = Ts / (4.0 * beta);
    for(int i = 0; i < taps; i++) {
        float t = (float)i - half + 0.5;
        if(t == 0.0) {
            coeff = (1.0f + beta * (4.0f / M_PI - 1.0f)) / Ts;
        } else if(t == limit || t == -limit) {
            coeff = ((1.0f + 2.0f / M_PI) * sinf(M_PI / (4.0f * beta)) + (1.0f - 2.0f / M_PI) * cos(M_PI / (4.0f * beta))) * beta / (Ts * sqrtf(2.0f));
        } else {
            coeff = ((sinf((1.0f - beta) * M_PI * t / Ts) + cosf((1.0f + beta) * M_PI * t / Ts) * 4.0f * beta * t / Ts) / ((1.0f - (4.0f * beta * t / Ts) * (4.0f * beta * t / Ts)) * M_PI * t / Ts)) / Ts;
        }
        mCoeffs.emplace_back(coeff);
    }
}

} // namespace DSP
