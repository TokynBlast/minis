#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__)
    typedef unsigned _BitInt(128) uint128;
    typedef signed   _BitInt(128) int128;
#else
    #error "128-bit integers are only supported in GCC with _BitInt(128)."
#endif

#ifdef __cplusplus
} // extern "C"
#endif
