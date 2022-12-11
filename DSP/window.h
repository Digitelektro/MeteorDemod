#ifndef DSP_WINDOW_H
#define DSP_WINDOW_H

#include <math.h>

namespace DSP {

namespace WINDOW {

inline double cosine(double n, double N, const double* coefs, int coefCount) {
    // assert(coefCount > 0);
    double win = 0.0;
    double sign = 1.0;
    for(int i = 0; i < coefCount; i++) {
        win += sign * coefs[i] * cos((double)i * 2.0 * M_PI * n / N);
        sign = -sign;
    }
    return win;
}

inline double nuttall(double n, double N) {
    const double coefs[] = {0.355768, 0.487396, 0.144232, 0.012604};
    return cosine(n, N, coefs, sizeof(coefs) / sizeof(double));
}
} // namespace WINDOW

} // namespace DSP

#endif // DSP_WINDOW_H
