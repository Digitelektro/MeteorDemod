#ifndef FILTER_H
#define FILTER_H

#include <complex>
#include <vector>
#include <list>

namespace DSP {

class FilterBase
{
public:
    typedef std::complex<float> complex;
public:
    FilterBase(int taps);
    virtual ~FilterBase() {}

    inline complex process(const complex &in) {
        complex out = 0.0f;

        mMemory.push_front(in);
        mMemory.pop_back();

        std::list<complex>::const_reverse_iterator it = mMemory.crbegin();

        for(int i = mTaps - 1; i >=0; --i, ++it) {
            out += (*it) * mCoeffs[i];
        }
        return out;
    }

    inline void process(const complex *inSamples, complex *outSamples, unsigned int count) {
        for(unsigned int i = 0; i < count; i++) {
            *outSamples = process(*inSamples++);
            outSamples++;
        }
    }

protected:
    std::vector<float> mCoeffs;
    std::list<complex> mMemory;
    int mTaps;
};

class RRCFilter : public FilterBase {
public:
    RRCFilter(int taps, float beta, float symbolrate, float samplerate);

private:
    void computeCoeffs(int taps, float beta, float Ts);
};

} //namespace DSP

#endif // FILTER_H
