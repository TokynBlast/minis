#include "../include/plugin.hpp"
#include "../include/value.hpp"
#include <cmath>

using namespace lang;

static Value mathSin(const std::vector<Value>& args) {
    if (args.size() == 1 && args[0].t == Type::Float) {
        return Value::F(std::sin(std::get<double>(args[0].v)));
    }
    return Value::N();
}

static Value mathCos(const std::vector<Value>& args) {
    if (args.size() == 1 && args[0].t == Type::Float) {
        return Value::F(std::cos(std::get<double>(args[0].v)));
    }
    return Value::N();
}

static Value mathPow(const std::vector<Value>& args) {
    if (args.size() == 2 && args[0].t == Type::Float && args[1].t == Type::Float) {
        return Value::F(std::pow(std::get<double>(args[0].v), std::get<double>(args[1].v)));
    }
    return Value::N();
}

static Value mathTan(const std::vector<Value>& args) {
    if (args.size() == 1 && args[0].t == Type::Float) {
        return Value::F(std::tan(std::get<double>(args[0].v)));
    }
    return Value::N();
}

static Value mathSqrt(const std::vector<Value>& args) {
    if (args.size() == 1 && args[0].t == Type::Float) {
        return Value::F(std::sqrt(std::get<double>(args[0].v)));
    }
    return Value::N();
}

static Value mathLog(const std::vector<Value>& args) {
    if (args.size() == 1 && args[0].t == Type::Float) {
        return Value::F(std::log(std::get<double>(args[0].v)));
    }
    return Value::N();
}

static Value mathExp(const std::vector<Value>& args) {
    if (args.size() == 1 && args[0].t == Type::Float) {
        return Value::F(std::exp(std::get<double>(args[0].v)));
    }
    return Value::N();
}

static const PluginFunctionEntry plugin_functions[] = {
    {"sin", mathSin},
    {"cos", mathCos},
    {"pow", mathPow},
    {"tan", mathTan},
    {"sqrt", mathSqrt},
    {"log", mathLog},
    {"exp", mathExp},
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
