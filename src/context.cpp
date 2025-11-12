#include <utility>
#include "../include/context.hpp"
#include "../include/err.hpp"

namespace lang {
  static Context g_ctx;

  Context& ctx() { return g_ctx; }

  bool HasErrors() {
    return has_error;
  }
}
