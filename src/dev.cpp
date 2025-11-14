// This is for the development of this library
// This will become boot strapped (it means made in minis)

#include <vector>
#include <unordered_map>
#include <function>
#include "../include/io.hpp"
#include "../include/token.hpp"
#include "../include/type.hpp"

using DevFn = std::function<Value(std::vector<Value>&)>;

// Registry of dev functions
inline static std::unordered_map<CString, BuiltinFn> builtins = {
  {"dev.emitStr", [](std::vector<Value>& args) {
    for (const auto& arg : args) std::cout << arg.AsStr() << " ";
    emit
    return Value::N();
  }},
}