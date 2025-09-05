#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <thread>
#include <chrono>
#include <filesystem>
#include <cstring>
#include "diagnostics.h" // defines Source, ScriptError, render_diagnostic
#include "minis.hpp"

static std::string CompactDiagnostic(const std::string& d) {
    std::istringstream is(d);
    std::string out, prev, line;
    bool havePrev = false;

    auto compressSpaces = [](const std::string& s){
        std::string r; r.reserve(s.size());
        int run = 0;
        for (char ch : s) {
            if (ch == ' ') { if (++run <= 4) r.push_back(' '); }
            else { run = 0; r.push_back(ch); }
        }
        return r;
    };

    while (std::getline(is, line)) {
        // detect caret line (spaces then ^)
        size_t caret = std::string::npos;
        {
            bool onlySpacesAndCaret = true;
            size_t i = 0;
            while (i < line.size() && line[i] == ' ') ++i;
            if (i < line.size() && line[i] == '^') caret = i; else caret = std::string::npos;
        }

        if (havePrev && caret != std::string::npos) {
            // prev = source line, line = caret alignment
            const std::string& src = prev;

            // use caret column; clamp snippet to ±20 chars
            size_t col = caret + 1;                 // 1-based
            size_t start = (col > 20 ? col - 21 : 0);
            size_t end   = std::min(src.size(), start + 41);
            std::string snippet = src.substr(start, end - start);

            // add ellipses if trimmed
            if (start > 0)         out += "…";
            out += snippet;
            if (end < src.size())  out += "…";
            out += "\n";

            // caret for the snippet
            size_t caretInSnippet = (col - 1) - start;
            out += std::string(caretInSnippet, ' ');
            out += "^\n";
            out += "  (column " + std::to_string(col) + ")\n";

            // we consumed the source+caret pair; reset prev
            havePrev = false;
            prev.clear();
            continue;
        }

        // normal line: lightly compress long space runs
        out += compressSpaces(line);
        out += "\n";
        prev = line;
        havePrev = true;
    }
    return out;
}


#ifdef _WIN32
  #include <windows.h>
  static void SetCwdToExeDir() {
    wchar_t pathW[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, pathW, MAX_PATH);
    if (n == 0 || n == MAX_PATH) return;
    std::filesystem::path exe = pathW;
    std::error_code ec;
    std::filesystem::current_path(exe.parent_path(), ec);
  }
#else
  #include <unistd.h>
  #include <limits.h>
  static void SetCwdToExeDir() {
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf)-1);
    if (n > 0) {
      buf[n] = '\0';
      std::error_code ec;
      std::filesystem::current_path(std::filesystem::path(buf).parent_path(), ec);
    }
  }
#endif


/*
TODO:
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
  SetCwdToExeDir();
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
    if (commandCs.rfind("dir",0)==0){
      std::error_code ec;
      auto cwd = std::filesystem::current_path(ec);
      std::cout << "[Avocado] Working directory: "
                << (ec ? "<unknown>" : cwd.string()) << "\n";

      std::cout << "[Avocado] code:      "
                << std::filesystem::absolute("code", ec).string() << "\n";
      std::cout << "[Avocado] guide.txt: "
                << std::filesystem::absolute("guide.txt", ec).string() << "\n";
      std::cout << "[Avocado] pref:      "
                << std::filesystem::absolute("pref", ec).string() << "\n\n";
      std::cout.flush();
    }

    if (commandCs.rfind("run", 0) == 0) {
        // Load current buffer from disk
        std::ifstream in("code", std::ios::binary);
        if (!in) {
            std::cerr << "[Avocado] No file named 'code' in the working directory.\n";
            continue;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        std::string program = ss.str();

        try {
            // Compile Minis source -> bytecode on disk (use .Ms as requested)
            minis::compile_file_to_avocado("code", program, "code.ms");
            // Run the produced bytecode
            minis::run_avocado("code.ms");
        }
        catch (const minis::ScriptError& me) {
            // Adapt minis::ScriptError -> diagnostics.h::ScriptError for caret output
            ScriptError se{ std::string(me.what()), Span{ me.span.beg, me.span.end } };
            for (const auto& n : me.notes) se.notes.push_back(n);
            std::string diag = render_diagnostic(Source{"code", program}, se, 1);
            std::cerr << CompactDiagnostic(diag);
        }
        catch (const ScriptError& e) {
            // Also handle your diagnostics ScriptError directly
            std::string diag = render_diagnostic(Source{"code", program}, e, 1);
            std::cerr << CompactDiagnostic(diag);
        }
        catch (const std::exception& e) {
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
    if (commandCs.rfind("exit",0)==0) { std::cout << "\x1b[?1000l\x1b[?1006l" << std::flush; return 0; }

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
