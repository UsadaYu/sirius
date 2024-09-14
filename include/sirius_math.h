/**
 * @name sirius_math.h
 * 
 * @author 胡益华
 * 
 * @date 2024-03-29
 * 
 * @brief common calculations
 */

#ifndef __SIRIUS_MATH_H__
#define __SIRIUS_MATH_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @param[in] args_num: the total number of integer numbers
 * @param[in] args: a number of integer numbers
 * 
 * @note: the parameter must be of integer type
 * 
 * @return the maximum value in the input parameters
 */
int sirius_math_max_int(unsigned int args_num, ...);

/**
 * @param[in] args_num: the total number of integer numbers
 * @param[in] args: a number of integer numbers
 * 
 * @note: the parameter must be of integer type
 * 
 * @return the minimum value in the input parameters
 */
int sirius_math_min_int(unsigned int args_num, ...);

/**
 * @param[in] args_num: the total number of double-type numbers
 * @param[in] args: a number of double-type numbers
 * 
 * @note: the parameter must be of double-type type
 * 
 * @return the maximum value in the input argument
 */
double sirius_math_max_dbl(unsigned int args_num, ...);

/**
 * @param[in] args_num: the total number of double-type numbers
 * @param[in] args: a number of double-type numbers
 * 
 * @note: the parameter must be of double-type type
 * 
 * @return the minimum value in the input parameters
 */
double sirius_math_min_dbl(unsigned int args_num, ...);

/**
 * @brief gets the greater of the two numbers
 */
#ifndef SIRIUS_MAX_T
#define SIRIUS_MAX_T(num1, num2)    ((num1) > (num2) ? (num1) : (num2))
#endif // SIRIUS_MAX_T

/**
 * @brief gets the lesser of the two numbers
 */
#ifndef SIRIUS_MIN_T
#define SIRIUS_MIN_T(num1, num2)        ((num1) < (num2) ? (num1) : (num2))
#endif // SIRIUS_MIN_T

#ifndef SIRIUS_MAX_INT
#define SIRIUS_MAX_INT(args_num, ...)   sirius_math_max_int(args_num, __VA_ARGS__)
#endif // SIRIUS_MAX_INT

#ifndef SIRIUS_MIN_INT
#define SIRIUS_MIN_INT(args_num, ...)   sirius_math_min_int(args_num, __VA_ARGS__)
#endif // SIRIUS_MIN_INT

#ifndef SIRIUS_MAX_DBL
#define SIRIUS_MAX_DBL(args_num, ...)   sirius_math_max_dbl(args_num, __VA_ARGS__)
#endif // SIRIUS_MAX_DBL

#ifndef SIRIUS_MIN_DBL
#define SIRIUS_MIN_DBL(args_num, ...)   sirius_math_min_dbl(args_num, __VA_ARGS__)
#endif // SIRIUS_MIN_DBL

#ifdef __cplusplus
}
#endif

#endif // __SIRIUS_MATH_H__
