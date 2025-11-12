//#include <cstdio>
//#include <cstdlib>
//#include <cstring>

#include "include/sso.hpp"
#include "include/driver.hpp"
#include "include/compiler.hpp"
#include "include/err.hpp"
#include "include/context.hpp"

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      std::fprintf(stderr,
        "Usage: cmin [options] <input>\n"
        "Options:\n"
        "  -db           enable debug data\n"
        "  -e            embed imports\n"
        "  -o <output>   output path (default: a.mi)\n"
        "  --no-warn     suppress warnings\n"
        "  --no-note     suppress notes\n");
      return 2;
    }

    lang::CString in_path;
    lang::CString out_path = "a.mi";
    bool opt_db = false;
    bool opt_e = false;
    bool opt_no_warn = false;
    bool opt_no_note = false;

    for (int i = 1; i < argc; ++i) {
      const char* a = argv[i];
      if (std::strcmp(a, "-db") == 0) { opt_db = true; continue; }
      if (std::strcmp(a, "-e") == 0)  { opt_e = true; continue; }
      if (std::strcmp(a, "-o") == 0) {
        if (i + 1 < argc) { out_path = argv[++i]; continue; }
        std::fprintf(stderr, "-o requires an argument\n");
        return 2;
      }
      if (std::strcmp(a, "--no-warn") == 0) { opt_no_warn = true; continue; }
      if (std::strcmp(a, "--no-note") == 0) { opt_no_note = true; continue; }

      if (in_path.empty()) { in_path = a; continue; }

      std::fprintf(stderr, "Unknown arg: %s\n", a);
      return 2;
    }

    if (in_path.empty()) {
      std::fprintf(stderr, "No input file specified\n");
      return 2;
    }

    std::printf("Input: %s\nOutput: %s\nDebug: %d  Embed: %d  NoWarn: %d  NoNote: %d\n",
                in_path.c_str(), out_path.c_str(), opt_db ? 1 : 0, opt_e ? 1 : 0,
                opt_no_warn ? 1 : 0, opt_no_note ? 1 : 0);

    lang::CString src_text = lang::ReadFile(in_path);
    lang::CompileToFile(in_path, src_text.c_str(), out_path);

    std::printf("Wrote %s\n", out_path.c_str());
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "Error: %s\n", e.what());
    return 1;
  }
}