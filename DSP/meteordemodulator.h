#ifndef METEORDEMODULATOR_H
#define METEORDEMODULATOR_H

#include <functional>
#include "iqsource.h"
#include "costasloop.h"
#include "filter.h"
#include "agc.h"

namespace DSP {

class MeteorDemodulator
{
public:
    typedef std::function<void(const CostasLoop::complex&, float progress)> MeteorDecoderCallback_t;

public:
    MeteorDemodulator(CostasLoop::Mode mode, float symbolRate, bool waitForLock = true, uint16_t costasBw = 100, uint16_t rrcFilterOrder = 64, uint16_t interploationFacor = 4, float rrcFilterAlpha = 0.6f);
    ~MeteorDemodulator();

    MeteorDemodulator &operator=(const MeteorDemodulator &) = delete;
    MeteorDemodulator(const MeteorDemodulator &) = delete;
    MeteorDemodulator &operator=(MeteorDemodulator &&) = delete;
    MeteorDemodulator(MeteorDemodulator &&) = delete;

    void process(IQSoruce &source, MeteorDecoderCallback_t callback);

private:
    void interpolator(FilterBase &filter, CostasLoop::complex *inSamples, int inSamplesCount, int factor, CostasLoop::complex *outSamples);

private:
    CostasLoop::Mode mMode;
    float mSymbolRate;
    bool mWaitForLock;
    uint16_t mCostasBw;
    uint16_t rrcFilterOrder;
    uint16_t mInterploationFacor;
    float rrcAlpha;
    Agc mAgc;
    CostasLoop::complex *samples;
    CostasLoop::complex *interpolatedSamples;

private:
    static constexpr uint32_t CHUNK_SIZE = 8192;
};

} // namespace DSP

#endif // METEORDEMODULATOR_H
