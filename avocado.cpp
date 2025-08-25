#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <thread>
#include <chrono>
#include <cstring>          // for std::strlen if minis.cpp uses it
#include "diagnostics.h"    // defines Source, ScriptError, render_diagnostic
#include "minis.cpp"        // (consider switching to a header later)

/*
TODO:
Add terminal coloring
Finish graphics
Add a micro-language
Add command-surfing (up/down history)
*/

std::vector<std::string> lines;
std::vector<std::string> prefs;

void clr() { std::cout << "\033[H\033[J"; }

static void load_prefs() {
  prefs.clear();
  std::ifstream prefFile("pref");
  std::string s;
  while (std::getline(prefFile, s)) prefs.push_back(s);
  if (prefs.size() < 1) prefs.push_back("> ");
  if (prefs.size() < 2) prefs.push_back("True");
}

static void save_prefs() {
  std::ofstream out("pref", std::ios::trunc);
  for (auto &p : prefs) out << p << '\n';
}

int main() {
  load_prefs();

  // preload current "code" file into in-memory buffer (for show/save/etc.)
  {
    std::ifstream codeFile("code");
    std::string line;
    while (std::getline(codeFile, line)) lines.push_back(line + "\n");
  }

  if (prefs.size() >= 2 && prefs[1] == "True") {
    std::cout << "\033[92mIf you need help with the Avocado IDE, type 'help'\n\033[0m";
    prefs[1] = "False";
    save_prefs();
  }

  for (;;) {
    std::string command;
    std::cout << prefs[0];
    if (!std::getline(std::cin, command)) break;

    std::string commandCs = command;
    std::transform(commandCs.begin(), commandCs.end(), commandCs.begin(), ::tolower);
/*
      clr();
      for (size_t i=0;i<lines.size();++i) {
        if (lines[i] != "\n") std::cout << (i+1) << ' ' << lines[i];
      }
      continue;
*/
    if (commandCs.rfind("show",0)==0) {
      clr();
      for (size_t i=0;i<lines.size();++i) {
        if (lines[i] != "\n") std::cout << (i+1) << ' ' << lines[i];
      }
      continue;
    }
    if (commandCs.rfind("run",0)==0) {
      // load the script text fresh from disk
      std::ifstream in("code");
      std::stringstream ss; ss << in.rdbuf();
      std::string program = ss.str();

      minis::Engine vm;                   // engine with stack/diagnostics
      Source src{"code", program};        // keep filename + line map
      try {
        vm.eval(src);                     // uses spans internally
      } catch (const ScriptError& e) {    // from diagnostics.h
        std::cerr << render_diagnostic(src, e, 1);
        if (!vm.stack.empty()){
          std::cerr << "stack trace:\n";
          for (auto it = vm.stack.rbegin(); it != vm.stack.rend(); ++it){
            auto loc = src.loc_at(it->call.beg);
            std::cerr << "  at " << it->fn << " (" << src.name
                      << ":" << loc.line << ":" << loc.col << ")\n";
          }
        }
      } catch (const std::exception& e) {
        std::cerr << "[native error] " << e.what() << "\n";
      }
      continue;
    }

    if (commandCs.rfind("clear mem",0)==0) {
      std::string rest = (commandCs.size() > 9) ? commandCs.substr(9) : std::string();
      rest.erase(rest.begin(),
                 std::find_if(rest.begin(), rest.end(),
                        [](unsigned char c){ return std::isspace(c); }));
      bool auto_yes = false;
      {
        std::istringstream iss(rest);
        std::string tok;
        while (iss >> tok) {
          if (tok == "-y") auto_yes = true; break;
        }
      }
      if (auto_yes) {
        std::ofstream codeFile("code", std::ios::trunc);
        lines.clear();
        std::cout << "Memory cleared.\n";
        continue;
      }
      std::cout << "Are you sure you want to clear the memory? This is irreversible. [Y/n]\n";
      std::string confirm;
      if (!std::getline(std::cin, confirm)) break;
      std::transform(confirm.begin(), confirm.end(), confirm.begin(), ::tolower);
      if (confirm == "y") {
        std::ofstream codeFile("code", std::ios::trunc);
        lines.clear();
        std::cout << "Memory cleared.\n";
      }
      continue;
    }
    if (commandCs.rfind("cls",0)==0) { clr(); continue; }
    if (commandCs.rfind("exit",0)==0) { return 0; }

    if (commandCs.rfind("save ",0)==0) {
      std::string fname = command.substr(5);
      if (fname.empty()) { std::cout << "The file name can't be blank.\n"; continue; }
      std::ofstream saveFile(fname, std::ios::trunc);
      if (!saveFile) { std::cout << "Could not open " << fname << " for writing.\n"; continue; }
      for (const auto &l : lines) saveFile << l;
      continue;
    }

    if (commandCs.rfind("help",0)==0) {
      std::ifstream guideFile("guide.txt");
      std::string gl;
      if (!guideFile) { std::cout << "No guide.txt found.\n"; continue; }
      while (std::getline(guideFile, gl)) std::cout << gl << '\n';
      continue;
    }

    if (commandCs.rfind("replace",0)==0) {
      std::string args = command.substr(8); // "replace " already matched
      size_t sep = args.find(" || ");
      if (sep == std::string::npos) { std::cout << "Usage: replace OLD || NEW\n"; continue; }
      std::string oldStr = args.substr(0, sep);
      std::string newStr = args.substr(sep + 4);
      for (auto &l : lines) {
        size_t pos = l.find(oldStr);
        if (pos != std::string::npos) l.replace(pos, oldStr.size(), newStr);
      }
      std::ofstream codeFile("code", std::ios::trunc);
      for (const auto &l : lines) codeFile << l;
      continue;
    }

    if (commandCs.rfind("load",0)==0) {
      std::ifstream loadFile(command.substr(5));
      if (!loadFile) { std::cout << "Could not open that file.\n"; continue; }
      lines.clear();
      std::string l;
      while (std::getline(loadFile, l)) lines.push_back(l + "\n");
      std::ofstream codeFile("code", std::ios::trunc);
      for (const auto &ln : lines) codeFile << ln;
      continue;
    }

    if (commandCs.rfind("info",0)==0) {
      std::ifstream file("code", std::ios::binary);
      std::streamsize file_size = 0;
      if (file) { file.seekg(0, std::ios::end); file_size = file.tellg(); }
      std::cout << "Lines: " << lines.size() << "\nBytes: " << file_size << "\n";
      continue;
    }

    // default: "N content" to set a line
    auto sp = command.find(' ');
    if (sp == std::string::npos) { std::cout << "Unrecognized command.\n"; continue; }
    try {
      int n = std::stoi(command.substr(0, sp));
      std::string content = command.substr(sp + 1) + "\n";
      if (n <= 0) { std::cout << "Line numbers start at 1.\n"; continue; }
      if ((size_t)n <= lines.size()) lines[n-1] = content;
      else { lines.resize(n-1, "\n"); lines.push_back(content); }
      std::ofstream codeFile("code", std::ios::trunc);
      for (const auto &l : lines) codeFile << l;
    } catch (...) {
      std::cout << "Invalid line command.\n";
    }
  }
}
