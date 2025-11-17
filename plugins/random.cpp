#include "../include/plugin.hpp"
#include "../include/value.hpp"
#include <random>
#include <chrono>

using namespace lang;

static std::mt19937_64& rng() {
    static std::mt19937_64 gen(std::chrono::steady_clock::now().time_since_epoch().count());
    return gen;
}

static Value random_int(const std::vector<Value>& args) {
    if (args.size() == 2 && args[0].t == Type::Int && args[1].t == Type::Int) {
        long long a = std::get<long long>(args[0].v);
        long long b = std::get<long long>(args[1].v);
        if (a > b) std::swap(a, b);
        std::uniform_int_distribution<long long> dist(a, b);
        return Value::I(dist(rng()));
    }
    return Value::N();
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
    "1.0.0",
    random_init,
    []() -> const PluginFunctionEntry* { return plugin_functions; },
    random_cleanup
};

extern "C" {
PluginInterface* get_plugin_interface() { return &random_interface; }
}
