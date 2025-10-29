#pragma once
#include <string>

namespace minis {
  std::string ReadFile(std::string& path);
  void ComplieFile(const std::string& SrcName,
                   const std::string& SrcText,
                   const std::string& OutLoc);
  void RunMini(const std::string& path);
}