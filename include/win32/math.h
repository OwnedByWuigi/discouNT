#ifndef DISCOUNT_MATH_H
#define DISCOUNT_MATH_H

static inline double fabs(double x) {
    return (x < 0.0) ? -x : x;
}

static inline double log10(double x) {
    double value = fabs(x);
    double result = 0.0;
    if (value <= 0.0) return 0.0;
    while (value >= 10.0) {
        value /= 10.0;
        result += 1.0;
    }
    while (value > 0.0 && value < 1.0) {
        value *= 10.0;
        result -= 1.0;
    }
    return result;
}

#endif
