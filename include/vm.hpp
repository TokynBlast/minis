#pragma once
#include <memory>
#include "sso.hpp"

namespace lang {
  struct VMEngine; // Forward declaration

  class VM {
  private:
    std::unique_ptr<VMEngine> engine;
  public:
    VM() = default;
    void load(const CString& path);
    void run();
  };

  // Global function for easy usage
  void run(const CString& path);
}