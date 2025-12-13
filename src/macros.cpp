/* NOTE: [comment expanded]
 * The purpose of having clang and gnu, is because we know we
 * will only ever compile via g++ or clang
 * Especially if using the provided scripts.
 * We should provide a compiled C++ script for x86-64,
 * so if the bash file doesn't work, the user can run that,
 * but only as an absolute fail safe, due to safety.
 */

#if defined(__clang__)
  #define hot __attribute__((hot))
  #define cold __attribute__((cold))
  #define impossible __builtin_unreachable()
#elif defined(__GNUC__)
  #define hot __attribute__((hot))
  #define cold __attribute__((cold))
  #define impossible __builtin_unreachable()
#else
  #if defined(_WIN32)
    #error "Run the install.ps1 file, or install using g++/clang."
  #elif defined(__linux__) || defined(__APPLE__)
    #error "Run the install.sh file, or install usng g++/clang."
  #else
    #error "Try running the install.sh file, or compiling using g++/clang.\nIf it doesn't work, make a new issue here: https://github.com/TokynBlast/minis/issues"
  #endif
#endif
