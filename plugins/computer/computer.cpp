#include "../include/plugin.hpp"
#include "../include/value.hpp"
#include <vector>

using namespace minis;

static Value os(const std::vector<Value>& args) {
  #if defined(_WIN32) || defined(_WIN64)
    return Value::Str("win");
  #elif defined(__linux__) || defined(__unix__)
    return Value::Str("linux");
  #elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    return Value::Str("freebsd");
  #elif defined(__APPLE__) || defined(__MACH__)
    return Value::Str("apple");
  #elif defined(sun) || defined(__sun__)
    return Value::Str("solaris");
  #elif defined(_AIX)
    return Value::Str("aix");
  #elif defined(__hpux)
    return Value::Str("hp-ux");
  #else
    return Value::Str("unknown");
  #endif
}

static Value cpu(std::vector<Value>& args) {
  #if defined(__i386__) || defined(i386)
    return Value::Str("x86");
  #elif defined(__x86_64__) || defined(M_X64)
    return Value::Str("x86-64");
  #elif defined(__ARM__)
    return Value::Str("arm");
  #elif defined(__aarch_64__)
    return Value::Str("arm64");
  #elif defined(__ppc__) || defined(__powerpc__)
    return Value::Str("powerpc");
  #elif defined(__sparc__)
    return Value::Str("sparc");
  #else
    return Value::Str("unknown");
  #endif
}

static const PluginFunctionEntry plugin_functions[] = {
  {"os", os},
  {"cpu", cpu},
  {nullptr, nullptr}
};

static bool computer_init() { return true; }
static void computer_cleanup() {}

static PluginInterface random_interface = {
    "computer",
    "0.0.3",
    computer_init,
    []() -> const PluginFunctionEntry* { return plugin_functions; },
    computer_cleanup
};

extern "C" {
PluginInterface* get_plugin_interface() { return &random_interface; }
}
