#pragma once

/**
 * @brief CPU pause (Hardware-level: spin backoff).
 *
 * @note The `yield` instruction in ARM assembly is a hardware hint, equivalent
 * to the `pause` instruction in x86/x64, and does not involve the OS.
 */
#if defined(_MSC_VER) && !defined(__clang__)
#  if defined(_M_IX86) || defined(_M_X64)
#    ifdef __cplusplus
extern "C" void _mm_pause(void);
#    else
void _mm_pause(void);
#    endif
#    pragma intrinsic(_mm_pause)
#    define sirius_cpu_pause() _mm_pause()
#  elif defined(_M_ARM) || defined(_M_ARM64)
#    if _MSC_VER >= 1920 && defined(_M_ARM64)
#      ifdef __cplusplus
extern "C" void __yield(void);
#      else
void __yield(void);
#      endif
#      pragma intrinsic(__yield)
#      define sirius_cpu_pause() __yield()
#    else
#      ifdef __cplusplus
extern "C" void __nop(void);
#      else
void __nop(void);
#      endif
#      pragma intrinsic(__nop)
#      define sirius_cpu_pause() __nop()
#    endif
#  else
#    define sirius_cpu_pause() \
      do { \
      } while (0)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  if defined(__i386__) || defined(__x86_64__)
#    define sirius_cpu_pause() __asm__ volatile("pause" ::: "memory")
#  elif defined(__aarch64__) || defined(__arm__)
#    if defined(__aarch64__) || (defined(__ARM_ARCH) && __ARM_ARCH >= 7)
#      define sirius_cpu_pause() __asm__ volatile("yield" ::: "memory")
#    else
#      define sirius_cpu_pause() __asm__ volatile("nop" ::: "memory")
#    endif
#  elif defined(__powerpc__) || defined(__ppc__)
#    define sirius_cpu_pause() __asm__ volatile("or 27,27,27" ::: "memory")
#  elif defined(__riscv)
#    define sirius_cpu_pause() __asm__ volatile("pause" ::: "memory")
#  else
#    define sirius_cpu_pause() \
      do { \
      } while (0)
#  endif
#else
#  define sirius_cpu_pause() \
    do { \
    } while (0)
#endif

/**
 * @brief OS Yield (System-level: Give up time slices).
 */
#if defined(_WIN32) || defined(_WIN64)
#  ifdef __cplusplus
extern "C" {
#  endif
__declspec(dllimport) int __stdcall SwitchToThread(void);
#  ifdef __cplusplus
}
#  endif
#  define sirius_os_yield() SwitchToThread()
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__) || \
  defined(__posix__)
#  include <sched.h>
#  define sirius_os_yield() sched_yield()
#elif (defined(__cplusplus) && __cplusplus >= 201103L) || \
  (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
#  include <thread>
#  define sirius_os_yield() std::this_thread::yield()
#else
#  define sirius_os_yield() \
    do { \
    } while (0)
#endif
