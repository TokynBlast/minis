#include <chrono>
#include <vector>
#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
  #include <wincrypt.h>
  #include <bcrypt.h>
#endif

#include "../../include/plugin.hpp"
#include "../../include/value.hpp"
#include "../../fast_io/include/fast_io.h"

using namespace minis;

extern "C" {
  static uint64 mersene_twister(uint64 seed);
}
static Value true_rand(const std::vector<Value>& args) {
#if defined(_WIN32) || defined(_WIN64)
  #pragma comment(lib, "bcrypt.lib")

    unsigned char random_bytes[8];   // Buffer to store random data (8 bytes for uint64)
  NTSTATUS status = BCryptGenRandom(
      NULL,                           // hAlgorithm (NULL for default provider)
      random_bytes,                   // pbBuffer
      sizeof(random_bytes),           // cbBuffer
      BCRYPT_USE_SYSTEM_PREFERRED_RNG // dwFlags
  );
  uint64 result = 0;
  for (int i = 0; i < 8; ++i) {
    result |= static_cast<uint64>(random_bytes[i]) << (i * 8);
  }
  return Value::UI64(result);
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__) || defined(__MACH__)
  // FIXME: Will consume all of urandom, consuming all entropy
  fast_io::iobuf_io_file file("/dev/urandom");
  unsigned char random_bytes[8];
  read(file, random_bytes, 8);
  uint64 result = 0;
  for (int i = 0; i < 8; ++i) {
    result |= static_cast<uint64>(random_bytes[i]) << (i * 8);
  }
  return Value::UI64(result);
#endif
}

static Value random_choice(const std::vector<Value>& args) {
    if (args.size() == 1 && args[0].t == Type::List) {
        const auto& list = std::get<std::vector<Value>>(args[0].v);
        if (list.empty()) return Value::N();
        std::uniform_int_distribution<size_t> dist(0, list.size() - 1);
        return list[dist(rng())];
    }
    return Value::N();
}

static Value random_float(const std::vector<Value>& args) {
    double a = 0.0, b = 1.0;
    if (args.size() == 2 && args[0].t == Type::Float && args[1].t == Type::Float) {
        a = std::get<double>(args[0].v);
        b = std::get<double>(args[1].v);
        if (a > b) std::swap(a, b);
    }
    std::uniform_real_distribution<double> dist(a, b);
    return Value::F(dist(rng()));
}

static const PluginFunctionEntry plugin_functions[] = {
    {"int", random_int},
    {"float", random_float},
    {"choice", random_choice},
    {nullptr, nullptr}
};

static bool random_init() { return true; }
static void random_cleanup() {}

static PluginInterface random_interface = {
    "random",
    "0.2.0",
    random_init,
    []() -> const PluginFunctionEntry* { return plugin_functions; },
    random_cleanup
};

extern "C" {
PluginInterface* get_plugin_interface() { return &random_interface; }
}
