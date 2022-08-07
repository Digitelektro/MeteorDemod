#ifndef DSP_POLYPHASEBANK_H
#define DSP_POLYPHASEBANK_H

#include <memory>
#include <vector>
#include <complex.h>

namespace DSP {

template<typename T>
class PolyphaseBank {
public:
    PolyphaseBank()
        : mPhaseCount(0)
        , mTapsSize(0)
        , mTapsPerPhase(0) {

    }

    inline void buildPolyphaseBank(int phaseCount, T *taps, int tapsCount) {
        mPhaseCount = phaseCount;
        mTapsSize = tapsCount;
        mTapsPerPhase = (tapsCount + mPhaseCount - 1) / phaseCount;
        mPases.resize(mPhaseCount);

        // Allocate phases
        for (int i = 0; i < mPhaseCount; i++) {
            mPases[i].resize(mTapsPerPhase);
            std::fill(mPases[i].begin(), mPases[i].end(), 0);
        }

        // Fill phases
        int totTapCount = mPhaseCount * mTapsPerPhase;
        for (int i = 0; i < totTapCount; i++) {
            mPases[(mPhaseCount - 1) - (i % mPhaseCount)][i / mPhaseCount] = (i < mTapsSize) ? taps[i] : 0;
        }
    }

    inline std::complex<T> process(const std::complex<T>* input, int phase) {
        std::complex<T> res = {0, 0};

        if(phase >= mPhaseCount) {
            return res;
        }

        for(auto &element : mPases[phase]) {
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

/*
template<typename T>
struct PolyphaseBank {
    int phaseCount;
    int tapsPerPhase;
    T **phases;
};

template<class T>
    inline PolyphaseBank<T> buildPolyphaseBank(int phaseCount, tap<T>& taps) {
        // Allocate bank
        PolyphaseBank<T> pb;
        pb.phaseCount = phaseCount;
        pb.phases = new T*[phaseCount];

        // Allocate phases
        pb.tapsPerPhase = (taps.size + phaseCount - 1) / phaseCount;
        for (int i = 0; i < phaseCount; i++) {
            pb.phases[i] = new T[pb.tapsPerPhase];
            std::fill(pb.phases[i], pb.phases[i] +  pb.tapsPerPhase, 0);
        }

        // Fill phases
        int totTapCount = phaseCount * pb.tapsPerPhase;
        for (int i = 0; i < totTapCount; i++) {
            pb.phases[(phaseCount - 1) - (i % phaseCount)][i / phaseCount] = (i < taps.size) ? taps.taps[i] : 0;
        }

        // int currentTap = 0;
        // for (int tap = 0; tap < pb.tapsPerPhase; tap++) {
        //     for (int phase = 0; phase < phaseCount; phase++) {
        //         if (currentTap < taps.size) {
        //             pb.phases[(phaseCount - 1) - phase][tap] = taps.taps[currentTap++];
        //         }
        //         else {
        //             pb.phases[(phaseCount - 1) - phase][tap] = 0;
        //         }
        //     }
        // }

        return pb;
    }
*/

} // namespace DSP

#endif // DSP_POLYPHASEBANK_H
