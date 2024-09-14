#ifndef __SIRIUS_INTERNAL_SYS_H__
#define __SIRIUS_INTERNAL_SYS_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <limits.h>
#include <float.h>
#include <errno.h>

#ifdef __linux__
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/syscall.h>

#if defined(__STDC_NO_ATOMICS__)
#warning "atomic not support"
#else
#include <stdatomic.h>
#endif

#endif // __linux__
#endif // __SIRIUS_INTERNAL_SYS_H__
