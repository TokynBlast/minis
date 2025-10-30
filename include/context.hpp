#pragma once
#include <vector>
#include <cstddef>

namespace minis {
  extern std::vector<std::size_t> g_posmap;
  inline std::size_t map_pos(std::size_t i){
    return (i < g_posmap.size()) ? g_posmap[i] : i;
  }
  bool HasErrors();
}