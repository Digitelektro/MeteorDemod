#include "meteordemodulator.h"
#include <iostream>
#include <iomanip>

namespace DSP {

MeteorDemodulator::MeteorDemodulator(Mode mode, float symbolRate, float costasBw, uint16_t rrcFilterOrder, uint16_t interploationFacor, float rrcFilterAlpha, bool brokenM2Modulation)
    : mMode(mode)
    , mBorkenM2Modulation(brokenM2Modulation)
    , mSymbolRate(symbolRate)
    , mCostasBw(costasBw)
    , rrcFilterOrder(rrcFilterOrder)
    , mInterploationFacor(interploationFacor)
    , rrcAlpha(rrcFilterAlpha)
    , samples(nullptr)
    , interpolatedSamples(nullptr)
{
    samples = new PLL::complex[CHUNK_SIZE];
    interpolatedSamples = new PLL::complex[CHUNK_SIZE * rrcFilterOrder];

   // OQPSK requires lower bandwidth
    if(mode == Mode::OQPSK) {
        mCostasBw /= 5.0f;
    }
}

MeteorDemodulator::~MeteorDemodulator()
{
    delete[] samples;
    delete[] interpolatedSamples;
}

void MeteorDemodulator::process(IQSoruce &source, MeteorDecoderCallback_t callback)
{
    float pllBandwidth = 2 * M_PI * mCostasBw / mSymbolRate;
    DSP::MeteorCostas costas(pllBandwidth, 0, 0, -M_PI, M_PI, mBorkenM2Modulation);
    DSP::RRCFilter rrcFilter(rrcFilterOrder, mInterploationFacor, source.getSampleRate()/mSymbolRate, rrcAlpha);
    uint32_t readedSamples;

    IQSoruce::complex before = 0;
    IQSoruce::complex mid = 0;
    IQSoruce::complex cur = 0;
    float resyncOffset = 0;
    float resyncError;
    float resyncPeriod = (source.getSampleRate() * mInterploationFacor) / mSymbolRate;
    uint64_t bytesWrited = 0;
    float progress = 0;

    if(samples == nullptr || interpolatedSamples == nullptr) {
        std::cout << "MeteorDecoder memory allocation is failed, skipping process" << std::endl;
        return;
    }

    // Discard the first null samples
    readedSamples = source.read(samples, rrcFilterOrder);
    interpolator(rrcFilter, samples, readedSamples, mInterploationFacor, interpolatedSamples);

    if(mMode == Mode::QPSK) {
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
                    cur = costas.process(cur);

                    progress = (source.getReadedSamples() / static_cast<float>(source.getTotalSamples())) * 100;

                    // Append the new samples to the output file
                    if(callback != nullptr) {
                        callback(cur, progress);
                    }
                    bytesWrited +=2;


                }
                resyncOffset++;
            }

            float carierFreq = costas.getFrequency() * mSymbolRate / (2 * M_PI);

            std::cout << std::fixed << std::setprecision(2) << " Carrier: " << carierFreq << "Hz\t OutputSize: " << bytesWrited/1024.0f/1024.0f << "Mb Progress: " <<  progress  << "% \t\t\r" << std::flush;
        }
    } else {
        using namespace std::complex_literals;
        IQSoruce::complex tmp;
        IQSoruce::complex inphase;
        IQSoruce::complex quad;
        float prevI = 0;

        while((readedSamples = source.read(samples, CHUNK_SIZE)) > 0) {
            interpolator(rrcFilter, samples, readedSamples, mInterploationFacor, interpolatedSamples);
            for(uint32_t i = 0; i < readedSamples * mInterploationFacor; i++) {
                tmp = interpolatedSamples[i];

                // symbol timing recovery (Gardner)
                if ((resyncOffset >= (resyncPeriod / 2.0f)) && (resyncOffset < (resyncPeriod / 2.0f + 1.0f))) {
                    inphase = costas.process(mAgc.process(tmp));
                    mid = prevI + 1.0if * std::imag(inphase);
                    prevI = std::real(inphase);
                } else if (resyncOffset >= resyncPeriod) {
                    quad = costas.process(mAgc.process(tmp));
                    cur = prevI + 1.0if * std::imag(quad);
                    prevI = std::real(quad);

                    resyncOffset -= resyncPeriod;
                    resyncError = (std::imag(quad) - std::imag(before)) * std::imag(mid);
                    resyncOffset += (resyncError * resyncPeriod / 2000000.0f);
                    before = cur;

                    /* Carrier tracking */
                    cur = costas.process(cur);
                    tmp = std::real(inphase) + 1.0if * std::imag(quad);

                    progress = (source.getReadedSamples() / static_cast<float>(source.getTotalSamples())) * 100;

                    if(callback != nullptr) {
                        callback(tmp, progress);
                    }
                    bytesWrited +=2;
                }
                resyncOffset++;
            }

            float carierFreq = costas.getFrequency() * mSymbolRate / (2 * M_PI);

            std::cout << std::fixed << std::setprecision(2) << " Carrier: " << carierFreq << "Hz\t OutputSize: " << bytesWrited/1024.0f/1024.0f << "Mb Progress: " <<  progress  << "% \t\t\r" << std::flush;
        }

    }
    std::cout << std::endl;
}


void MeteorDemodulator::interpolator(FilterBase &filter, PLL::complex *inSamples, int inSamplesCount, int factor, PLL::complex *outSamples)
{
    uint32_t outSamplesCount = inSamplesCount * factor;

    for (uint32_t i = 0; i < outSamplesCount; i++) {
        outSamples[i] = filter.process(inSamples[i/factor]);
    }
}

} // namespace DSP
