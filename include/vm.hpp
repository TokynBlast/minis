#pragma once
#include <memory>
#include <cstdio>
#include <cstdint>
#include "sso.hpp"

namespace minis {
  struct VMEngine;

  class VM {
  private:
    std::unique_ptr<VMEngine> engine;

  public:
    VM();
    ~VM();

    void load(const CString& path);
    void run();
  };


  void run(const CString& path);
}
