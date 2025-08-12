#ifndef __CUSTOM_MATH_H__
#define __CUSTOM_MATH_H__

#include <float.h>
#include <limits.h>
#include <stdarg.h>

#include "sirius/sirius_attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

static force_inline int _custom_max_int(
    unsigned int args_num, ...) {
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

static force_inline int _custom_min_int(
    unsigned int args_num, ...) {
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

static force_inline double _custom_max_double(
    unsigned int args_num, ...) {
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

static force_inline double _custom_min_double(
    unsigned int args_num, ...) {
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

#ifdef __cplusplus
}
#endif

#endif  // __CUSTOM_MATH_H__
