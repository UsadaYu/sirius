/**
 * @name sirius_math.h
 *
 * @author UsadaYu
 *
 * @date create: 2025-03-29
 * @date update: 2025-03-29
 *
 * @brief Common calculations.
 */

#ifndef __SIRIUS_MATH_H__
#define __SIRIUS_MATH_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @param[in] args_num: The total number of integer
 *  numbers.
 * @param[in] args: A number of integer numbers.
 *
 * @note: The parameter must be of integer type.
 *
 * @return The maximum value in the input parameters.
 */
int sirius_math_max_int(unsigned int args_num, ...);

/**
 * @param[in] args_num: The total number of integer
 *  numbers.
 * @param[in] args: A number of integer numbers.
 *
 * @note: The parameter must be of integer type.
 *
 * @return The minimum value in the input parameters.
 */
int sirius_math_min_int(unsigned int args_num, ...);

/**
 * @param[in] args_num: The total number of double-type
 *  numbers.
 * @param[in] args: A number of double-type numbers.
 *
 * @note: The parameter must be of double-type type.
 *
 * @return The maximum value in the input argument.
 */
double sirius_math_max_dbl(unsigned int args_num, ...);

/**
 * @param[in] args_num: The total number of double-type
 *  numbers.
 * @param[in] args: A number of double-type numbers.
 *
 * @note: The parameter must be of double-type type.
 *
 * @return The minimum value in the input parameters.
 */
double sirius_math_min_dbl(unsigned int args_num, ...);

/**
 * @brief Gets the greater of the two numbers.
 */
#ifndef SIRIUS_MAX_T
#define SIRIUS_MAX_T(num1, num2) \
  ((num1) > (num2) ? (num1) : (num2))
#endif  // SIRIUS_MAX_T

/**
 * @brief Gets the lesser of the two numbers.
 */
#ifndef SIRIUS_MIN_T
#define SIRIUS_MIN_T(num1, num2) \
  ((num1) < (num2) ? (num1) : (num2))
#endif  // SIRIUS_MIN_T

#ifndef SIRIUS_MAX_INT
#define SIRIUS_MAX_INT(args_num, ...) \
  sirius_math_max_int(args_num, __VA_ARGS__)
#endif  // SIRIUS_MAX_INT

#ifndef SIRIUS_MIN_INT
#define SIRIUS_MIN_INT(args_num, ...) \
  sirius_math_min_int(args_num, __VA_ARGS__)
#endif  // SIRIUS_MIN_INT

#ifndef SIRIUS_MAX_DBL
#define SIRIUS_MAX_DBL(args_num, ...) \
  sirius_math_max_dbl(args_num, __VA_ARGS__)
#endif  // SIRIUS_MAX_DBL

#ifndef SIRIUS_MIN_DBL
#define SIRIUS_MIN_DBL(args_num, ...) \
  sirius_math_min_dbl(args_num, __VA_ARGS__)
#endif  // SIRIUS_MIN_DBL

#ifdef __cplusplus
}
#endif

#endif  // __SIRIUS_MATH_H__
