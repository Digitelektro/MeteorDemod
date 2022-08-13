#ifndef METEORDEMODULATOR_H
#define METEORDEMODULATOR_H

#include <functional>
#include "iqsource.h"
#include "meteorcostas.h"
#include "filter.h"
#include "agc.h"
#include "mm.h"

namespace DSP {

class MeteorDemodulator
{
public:
    typedef std::function<void(const PLL::complex&, float progress)> MeteorDecoderCallback_t;

    enum Mode {
        QPSK,
        OQPSK
    };

public:
    MeteorDemodulator(Mode mode, float symbolRate, float costasBw = 100.0f, uint16_t rrcFilterOrder = 64, bool waitForLock = true, bool brokenM2Modulation = false);
    ~MeteorDemodulator();

    MeteorDemodulator &operator=(const MeteorDemodulator &) = delete;
    MeteorDemodulator(const MeteorDemodulator &) = delete;
    MeteorDemodulator &operator=(MeteorDemodulator &&) = delete;
    MeteorDemodulator(MeteorDemodulator &&) = delete;

    void process(IQSoruce &source, MeteorDecoderCallback_t callback);

private:
    Mode mMode;
    bool mBorkenM2Modulation;
    bool mWaitForLock;
    float mSymbolRate;
    float mCostasBw;
    uint16_t mRrcFilterOrder;
    Agc mAgc;
    float mPrevI;
    std::unique_ptr<PLL::complex[]> mSamples;
    std::unique_ptr<PLL::complex[]> mProcessedSamples;
};

} // namespace DSP

#endif // METEORDEMODULATOR_H
