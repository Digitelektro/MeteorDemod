#include "meteordemodulator.h"

#include <iomanip>
#include <iostream>

#include "global.h"

namespace DSP {

MeteorDemodulator::MeteorDemodulator(Mode mode, float symbolRate, float costasBw, uint16_t rrcFilterOrder, bool waitForLock, bool brokenM2Modulation)
    : mMode(mode)
    , mBorkenM2Modulation(brokenM2Modulation)
    , mWaitForLock(waitForLock)
    , mSymbolRate(symbolRate)
    , mCostasBw(costasBw)
    , mRrcFilterOrder(rrcFilterOrder)
    , mAgc(0.5f, 100)
    , mPrevI(0.0f)
    , mSamples(nullptr)
    , mProcessedSamples(nullptr) {
    mSamples = std::make_unique<PLL::complex[]>(STREAM_CHUNK_SIZE);
    mProcessedSamples = std::make_unique<PLL::complex[]>(STREAM_CHUNK_SIZE);
}

MeteorDemodulator::~MeteorDemodulator() {}

void MeteorDemodulator::process(IQSoruce& source, MeteorDecoderCallback_t callback) {
    float pllBandwidth = 2 * M_PI * mCostasBw / mSymbolRate;
    float maxFreqDeviation = 10000.0f * (2.0f * M_PI) / source.getSampleRate(); //+-10kHz
    DSP::MeteorCostas costas(pllBandwidth, 0, 0, -maxFreqDeviation, maxFreqDeviation, mBorkenM2Modulation);
    DSP::RRCFilter rrcFilter(mRrcFilterOrder, 0.6f, mSymbolRate, source.getSampleRate());
    MM mm(source.getSampleRate() / mSymbolRate, 1e-6, 0.01f, 0.01f);
    uint32_t readedSamples;

    uint64_t bytesWrited = 0;
    float progress = 0;

    if(mSamples == nullptr || mProcessedSamples == nullptr) {
        std::cout << "MeteorDecoder memory allocation is failed, skipping process" << std::endl;
        return;
    }

    // Discard the first null samples
    readedSamples = source.read(mSamples.get(), mRrcFilterOrder);
    mAgc.process(mSamples.get(), mProcessedSamples.get(), readedSamples);
    rrcFilter.process(mProcessedSamples.get(), mProcessedSamples.get(), readedSamples);

    while((readedSamples = source.read(mSamples.get(), STREAM_CHUNK_SIZE)) > 0) {
        mAgc.process(mSamples.get(), mProcessedSamples.get(), readedSamples);
        rrcFilter.process(mProcessedSamples.get(), mProcessedSamples.get(), readedSamples);
        costas.process(mProcessedSamples.get(), mProcessedSamples.get(), readedSamples);

        mm.process(readedSamples, mProcessedSamples.get(), [progress, &bytesWrited, callback, &costas, this](MM::complex value) mutable {
            if(mMode == Mode::OQPSK) {
                float temp = value.imag();
                value = MM::complex(value.real(), mPrevI);
                mPrevI = temp;
            }

            // Append the new samples to the output file
            if(callback != nullptr && (!mWaitForLock || costas.isLockedOnce())) {
                callback(value, progress);
            }
            bytesWrited += 2;
        });
        progress = (source.getReadedSamples() / static_cast<float>(source.getTotalSamples())) * 100;

        float carierFreq = costas.getFrequency() / (2 * M_PI) * source.getSampleRate();

        std::cout << std::fixed << std::setprecision(2) << " Carrier: " << carierFreq << "Hz\t Lock detector: " << costas.getError() << "\t isLocked: " << costas.isLocked() << "\t OutputSize: " << bytesWrited / 1024.0f / 1024.0f
                  << "Mb Progress: " << progress << "% \t\t\r" << std::flush;
    }

    std::cout << std::endl;
}

} // namespace DSP
