/*NOTE:
 * The purpose of this file, is to allow simple use of GNU project features.
 * We use Clang, and only Clang, since it provides the same GNU features,
 * And overall better matches our target.
 */
#pragma once

#include <cstdint>

#define uint8 uint8_t
#define uint16 uint16_t
#define uint32 uint32_t
#define uint64 uint64_t
#define int8 int8_t
#define int16 int16_t
#define int32 int32_t
#define int64 int64_t

#define int DONT_USE_INT___USE_FIXED_SIZES

#if !defined(_WIN32) && !defined(__linux__) && !defined(__APPLE__)
  #error "Use Windows, Linux, or Apple."
#endif

#if defined(__GNUC__) || defined(__clang__)
  #define hot __attribute__((hot))
  #define cold __attribute__((cold))
  #define impossible __builtin_unreachable()
  #define noreturn __attribute__((noreturn))
  #define nouse __attribute__((unused))
  #define uint128 unsigned __int128
  #define int128 __int128
  // Add the folliowing with boost::multiprecision
  //#define uint256 unsigned _BitInt(256)
  //#define int256 _BitInt(256)
#else
  #error "Use g++ or Clang to compile."
#endif
