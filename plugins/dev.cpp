#include "../include/plugin.hpp"
#include "../include/value.hpp"
#include "../include/sso.hpp"
#include <iostream>
#include <fstream>
#include <cstring>

using namespace lang;

// Example plugin functions for 'dev' module

static Value dev_write_bytes(const std::vector<Value>& args) {
    if (args.size() < 2) return Value::N();
    
    // args[0] = filename, args[1] = byte list
    if (args[0].t != Type::Str || args[1].t != Type::List) return Value::N();
    
    const CString& filename = std::get<CString>(args[0].v);
    const auto& bytes = std::get<std::vector<Value>>(args[1].v);
    
    std::ofstream file(filename.c_str(), std::ios::binary);
    if (!file) return Value::B(false);
    
    for (const auto& byte_val : bytes) {
        if (byte_val.t == Type::Int) {
            uint8_t b = (uint8_t)std::get<long long>(byte_val.v);
            file.write((const char*)&b, 1);
        }
    }
    
    return Value::B(true);
}

static Value dev_read_bytes(const std::vector<Value>& args) {
    if (args.empty() || args[0].t != Type::Str) return Value::N();
    
    const CString& filename = std::get<CString>(args[0].v);
    std::ifstream file(filename.c_str(), std::ios::binary | std::ios::ate);
    if (!file) return Value::L({});
    
    size_t size = file.tellg();
    file.seekg(0);
    
    std::vector<Value> bytes;
    bytes.reserve(size);
    
    for (size_t i = 0; i < size; ++i) {
        uint8_t b;
        file.read((char*)&b, 1);
        bytes.push_back(Value::I((long long)b));
    }
    
    return Value::L(std::move(bytes));
}

static Value dev_emit_u64(const std::vector<Value>& args) {
    // Converts a number to 8-byte little-endian representation
    if (args.empty()) return Value::L({});
    
    long long val = 0;
    if (args[0].t == Type::Int) {
        val = std::get<long long>(args[0].v);
    } else if (args[0].t == Type::Float) {
        val = (long long)std::get<double>(args[0].v);
    }
    
    std::vector<Value> bytes;
    for (int i = 0; i < 8; ++i) {
        bytes.push_back(Value::I((val >> (i * 8)) & 0xFF));
    }
    
    return Value::L(std::move(bytes));
}

static Value dev_emit_str(const std::vector<Value>& args) {
    // Converts a string to length-prefixed byte list
    if (args.empty() || args[0].t != Type::Str) return Value::L({});
    
    const CString& str = std::get<CString>(args[0].v);
    size_t len = str.size();
    
    std::vector<Value> bytes;
    
    // Emit length as u64
    for (int i = 0; i < 8; ++i) {
        bytes.push_back(Value::I((len >> (i * 8)) & 0xFF));
    }
    
    // Emit string bytes
    for (size_t i = 0; i < len; ++i) {
        bytes.push_back(Value::I((long long)(uint8_t)str.c_str()[i]));
    }
    
    return Value::L(std::move(bytes));
}

    // moveto(file_handle, offset, whence=0)
    static Value dev_moveto(const std::vector<Value>& args) {
        if (args.size() != 2 && args.size() != 3) return Value::I(-1);
        FILE* file = (FILE*)(uintptr_t)args[0].AsInt(0);
        long long offset = args[1].AsInt(0);
        int whence = SEEK_SET;
        if (args.size() == 3) {
            long long w = args[2].AsInt(0);
            if (w == 0) whence = SEEK_SET;
            else if (w == 1) whence = SEEK_CUR;
            else if (w == 2) whence = SEEK_END;
        }
        if (!file) return Value::I(-1);
        int result = fseek(file, (long)offset, whence);
        return Value::I(result);
    }

    // pos(file_handle)
    static Value dev_pos(const std::vector<Value>& args) {
        if (args.size() != 1) return Value::I(-1);
        FILE* file = (FILE*)(uintptr_t)args[0].AsInt(0);
        if (!file) return Value::I(-1);
        long position = ftell(file);
        return Value::I((long long)position);
    }

    // typename(value)
    static Value dev_typename(const std::vector<Value>& args) {
        if (args.size() != 1) return Value::S("unknown");
        switch (args[0].t) {
            case Type::Int:   return Value::S("int");
            case Type::Float: return Value::S("float");
            case Type::Str:   return Value::S("string");
            case Type::Bool:  return Value::S("bool");
            case Type::List:  return Value::S("list");
            case Type::Null:  return Value::S("null");
            default:          return Value::S("unknown");
        }
    }

// Plugin function table
static const PluginFunctionEntry plugin_functions[] = {
    {"write_bytes", dev_write_bytes},
    {"read_bytes", dev_read_bytes},
    {"emit_u64", dev_emit_u64},
    {"emit_str", dev_emit_str},
    {"moveto", dev_moveto},
    {"pos", dev_pos},
    {"typename", dev_typename},
    {nullptr, nullptr} // Sentinel
};

// Plugin initialization
static bool dev_init() {
    std::cout << "  dev plugin initialized" << std::endl;
    return true;
}

// Plugin cleanup
static void dev_cleanup() {
    // Nothing to clean up
}

// Plugin interface
static PluginInterface dev_interface = {
    "dev",
    "1.0.0",
    dev_init,
    []() -> const PluginFunctionEntry* { return plugin_functions; },
    dev_cleanup
};

// Entry point - must be extern "C"
extern "C" {
    PluginInterface* get_plugin_interface() {
        return &dev_interface;
    }
}
