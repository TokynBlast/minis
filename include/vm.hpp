#pragma once
#include <memory>
#include <cstdio>
#include <string>

namespace minis {
  struct VMEngine;

  class VM {
  private:
    std::unique_ptr<VMEngine> engine;

  public:
    VM();
    ~VM();

    void load(const std::string& path);
    void run();
  };


  void run(const std::string& path);
}
