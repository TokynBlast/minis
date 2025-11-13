#pragma once
#include "sso.hpp"
#include <memory>

namespace lang {
  struct VMEngine; // Forward declaration

  class VM {
  private:
    std::unique_ptr<VMEngine> engine;

  public:
    VM();
    ~VM();

    VM(const VM&) = delete;
    VM& operator=(const VM&) = delete;

    VM(VM&&) = default;
    VM& operator=(VM&&) = default;

    void load(const CString& path);
    void run();
  };

  void run(const CString& path);
}