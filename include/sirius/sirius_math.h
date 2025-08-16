/**
 * @name sirius_math.h
 *
 * @author UsadaYu
 *
 * @date
 *  Create: 2025-03-29
 *  Update: 2025-07-11
 *
 * @brief Common calculations.
 */

#ifndef SIRIUS_MATH_H
#define SIRIUS_MATH_H

#include "custom/custom_math.h"

#ifdef __cplusplus
extern "C" {
#endif

#define sirius_max(num1, num2) \
  ((num1) > (num2) ? (num1) : (num2))
#define sirius_min(num1, num2) \
  ((num1) < (num2) ? (num1) : (num2))

#define sirius_max_int(args_num, ...) \
  (_custom_max_int(args_num, __VA_ARGS__))
#define sirius_min_int(args_num, ...) \
  (_custom_min_int(args_num, __VA_ARGS__))
#define sirius_max_double(args_num, ...) \
  (_custom_max_double(args_num, __VA_ARGS__))
#define sirius_min_double(args_num, ...) \
  (_custom_min_double(args_num, __VA_ARGS__))

#ifdef __cplusplus
}
#endif

#endif  // SIRIUS_MATH_H
