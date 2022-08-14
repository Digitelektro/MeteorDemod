#ifndef DSP_TAPS_WINDOWEDSINC_H
#define DSP_TAPS_WINDOWEDSINC_H

#include <complex>
#include <vector>

namespace DSP {
namespace TAPS {

    inline double sinc(double x) {
        return (x == 0.0) ? 1.0 : (sin(x) / x);
    }
    inline double hzToRads(double freq, double samplerate) {
        return 2.0 * M_PI * (freq / samplerate);
    }

    template<class T, typename Func>
    inline std::vector<T> windowedSinc(int count, double omega, Func window, double norm = 1.0) {
        std::vector<T> taps;
        taps.resize(count);

        // Generate using window
        double half = (double)count / 2.0;
        double corr = norm * omega / M_PI;

        for (int i = 0; i < count; i++) {
            double t = (double)i - half + 0.5;
            if constexpr (std::is_same_v<T, float>) {
                taps[i] = sinc(t * omega) * window(t - half, count) * corr;
            }
            if constexpr (std::is_same_v<T, std::complex<T>>) {
                std::complex<T> cplx = { (float)sinc(t * omega), 0.0f };
                taps[i] = cplx * window(t - half, count) * corr;
            }
        }

        return taps;
    }

    template<class T, typename Func>
    inline std::vector<T> windowedSinc(int count, double cutoff, double samplerate, Func window, double norm = 1.0) {
        return windowedSinc<T>(count, hzToRads(cutoff, samplerate), window, norm);
    }

} // namespace TAPS
} // namespace DSP

#endif // DSP_TAPS_WINDOWEDSINC_H
