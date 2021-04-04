#include "meteordemodulator.h"
#include <iostream>
#include <iomanip>

namespace DSP {

MeteorDemodulator::MeteorDemodulator(CostasLoop::Mode mode, float symbolRate, uint16_t costasBw, float rrcFilterAlpha, uint16_t rrcFilterOrder, uint16_t interploationFacor)
    : mMode(mode)
      , mSymbolRate(symbolRate)
      , mCostasBw(costasBw)
      , rrcAlpha(rrcFilterAlpha)
      , rrcFilterOrder(rrcFilterOrder)
      , mInterploationFacor(interploationFacor)
      , samples(nullptr)
      , interpolatedSamples(nullptr)
{
    samples = new CostasLoop::complex[CHUNK_SIZE];
    interpolatedSamples = new CostasLoop::complex[CHUNK_SIZE * rrcFilterOrder];
}

MeteorDemodulator::~MeteorDemodulator()
{
    delete[] samples;
    delete[] interpolatedSamples;
}

void MeteorDemodulator::process(IQSoruce &source, MeteorDecoderCallback_t callback)
{
    float pllBandwidth = 2 * M_PI * mCostasBw / mSymbolRate;
    DSP::CostasLoop pll(pllBandwidth, mMode);
    DSP::RRCFilter rrcFilter(rrcFilterOrder, mInterploationFacor, source.getSampleRate()/mSymbolRate, rrcAlpha);
    uint32_t readedSamples;

    IQSoruce::complex before = 0;
    IQSoruce::complex mid = 0;
    IQSoruce::complex cur = 0;
    float resyncOffset = 0;
    float resyncError;
    float resyncPeriod = (source.getSampleRate() * mInterploationFacor) / mSymbolRate;
    bool writeStarted = false;
    uint64_t bytesWrited = 0;
    float progress = 0;

    if(samples == nullptr || interpolatedSamples == nullptr) {
        std::cout << "MeteorDecoder memory allocation is failed, skipping process" << std::endl;
        return;
    }

    // Discard the first null samples
    readedSamples = source.read(samples, rrcFilterOrder);
    interpolator(rrcFilter, samples, readedSamples, mInterploationFacor, interpolatedSamples);

    while((readedSamples = source.read(samples, CHUNK_SIZE)) > 0) {
        interpolator(rrcFilter, samples, readedSamples, mInterploationFacor, interpolatedSamples);
        for(uint32_t i = 0; i < readedSamples * mInterploationFacor; i++) {

            // symbol timing recovery (Gardner)
            if ((resyncOffset >= (resyncPeriod / 2.0f)) && (resyncOffset < (resyncPeriod / 2.0f + 1.0f))) {
                mid = mAgc.process(interpolatedSamples[i]);
            } else if (resyncOffset >= resyncPeriod) {
                cur = mAgc.process(interpolatedSamples[i]);
                resyncOffset -= resyncPeriod;
                resyncError = (std::imag(cur) - std::imag(before)) * std::imag(mid);
                resyncOffset += resyncError * resyncPeriod / 2000000.0f;
                before = cur;

                // Costas loop frequency/phase tuning
                cur = pll.mix(cur);
                pll.correctPhase(pll.delta(cur, cur));

                if(pll.isLocked()) {
                    writeStarted = true;
                }

                progress = (source.getReadedSamples() / static_cast<float>(source.getTotalSamples())) * 100;

                // Append the new samples to the output file
                if(writeStarted) {
                    if(callback != nullptr) {
                        callback(cur, progress);
                    }
                    bytesWrited +=2;
                }


            }
            resyncOffset++;
        }

        float carierFreq = pll.getFrequency() * mSymbolRate / (2 * M_PI);

        std::cout << " lock: " << pll.isLocked() << std::fixed << std::setprecision(2) << " Carrier: " << carierFreq << "Hz\t OutputSize: " << bytesWrited/1024.0f/1024.0f << "Mb Progress: " <<  progress  << "% \t\t\r" << std::flush;
    }
    std::cout << std::endl;
}


void MeteorDemodulator::interpolator(FilterBase &filter, CostasLoop::complex *inSamples, int inSamplesCount, int factor, CostasLoop::complex *outSamples)
{
    uint32_t outSamplesCount = inSamplesCount * factor;

    for (uint32_t i = 0; i < outSamplesCount; i++) {
        outSamples[i] = filter.process(inSamples[i/factor]);
    }
}

} // namespace DSP
