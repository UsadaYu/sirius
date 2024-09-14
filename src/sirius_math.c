#include "sirius_math.h"

#include "./internal/sirius_internal_sys.h"

inline int
sirius_math_max_int(unsigned int args_num, ...)
{
    va_list args;
    va_start(args, args_num);

    int max_val = INT_MIN;
    for (unsigned int i = 0; i < args_num; i++) {
        int arg = va_arg(args, int);
        max_val = arg > max_val ? arg : max_val;
    }

    va_end(args);
    return max_val;
}

inline int
sirius_math_min_int(unsigned int args_num, ...)
{
    va_list args;
    va_start(args, args_num);

    int min_val = INT_MAX;
    for (unsigned int i = 0; i < args_num; i++) {
        int arg = va_arg(args, int);
        min_val = arg < min_val ? arg : min_val;
    }

    va_end(args);
    return min_val;
}

inline double
sirius_math_max_dbl(unsigned int args_num, ...)
{
    va_list args;
    va_start(args, args_num);

    double max_val = DBL_MIN;
    for (unsigned int i = 0; i < args_num; i++) {
        double arg = va_arg(args, double);
        max_val = arg > max_val ? arg : max_val;
    }

    va_end(args);
    return max_val;
}

inline double
sirius_math_min_dbl(unsigned int args_num, ...)
{
    va_list args;
    va_start(args, args_num);

    double min_val = DBL_MAX;
    for (unsigned int i = 0; i < args_num; i++) {
        double arg = va_arg(args, double);
        min_val = arg < min_val ? arg : min_val;
    }

    va_end(args);
    return min_val;
}
