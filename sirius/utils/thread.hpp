#pragma once
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

namespace sirius {
namespace utils {
namespace thread {
#if defined(__linux__)
inline uint64_t linux_get_tid_impl() {
#  if defined(__x86_64__)
  pid_t tid;
  __asm__ volatile(
    "mov $186, %%eax\n"
    "syscall\n"
    : "=a"(tid)
    :
    : "rcx", "r11", "memory");
  return (uint64_t)tid;
#  elif defined(__i386__)
  pid_t tid;
  __asm__ volatile(
    "mov $224, %%eax\n"
    "int $0x80\n"
    : "=a"(tid)
    :
    : "memory");
  return (uint64_t)tid;
#  elif defined(__aarch64__)
  uint64_t tid;
  __asm__ volatile(
    "mov x8, %1\n"
    "svc #0\n"
    "mov %0, x0\n"
    : "=r"(tid)
    : "i"(178)
    : "x0", "x8", "memory");
  return tid;
#  elif defined(__arm__)
  uint32_t tid;
  __asm__ volatile(
    "mov r7, %1\n"
    "svc #0\n"
    "mov %0, r0\n"
    : "=r"(tid)
    : "i"(224)
    : "r0", "r7", "memory");
  return (uint64_t)tid;
#  elif defined(__mips__) && defined(__mips_o32)
  uint32_t tid;
  __asm__ volatile(
    "li $v0, %1\n"
    "syscall\n"
    "move %0, $v0\n"
    : "=r"(tid)
    : "i"(4244)
    : "$v0", "memory");
  return (uint64_t)tid;
#  elif defined(__mips__) && \
    (defined(__mips64) || defined(__mips_n64) || defined(__mips_n32))
  uint64_t tid;
  __asm__ volatile(
    "li $v0, %1\n"
    "syscall\n"
    "move %0, $v0\n"
    : "=r"(tid)
    : "i"(5152)
    : "$v0", "memory");
  return tid;
#  elif defined(__powerpc64__) || defined(__PPC64__)
  uint64_t tid;
  __asm__ volatile(
    "li 0, %1\n"
    "sc\n"
    "mr %0, 3\n"
    : "=r"(tid)
    : "i"(207)
    : "r0", "r3", "memory");
  return tid;
#  elif defined(__powerpc__)
  uint32_t tid;
  __asm__ volatile(
    "li 0, %1\n"
    "sc\n"
    "mr %0, 3\n"
    : "=r"(tid)
    : "i"(207)
    : "r0", "r3", "memory");
  return (uint64_t)tid;
#  elif defined(__riscv) && __riscv_xlen == 64
  uint64_t tid;
  __asm__ volatile(
    "li a7, %1\n"
    "ecall\n"
    "mv %0, a0\n"
    : "=r"(tid)
    : "i"(178)
    : "a0", "a7", "memory");
  return tid;
#  elif defined(__riscv) && __riscv_xlen == 32
  uint32_t tid;
  __asm__ volatile(
    "li a7, %1\n"
    "ecall\n"
    "mv %0, a0\n"
    : "=r"(tid)
    : "i"(178)
    : "a0", "a7", "memory");
  return (uint64_t)tid;
#  elif defined(__s390x__)
  uint64_t tid;
  __asm__ volatile(
    "lghi %%r1, %1\n"
    "svc 0\n"
    "lgr %0, %%r2\n"
    : "=r"(tid)
    : "i"(236)
    : "r1", "r2", "memory");
  return tid;
#  elif defined(__s390__)
  uint32_t tid;
  __asm__ volatile(
    "lr 1, %1\n"
    "svc 0\n"
    "lr %0, 2\n"
    : "=r"(tid)
    : "r"(236)
    : "r1", "r2", "memory");
  return (uint64_t)tid;
#  else
  /**
   * @brief Fallback.
   */
  return (uint64_t)(uintptr_t)pthread_self();
#  endif
}
#endif

inline uint64_t get_tid_impl() {
#if defined(__linux__)
#  if defined(__GLIBC__) && defined(__GLIBC_PREREQ) && __GLIBC_PREREQ(2, 30)
  return (uint64_t)::gettid();
#  elif defined(SYS_gettid) && \
    (defined(_GNU_SOURCE) || \
     (defined(__STDC_VERSION__) && __STDC_VERSION__ <= 201710L))
  return (uint64_t)syscall(SYS_gettid);
#  else
  return linux_get_tid_impl();
#  endif
#elif defined(_WIN32) || defined(_WIN64)
  return (uint64_t)GetCurrentThreadId();
#elif defined(__APPLE__) && defined(__MACH__)
  uint64_t tid = 0;
  pthread_threadid_np(nullptr, &tid);
  return tid;
#elif defined(__FreeBSD__)
  return (uint64_t)pthread_getthreadid_np();
#elif defined(__NetBSD__)
  return (uint64_t)_lwp_self();
#elif defined(__OpenBSD__)
  return (uint64_t)getthrid();
#elif defined(sun) || defined(__sun)
  return (uint64_t)thr_self();
#else
  /**
   * @brief Fallback.
   *
   * @note `pthread_t` is not guaranteed to be an integer, but in most
   * implementations it is either a pointer or an integer.
   */
  return (uint64_t)(uintptr_t)pthread_self();
#endif
}
} // namespace thread
} // namespace utils
} // namespace sirius
