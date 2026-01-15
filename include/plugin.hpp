#pragma once
#include <vector>
#include <string>
#include "value.hpp"

namespace minis {
  // Plugin function signature - same as built-in functions
  using PluginFn = Value(*)(const std::vector<Value>&);

  // Plugin variable signature - pointer to constant Value
  using PluginVar = const Value*;

  // Plugin function registration entry
  struct PluginFunctionEntry {
      const char* name;
      PluginFn function;
      PluginVar variable;
  };

  // Plugin interface - must be implemented by plugins
  struct PluginInterface {
      // Plugin metadata
      const char* name;
      const char* version;

      // Initialize plugin (called once on load)
      bool (*init)();

      // Get list of functions to register (NULL-terminated array)
      const PluginFunctionEntry* (*get_functions)();

      // Cleanup plugin (called on unload)
      void (*cleanup)();
  };

  // Plugin manager
  class PluginManager {
  public:
      // Load a plugin from a shared library
      static bool load_plugin(const std::string& plugin_name, const std::string& library_path);

      // Get a plugin function by name
      static PluginFn get_function(const std::string& name);

      // Get a plugin variable by name
      static PluginVar get_variable(const std::string& name);

      // Check if a function is provided by a plugin
      static bool has_function(const std::string& name);

      // Check if a variable is provided by a plugin
      static bool has_variable(const std::string& name);

      // Unload all plugins
      static void cleanup();
  };

}
