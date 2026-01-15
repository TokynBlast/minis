#ifdef _WIN32
  #include <windows.h>
#else
  #include <dlfcn.h>
#endif
#include <unordered_map>
#include <vector>
#include <string>
#include "../include/value.hpp"
#include "../include/plugin.hpp"

namespace minis {
  struct LoadedPlugin {
    void* handle;
    PluginInterface iface;
  };

  static std::unordered_map<std::string, LoadedPlugin> loaded;
  static std::unordered_map<std::string, PluginFn> funcs;
  static std::unordered_map<std::string, PluginVar> vars;

  bool PluginManager::load_plugin(const std::string& plugin_name, const std::string& library_path) {
    if (loaded.find(plugin_name) != loaded.end()) return true;

#ifdef _WIN32
    HMODULE handle = LoadLibraryA(library_path.c_str());
    if (!handle) return false;

    using GetIface = PluginInterface* (*)();
    GetIface get = (GetIface)GetProcAddress(handle, "get_plugin_interface");
    if (!get) { FreeLibrary(handle); return false; }
#else
    void* handle = dlopen(library_path.c_str(), RTLD_LAZY);
    if (!handle) return false;

    using GetIface = PluginInterface* (*)();
    GetIface get = (GetIface)dlsym(handle, "get_plugin_interface");
    if (!get) { dlclose(handle); return false; }
#endif

    PluginInterface* iface = get();
    if (!iface) {
#ifdef _WIN32
      FreeLibrary((HMODULE)handle);
#else
      dlclose(handle);
#endif
      return false;
    }

    if (iface->init && !iface->init()) {
#ifdef _WIN32
      FreeLibrary((HMODULE)handle);
#else
      dlclose(handle);
#endif
      return false;
    }

    LoadedPlugin p;
    p.handle = handle;
    p.iface = *iface;

    if (iface->get_functions) {
      const PluginFunctionEntry* f = iface->get_functions();
      for (; f->name; ++f) {
        std::string full = plugin_name + "_" + f->name;
        if (f->function) funcs[full] = f->function;
        if (f->variable) vars[full] = f->variable;
      }
    }

    loaded[plugin_name] = p;
    return true;
  }

  PluginFn PluginManager::get_function(const std::string& name) {
    auto it = funcs.find(name);
    return (it == funcs.end()) ? nullptr : it->second;
  }

  PluginVar PluginManager::get_variable(const std::string& name) {
    auto it = vars.find(name);
    return (it == vars.end()) ? nullptr : it->second;
  }

  bool PluginManager::has_function(const std::string& name) {
    return funcs.find(name) != funcs.end();
  }

  bool PluginManager::has_variable(const std::string& name) {
    return vars.find(name) != vars.end();
  }

  void PluginManager::cleanup() {
    for (auto& p : loaded) {
      if (p.second.iface.cleanup) p.second.iface.cleanup();
#ifdef _WIN32
      FreeLibrary((HMODULE)p.second.handle);
#else
      dlclose(p.second.handle);
#endif
    }
    loaded.clear();
    funcs.clear();
    vars.clear();
  }
}
