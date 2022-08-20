#ifndef DSP_MM_H
#define DSP_MM_H

#include <functional>
#include "phasecontrolloop.h"
#include "polyphasebank.h"

namespace DSP {

class MM
{
public:
    using complex = std::complex<float>;

public:
    MM(float omega, float omegaGain, float muGain, float omegaRelLimit, int interpPhaseCount = 128, int interpTapCount = 8);
    ~MM() = default;

    int process(int count, const complex *in, std::function<void(complex)> callback);

protected:
    void generateInterpTaps();

private:
    int mInterpPhaseCount;
    int mInterpTapCount;
    PhaseControlLoop mPcl;

    complex mp0T;
    complex mp1T;
    complex mp2T;
    complex mc0T;
    complex mc1T;
    complex mc2T;

    int mOffset;
    std::unique_ptr<complex[]>mBuffer;
    complex *mpBufStart;
    PolyphaseBank<float> mInterpBank;

private:
    static inline complex step(const complex &value) {
         return {(value.real() > 0.0f) ? 1.0f : -1.0f, (value.imag() > 0.0f) ? 1.0f : -1.0f};
    }
};

} // namespace DSP

#endif // DSP_MM_H
