#include <dlfcn.h>
#include <unordered_map>
#include <iostream>
#include "../include/plugin.hpp"

namespace lang {

// Global storage for loaded plugins
struct LoadedPlugin {
    void* handle;
    PluginInterface interface;
    std::unordered_map<CString, PluginFn> functions;
};

static std::unordered_map<CString, LoadedPlugin> loaded_plugins;
static std::unordered_map<CString, PluginFn> plugin_functions;

bool PluginManager::load_plugin(const char* plugin_name, const char* library_path) {
    // Check if already loaded
    if (loaded_plugins.find(plugin_name) != loaded_plugins.end()) {
        return true; // Already loaded
    }
    
    // Load shared library
    void* handle = dlopen(library_path, RTLD_LAZY);
    if (!handle) {
        std::cerr << "Failed to load plugin " << plugin_name << ": " << dlerror() << std::endl;
        return false;
    }
    
    // Get plugin interface
    typedef PluginInterface* (*GetPluginFunc)();
    GetPluginFunc get_plugin = (GetPluginFunc)dlsym(handle, "get_plugin_interface");
    if (!get_plugin) {
        std::cerr << "Plugin " << plugin_name << " missing get_plugin_interface()" << std::endl;
        dlclose(handle);
        return false;
    }
    
    PluginInterface* iface = get_plugin();
    if (!iface) {
        std::cerr << "Plugin " << plugin_name << " returned NULL interface" << std::endl;
        dlclose(handle);
        return false;
    }
    
    // Initialize plugin
    if (iface->init && !iface->init()) {
        std::cerr << "Plugin " << plugin_name << " initialization failed" << std::endl;
        dlclose(handle);
        return false;
    }
    
    // Register functions
    LoadedPlugin plugin;
    plugin.handle = handle;
    plugin.interface = *iface;
    
    if (iface->get_functions) {
        const PluginFunctionEntry* funcs = iface->get_functions();
        for (int i = 0; funcs[i].name != nullptr; ++i) {
            CString fname = CString(plugin_name) + "_" + funcs[i].name + "_";
            plugin.functions[fname] = funcs[i].function;
            plugin_functions[fname] = funcs[i].function;
            std::cout << "  Registered plugin function: " << fname.c_str() << std::endl;
        }
    }
    
    loaded_plugins[plugin_name] = std::move(plugin);
    std::cout << "✓ Loaded plugin: " << plugin_name << " v" << iface->version << std::endl;
    return true;
}

PluginFn PluginManager::get_function(const char* name) {
    auto it = plugin_functions.find(name);
    if (it != plugin_functions.end()) {
        return it->second;
    }
    return nullptr;
}

bool PluginManager::has_function(const char* name) {
    return plugin_functions.find(name) != plugin_functions.end();
}

void PluginManager::cleanup() {
    for (auto& pair : loaded_plugins) {
        if (pair.second.interface.cleanup) {
            pair.second.interface.cleanup();
        }
        dlclose(pair.second.handle);
    }
    loaded_plugins.clear();
    plugin_functions.clear();
}

} // namespace lang
