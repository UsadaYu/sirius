#ifndef __SIRIUS_INTERNAL_FILE_H__
#define __SIRIUS_INTERNAL_FILE_H__

#include "sirius_internal_sys.h"

/**
 * if the file exists but fails to be deleted,
 * `errno` is returned, 0 otherwise
 */
#define internal_file_access_and_remove(ret, file) \
    do { \
        ret = 0; \
        if (access(file, F_OK) == 0) { \
            if (remove(file)) { \
                perror("remove error"); \
                ret = errno; \
            } \
        } \
    } while (0)

#define internal_file_mkfifo(ret, file, mode) \
    do { \
        ret = -1; \
        internal_file_access_and_remove(ret, file); \
        if (ret) break; \
        ret = mkfifo(file, mode); \
        if (ret) { \
            perror("mkfifo error"); \
        } \
    } while (0)

#endif // __SIRIUS_INTERNAL_FILE_H__
