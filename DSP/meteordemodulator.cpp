#include "meteordemodulator.h"
#include "global.h"

#include <iostream>
#include <iomanip>

namespace DSP {

MeteorDemodulator::MeteorDemodulator(Mode mode, float symbolRate, float costasBw, uint16_t rrcFilterOrder, bool brokenM2Modulation)
    : mMode(mode)
    , mBorkenM2Modulation(brokenM2Modulation)
    , mSymbolRate(symbolRate)
    , mCostasBw(costasBw)
    , mRrcFilterOrder(rrcFilterOrder)
    , mSamples(nullptr)
    , mProcessedSamples(nullptr)
{
    mSamples = std::make_unique<PLL::complex[]>(STREAM_CHUNK_SIZE);
    mProcessedSamples = std::make_unique<PLL::complex[]>(STREAM_CHUNK_SIZE);

   // OQPSK requires lower bandwidth
    if(mode == Mode::OQPSK) {
        mCostasBw /= 5.0f;
    }
}

MeteorDemodulator::~MeteorDemodulator()
{

}

void MeteorDemodulator::process(IQSoruce &source, MeteorDecoderCallback_t callback)
{
    float pllBandwidth = 2 * M_PI * mCostasBw / mSymbolRate;
    DSP::MeteorCostas costas(pllBandwidth, 0, 0, -M_PI, M_PI, mBorkenM2Modulation);
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
    rrcFilter.process(mSamples.get(), mProcessedSamples.get(), readedSamples);

    if(mMode == Mode::QPSK) {
        while((readedSamples = source.read(mSamples.get(), STREAM_CHUNK_SIZE)) > 0) {
            rrcFilter.process(mSamples.get(), mProcessedSamples.get(), readedSamples);
            mAgc.process(mProcessedSamples.get(), mProcessedSamples.get(), readedSamples);
            costas.process(mProcessedSamples.get(), mProcessedSamples.get(), readedSamples);

            mm.process(readedSamples, mProcessedSamples.get(), [progress, &bytesWrited, callback](MM::complex value) mutable {
                // Append the new samples to the output file
                if(callback != nullptr) {
                    callback(value, progress);
                }
                bytesWrited +=2;
            });
            progress = (source.getReadedSamples() / static_cast<float>(source.getTotalSamples())) * 100;

            float carierFreq = costas.getFrequency() * mSymbolRate / (2 * M_PI);

            std::cout << std::fixed << std::setprecision(2) << " Carrier: " << carierFreq << "Hz\t Error: "<< costas.getError() << "\t OutputSize: " << bytesWrited/1024.0f/1024.0f << "Mb Progress: " <<  progress  << "% \t\t\r" << std::flush;
        }
    } else if(Mode::OQPSK) {

    }

    std::cout << std::endl;
}

} // namespace DSP
