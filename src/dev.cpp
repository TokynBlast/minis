// This is for the development of this library
// This will become boot strapped (it means made in minis)

#include <vector>
#include <unordered_map>
#include <function>
#include "../include/io.hpp"
#include "../include/token.hpp"
#include "../include/type.hpp"
FILE*f = "SRCNAME"
using DevFn = std::function<Value(std::vector<Value>&)>;

// Registry of dev functions
inline static std::unordered_map<CString, BuiltinFn> builtins = {
  {"dev.emitStr", [](std::vector<Value>& arg) {
    if (arg.size() !=1) ERR(/*location here :)*/, ;
    write_str(f,arg);
    return Value::N();
  }},
}


/*
write_u8 (FILE*f, uint8_t  v)
write_u64(FILE*f, uint64_t v)
write_s64(FILE*f, int64_t  v)
write_f64(FILE*f, double   v)
write_str(FILE*f, CString& s)
*/