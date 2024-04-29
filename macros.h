#ifndef __MACROS_H
#define __MACROS_H
#include "constants.h"
#include "logging.h"

#include <stdlib.h>

#define return_defer(value) do { ret = value; goto defer; } while(0)
#define UNUSED(x) ((void)x)
#define SIGN(x) ((x) > 0 ? 1.0: -1.0)
#define INCEPTION(M)
#define CLOSE_ENOUGH(x, y, eps) ((SIGN((x) - (y)) * ((x) - (y))) <= (eps))

#define da_append(da, item) do { \
    if ((da)->len >= (da)->cap) { \
        if ((da)->cap <= 1) \
            (da)->cap = (da)->len + 2;\
        else\
            (da)->cap *= 1.5; \
        (da)->items = realloc((da)->items, sizeof(*(da)->items) * (da)->cap); \
        if (!(da)->items) \
            logging_log(LOG_FATAL, "%s:%d Could not append item to dynamic array. Allocation failed. Buy more RAM I guess, lol", __FILE__, __LINE__); \
        memset(&(da)->items[(da)->len], 0, sizeof(*(da)->items) * ((da)->cap - (da)->len));\
    } \
    (da)->items[(da)->len] = (item); \
    (da)->len += 1; \
} while(0)

#define da_remove(da, idx) do { \
    if ((idx) >= (da)->len || (idx) < 0) { \
        logging_log(LOG_ERROR, "%s:%d Trying to remove out of range idx %"PRIi64" from dynamic array", __FILE__, __LINE__, (s64)idx); \
        break; \
    } \
    memmove(&(da)->items[(idx)], &(da)->items[(idx) + 1], sizeof(*(da)->items) * ((da)->len - (idx) - 1)); \
    if ((da)->len <= (da)->cap / 2) { \
        (da)->cap /= 1.5; \
        (da)->items = realloc((da)->items, sizeof(*(da)->items) * (da)->cap); \
        if (!(da)->items) \
            logging_log(LOG_FATAL, "%s:%d Could not append item to dynamic array. Allocation failed. Buy more RAM I guess, lol", __FILE__, __LINE__); \
        memset(&(da)->items[(da)->len], 0, sizeof(*(da)->items) * ((da)->cap - (da)->len));\
    } \
    (da)->len -= 1;\
} while(0)

#define assert(cond) do { if (!(cond)) logging_log(LOG_FATAL, "%s:%d Condition \""#cond"\" failed", __FILE__, __LINE__);} while(0)
#ifdef DEBUG
#define debug_assert(cond) assert(cond)
#else
#define debug_assert(cond)
#endif

#endif
