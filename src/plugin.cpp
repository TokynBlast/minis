#pragma once
#include <dlfcn.h>
#include <unordered_map>
#include <vector>
#include "value.hpp"
#include "sso.hpp"

namespace lang {
  using PluginFn = Value (*)(const std::vector<Value>&);
  using PluginVar = const Value*;
  
  struct PluginFunctionEntry {
    const char* name;
    PluginFn function;
    PluginVar variable;
  };
  
  struct PluginInterface {
    const char* name;
    const char* version;
    bool (*init)();
    const PluginFunctionEntry* (*get_functions)();
    void (*cleanup)();
  };
  
  struct LoadedPlugin {
    void* handle;
    PluginInterface iface;
  };
  
  static std::unordered_map<CString, LoadedPlugin> loaded;
  static std::unordered_map<CString, PluginFn> funcs;
  static std::unordered_map<CString, PluginVar> vars;
  
  class PluginManager {
  public:
    static bool load_plugin(const char* plugin_name, const char* library_path) {
      if (loaded.find(plugin_name) != loaded.end()) return true;
  
      void* handle = dlopen(library_path, RTLD_LAZY);
      if (!handle) return false;
  
      using GetIface = PluginInterface* (*)();
      GetIface get = (GetIface)dlsym(handle, "get_plugin_interface");
      if (!get) { dlclose(handle); return false; }
  
      PluginInterface* iface = get();
      if (!iface) { dlclose(handle); return false; }
  
      if (iface->init && !iface->init()) {
        dlclose(handle);
        return false;
      }
  
      LoadedPlugin p;
      p.handle = handle;
      p.iface = *iface;
  
      if (iface->get_functions) {
        const PluginFunctionEntry* f = iface->get_functions();
        for (; f->name; ++f) {
          CString full = CString(plugin_name) + "_" + f->name;
          if (f->function) funcs[full] = f->function;
          if (f->variable) vars[full] = f->variable;
        }
      }
  
      loaded[plugin_name] = p;
      return true;
    }
  
    static PluginFn get_function(const char* name) {
      auto it = funcs.find(name);
      return (it == funcs.end()) ? nullptr : it->second;
    }
  
    static PluginVar get_variable(const char* name) {
      auto it = vars.find(name);
      return (it == vars.end()) ? nullptr : it->second;
    }
  
    static bool has_function(const char* name) {
      return funcs.find(name) != funcs.end();
    }
  
    static bool has_variable(const char* name) {
      return vars.find(name) != vars.end();
    }
  
    static void cleanup() {
      for (auto& p : loaded) {
        if (p.second.iface.cleanup) p.second.iface.cleanup();
        dlclose(p.second.handle);
      }
      loaded.clear();
      funcs.clear();
      vars.clear();
    }
  };
}
