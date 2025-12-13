#include "../include/plugin.hpp"
#include "../include/value.hpp"
#include <vector>

using namespace minis;

static Value os(const std::vector<Value>& args) {
  #if defined(_WIN32) || defined(_WIN64)
    return Value::S("win");
  #elif defined(__linux__) || defined(__unix__)
    return Value::S("linux");
  #elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    return Value::S("freebsd");
  #elif defined(__APPLE__) || defined(__MACH__)
    return Value::S("apple");
  #elif defined(sun) || defined(__sun__)
    return Value::S("solaris");
  #elif defined(_AIX)
    return Value::S("aix");
  #elif defined(__hpux)
    return Value::S("hp-ux");
  #else
    return Value::S("unknown");
  #endif
}

static Value int_size() {
  return Value::I(sizeof(int));
}

static Value cpu() {
  #if defined(__i386__) || defined(i386)
    return Value::S("x86");
  #elif defined(__x86_64__) || defined(M_X64)
    return Value::S("x86-64");
  #elif defined(__ARM__)
    return Value::S("arm");
  #elif defined(__aarch_64__)
    return Value::S("arm64");
  #elif defined(__ppc__) || defined(__powerpc__)
    return Value::S("powerpc");
  #elif defined(__sparc__)
    return Value::S("sparc");
  #else
    return Value::S("unknown");
  #endif
}

static const PluginFunctionEntry plugin_functions[] = {
  {"os", os},
  {"cpu", cpu},
  {"sint", int_size},
  {nullptr, nullptr}
};

static bool computer_init() { return true; }
static void computer_cleanup() {}

static PluginInterface random_interface = {
    "computer",
    "0.0.2",
    computer_init,
    []() -> const PluginFunctionEntry* { return plugin_functions; },
    computer_cleanup
};

extern "C" {
PluginInterface* get_plugin_interface() { return &random_interface; }
}
