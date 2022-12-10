#ifndef DSP_POLYPHASEBANK_H
#define DSP_POLYPHASEBANK_H

#include <complex.h>

#include <memory>
#include <vector>

namespace DSP {

template <typename T>
class PolyphaseBank {
  public:
    PolyphaseBank()
        : mPhaseCount(0)
        , mTapsSize(0)
        , mTapsPerPhase(0) {}

    inline void buildPolyphaseBank(int phaseCount, T* taps, int tapsCount) {
        mPhaseCount = phaseCount;
        mTapsSize = tapsCount;
        mTapsPerPhase = (tapsCount + mPhaseCount - 1) / phaseCount;
        mPases.resize(mPhaseCount);

        // Allocate phases
        for(int i = 0; i < mPhaseCount; i++) {
            mPases[i].resize(mTapsPerPhase);
            std::fill(mPases[i].begin(), mPases[i].end(), 0);
        }

        // Fill phases
        int totTapCount = mPhaseCount * mTapsPerPhase;
        for(int i = 0; i < totTapCount; i++) {
            mPases[(mPhaseCount - 1) - (i % mPhaseCount)][i / mPhaseCount] = (i < mTapsSize) ? taps[i] : 0;
        }
    }

    inline std::complex<T> process(const std::complex<T>* input, int phase) {
        std::complex<T> res = {0, 0};

        if(phase >= mPhaseCount) {
            return res;
        }

        for(auto& element : mPases[phase]) {
            res += (*input++) * element;
        }

        return res;
    }

  private:
    int mPhaseCount;
    int mTapsSize;
    int mTapsPerPhase;
    std::vector<std::vector<T>> mPases;
};

} // namespace DSP

#endif // DSP_POLYPHASEBANK_H
