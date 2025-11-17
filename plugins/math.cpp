#include "../include/plugin.hpp"
#include "../include/value.hpp"
#include <cmath>

using namespace lang;

static Value math_sin(const std::vector<Value>& args) {
    if (args.size() == 1 && args[0].t == Type::Float) {
        return Value::F(std::sin(std::get<double>(args[0].v)));
    }
    return Value::N();
}

static Value math_cos(const std::vector<Value>& args) {
    if (args.size() == 1 && args[0].t == Type::Float) {
        return Value::F(std::cos(std::get<double>(args[0].v)));
    }
    return Value::N();
}

static Value math_pow(const std::vector<Value>& args) {
    if (args.size() == 2 && args[0].t == Type::Float && args[1].t == Type::Float) {
        return Value::F(std::pow(std::get<double>(args[0].v), std::get<double>(args[1].v)));
    }
    return Value::N();
}

static const PluginFunctionEntry plugin_functions[] = {
    {"sin", math_sin},
    {"cos", math_cos},
    {"pow", math_pow},
    {nullptr, nullptr}
};

static bool math_init() { return true; }
static void math_cleanup() {}

static PluginInterface math_interface = {
    "math",
    "0.0.1",
    math_init,
    []() -> const PluginFunctionEntry* { return plugin_functions; },
    math_cleanup
};

extern "C" {
PluginInterface* get_plugin_interface() { return &math_interface; }
}
