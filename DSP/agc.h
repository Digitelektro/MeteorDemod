#ifndef AGC_H
#define AGC_H

#include <complex>

namespace DSP {

class Agc {
  public:
    typedef std::complex<float> complex;

  public:
    Agc(float targetAmplitude, float maxGain = 20, float windowSize = 1024 * 64, float biasWindowSize = 256 * 1024);

    inline complex process(complex sample) {
        float rho;

        mBias = (mBias * static_cast<float>(mBiasWindowSize - 1) + sample) / static_cast<float>(mBiasWindowSize);
        sample -= mBias;

        // Update the sample magnitude average
        rho = std::abs(sample);
        mAvg = (mAvg * (mWindowSize - 1) + rho) / mWindowSize;

        mGain = mTargetAmplitude / mAvg;
        if(mGain > mMaxGain) {
            mGain = mMaxGain;
        }
        return sample * mGain;
    }

    inline void process(const complex* inSamples, complex* outsamples, unsigned int count) {
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
    float mMaxGain;
    float mTargetAmplitude;
    float mBiasWindowSize;
    complex mBias;
};

} // namespace DSP

#endif // AGC_H
