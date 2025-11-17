#include <iostream>
#include <fstream>
#include <cstring>
#include <unordered_map>
#include "../include/plugin.hpp"
#include "../include/value.hpp"
#include "../include/sso.hpp"
#include "../include/io.hpp"

using namespace lang;

static std::unordered_map<int, FILE*> fileHandles;
static int nextHandle = 1;

static Value openFile(const std::vector<Value>& args) {
    if (args.size() != 1 || args[0].t != Type::Str) return Value::N();

    const CString& filename = std::get<CString>(args[0].v);
    FILE* file = fopen(filename.c_str(), "rb+");
    if (!file) file = fopen(filename.c_str(), "wb+"); // Try to create if not exist

    if (!file) return Value::N();

    int handle = nextHandle++;
    fileHandles[handle] = file;

    return Value::I((long long)handle);
}

static FILE* getFileHandle(const Value& v) {
    int handle = (int)v.AsInt(0);
    auto it = fileHandles.find(handle);
    if (it == fileHandles.end()) return nullptr;
    return it->second;
}

static Value devWriteBytes(const std::vector<Value>& args) {
    if (args.size() < 2) return Value::N();
    FILE* file = getFileHandle(args[0]);
    if (!file || args[1].t != Type::List) return Value::N();

    const auto& bytes = std::get<std::vector<Value>>(args[1].v);

    for (const auto& byteVal : bytes) {
        if (byteVal.t == Type::Int) {
            uint8_t b = (uint8_t)std::get<long long>(byteVal.v);
            fwrite(&b, 1, 1, file);
        }
    }
    fflush(file);
    return Value::B(true);
}

// readBytes(file_handle, count)
static Value devReadBytes(const std::vector<Value>& args) {
    if (args.size() < 2) return Value::N();
    FILE* file = getFileHandle(args[0]);
    if (!file || args[1].t != Type::Int) return Value::L({});

    long long count = args[1].AsInt(0);
    if (count <= 0) return Value::L({});

    std::vector<Value> bytes;
    bytes.reserve((size_t)count);

    for (long long i = 0; i < count; ++i) {
        uint8_t b;
        if (fread(&b, 1, 1, file) != 1) break;
        bytes.push_back(Value::I((long long)b));
    }

    return Value::L(std::move(bytes));
}

static Value devEmitU16(const std::vector<Value>& args) {
    if (args.size() != 2) return Value::B(false);
    FILE* file = getFileHandle(args[0]);
    uint16_t v = (uint16_t)args[1].AsInt(0);
    if (!file) return Value::B(false);
    write_u16(file, v);
    return Value::B(!error);
}

// emitU8(file_handle, value)
static Value devEmitU8(const std::vector<Value>& args) {
    if (args.size() != 2) return Value::B(false);
    FILE* file = getFileHandle(args[0]);
    uint8_t v = (uint8_t)args[1].AsInt(0);
    if (!file) return Value::B(false);
    write_u8(file, v);
    return Value::B(!error);
}

// emitU32(file_handle, value)
static Value devEmitU32(const std::vector<Value>& args) {
    if (args.size() != 2) return Value::B(false);
    FILE* file = getFileHandle(args[0]);
    uint32_t v = (uint32_t)args[1].AsInt(0);
    if (!file) return Value::B(false);
    write_u32(file, v);
    return Value::B(!error);
}

// emitU64(file_handle, value)
static Value devEmitU64(const std::vector<Value>& args) {
    if (args.size() != 2) return Value::B(false);
    FILE* file = getFileHandle(args[0]);
    uint64_t v = (uint64_t)args[1].AsInt(0);
    if (!file) return Value::B(false);
    write_u64(file, v);
    return Value::B(!error);
}

// readU8(file_handle)
static Value devReadU8(const std::vector<Value>& args) {
    if (args.size() != 1) return Value::N();
    FILE* file = getFileHandle(args[0]);
    if (!file) return Value::N();
    uint8_t v;
    if (fread(&v, 1, 1, file) != 1) return Value::N();
    return Value::I((long long)v);
}

// readU32(file_handle)
static Value devReadU32(const std::vector<Value>& args) {
    if (args.size() != 1) return Value::N();
    FILE* file = getFileHandle(args[0]);
    if (!file) return Value::N();
    uint32_t v;
    if (!read_u32(file)) return Value::N();
    return Value::I((long long)v);
}

// readU64(file_handle)
static Value devReadU64(const std::vector<Value>& args) {
    if (args.size() != 1) return Value::N();
    FILE* file = getFileHandle(args[0]);
    if (!file) return Value::N();
    uint64_t v;
    if (!read_u64(file)) return Value::N();
    return Value::I((long long)v);
}

// emitStr(file_handle, string)
static Value devEmitStr(const std::vector<Value>& args) {
    if (args.size() != 2 || args[1].t != Type::Str) return Value::B(false);
    FILE* file = getFileHandle(args[0]);
    if (!file) return Value::B(false);

    const CString& str = std::get<CString>(args[1].v);
    uint64_t len = str.size();
    write_u64(file, len);
    if (fwrite(str.c_str(), 1, len, file) != len) return Value::B(false);
    return Value::B(true);
}

// readStr(file_handle)
static Value devReadStr(const std::vector<Value>& args) {
    if (args.size() != 1) return Value::N();
    FILE* file = getFileHandle(args[0]);
    if (!file) return Value::N();

    uint64_t len;
    read_u64(file);
    CString str;
    str.resize(len);
    if (fread(&str[0], 1, len, file) != len) return Value::N();
    return Value::S(str);
}

// moveto(file_handle, offset, whence=0)
static Value devMoveTo(const std::vector<Value>& args) {
    if (args.size() != 2 && args.size() != 3) return Value::I(-1);
    FILE* file = getFileHandle(args[0]);
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
static Value devPos(const std::vector<Value>& args) {
    if (args.size() != 1) return Value::I(-1);
    FILE* file = getFileHandle(args[0]);
    if (!file) return Value::I(-1);
    long position = ftell(file);
    return Value::I((long long)position);
}

// close(file_handle)
static Value devClose(const std::vector<Value>& args) {
    if (args.size() != 1) return Value::B(false);
    int handle = (int)args[0].AsInt(0);
    auto it = fileHandles.find(handle);
    if (it == fileHandles.end()) return Value::B(false);
    fclose(it->second);
    fileHandles.erase(it);
    return Value::B(true);
}

// typename(value)
static Value devTypeName(const std::vector<Value>& args) {
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

static const PluginFunctionEntry pluginFunctions[] = {
    {"openFile", openFile},
    {"close", devClose},
    {"writeBytes", devWriteBytes},
    {"readBytes", devReadBytes},
    {"emitU16", devEmitU16},
    {"emitU8", devEmitU8},
    {"emitU32", devEmitU32},
    {"emitU64", devEmitU64},
    {"emitStr", devEmitStr},
    {"readU8", devReadU8},
    {"readU32", devReadU32},
    {"readU64", devReadU64},
    {"readStr", devReadStr},
    {"moveTo", devMoveTo},
    {"pos", devPos},
    {"typename", devTypeName},
    {nullptr, nullptr}
};

static bool devInit() {
    std::cout << "  dev plugin initialized" << std::endl;
    return true;
}

static void devCleanup() {
    for (auto& kv : fileHandles) {
        fclose(kv.second);
    }
    fileHandles.clear();
}

static PluginInterface devInterface = {
    "dev",
    "0.0.3",
    devInit,
    []() -> const PluginFunctionEntry* { return pluginFunctions; },
    devCleanup
};

extern "C" {
    PluginInterface* get_plugin_interface() {
        return &devInterface;
    }
}
