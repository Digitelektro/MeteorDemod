#ifndef AGC_H
#define AGC_H

#include <complex>

namespace DSP {

class Agc
{
public:
    typedef std::complex<float> complex;

public:
    Agc();

    inline complex process(complex sample){
        float rho;

        mBias = (mBias * static_cast<float>(AGC_BIAS_WINSIZE - 1) + sample) / static_cast<float>(AGC_BIAS_WINSIZE);
        sample -= mBias;

        // Update the sample magnitude average
        rho = std::abs(sample);
        mAvg = (mAvg * (mWindowSize - 1) + rho) / mWindowSize;

        mGain = mTargetAmplitude / mAvg;
        if (mGain > AGC_MAX_GAIN) {
            mGain = AGC_MAX_GAIN;
        }
        return sample * mGain;
    }

    inline void process(const complex *inSamples, complex *outsamples, unsigned int count) {
        for(unsigned int i = 0; i < count; i++) {
            *outsamples = process(*inSamples++);
            outsamples++;
        }
    }

public:
    float getGain() const {
        return mGain;
    }

private:
    uint32_t mWindowSize;
    float mAvg;
    float mGain;
    float mTargetAmplitude;
    complex mBias;

private:
    static constexpr uint32_t AGC_WINSIZE = 1024*64;
    static constexpr uint32_t AGC_TARGET = 180;
    static constexpr uint32_t AGC_MAX_GAIN = 20;
    static constexpr uint32_t AGC_BIAS_WINSIZE = 256*1024;
};

} //namespace DSP

#endif // AGC_H
