/**
 * @name sirius_common.h
 *
 * @author UsadaYu
 *
 * @date create: 2024-07-30
 * @date update: 2025-01-20
 */

#ifndef __SIRIUS_COMMON_H__
#define __SIRIUS_COMMON_H__

#define SIRIUS_VERSION_MAJOR 1
#define SIRIUS_VERSION_MINOR 1
#define SIRIUS_VERSION_PATCH 0

#define SIRIUS_VERSION "1.1.0"

#if defined(_WIN32)
#include <Windows.h>
#endif

#if !defined(_MSC_VER)
#include <pthread.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif  // __SIRIUS_COMMON_H__
