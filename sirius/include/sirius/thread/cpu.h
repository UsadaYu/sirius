#pragma once

#if defined(_MSC_VER)
#  if defined(_M_IX86) || defined(_M_X64)
#    ifdef __cplusplus
extern "C"
#    endif
  void _mm_pause(void);
#    pragma intrinsic(_mm_pause)
#    define sirius_cpu_relax() _mm_pause()
#  elif defined(_M_ARM) || defined(_M_ARM64)
#    if _MSC_VER >= 1920 && defined(_M_ARM64)
#      ifdef __cplusplus
extern "C"
#      endif
  void __yield(void);
#      pragma intrinsic(__yield)
#      define sirius_cpu_relax() __yield()
#    else
#      ifdef __cplusplus
extern "C"
#      endif
  void __nop(void);
#      pragma intrinsic(__nop)
#      define sirius_cpu_relax() __nop()
#    endif
#  else
#    define sirius_cpu_relax() \
      do { \
      } while (0)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  if defined(__i386__) || defined(__x86_64__)
#    ifdef __SSE2__
#      include <emmintrin.h>
#      define sirius_cpu_relax() _mm_pause()
#    else
#      define sirius_cpu_relax() __asm__ volatile("pause" ::: "memory")
#    endif
#  elif defined(__aarch64__) || defined(__arm__)
#    if defined(__aarch64__)
#      define sirius_cpu_relax() __asm__ volatile("yield" ::: "memory")
#    elif defined(__ARM_ARCH) && (__ARM_ARCH >= 7)
#      define sirius_cpu_relax() __asm__ volatile("yield" ::: "memory")
#    else
#      define sirius_cpu_relax() \
        do { \
        } while (0)
#    endif
#  elif defined(__powerpc__) || defined(__ppc__)
#    define sirius_cpu_relax() __asm__ volatile("or 27,27,27" ::: "memory")
#  elif defined(__riscv)
#    define sirius_cpu_relax() __asm__ volatile("pause" ::: "memory")
#  else
#    define sirius_cpu_relax() \
      do { \
      } while (0)
#  endif
#else
#  define sirius_cpu_relax() \
    do { \
    } while (0)
#endif
