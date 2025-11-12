#include <unordered_map>
#include <cstring>
#include <cstdlib>
#include "../include/token.hpp"
#include "../include/context.hpp"
#include "../include/ugly.hpp"
#include "../include/sso.hpp"

using namespace lang;

// Rename all variables

static char* gensym_name(uint64_t n) {
  uint64_t remaining = n;
  uint64_t len = 1;
  uint64_t block = 26;
  while (remaining >= block) { remaining -= block; ++len; block *= 26ULL; }

  char* out = (char*)malloc(len + 1);
  if (!out) return nullptr;

  for (uint64_t i = 0; i < len; ++i) {
    uint64_t d = remaining % 26ULL;
    out[i] = 'a' + d;
    remaining /= 26ULL;
  }
  out[len] = '\0';
  return out;
}

struct MinifierPlan {  // Changed to a completely different name
  std::unordered_map<CString, char*> id2mini;
  uint64_t counter = 0;

  ~MinifierPlan() {
    // Free all allocated C strings
    for (auto& pair : id2mini) {
      free(pair.second);
    }
  }

  const char* ensure(const CString& name) {
    auto it = id2mini.find(name);
    if (it != id2mini.end()) return it->second;
    char* alias = gensym_name(counter++);
    if (!alias) return nullptr;
    id2mini.emplace(name, alias);
    return alias;
  }
};

static char* rebuild(const std::vector<Token>& ts, const MinifierPlan& plan) {
  auto need_space = [](const Token& a, const Token& b) -> bool {
    auto idlike = [](Tok k) { return k == Tok::Id || k == Tok::Num; };
    return idlike(a.k) && idlike(b.k);
  };

  size_t estimated_size = ts.size() * 4;
  char* out = (char*)malloc(estimated_size);
  if (!out) return nullptr;

  size_t out_pos = 0;
  Token prev{Tok::Sym, ""};

  for (size_t i = 0; i < ts.size(); ++i) {
    const Token& t = ts[i];
    if (t.k == Tok::Eof) break;
    if (t.k == Tok::WS) continue;

    const char* chunk = nullptr;

    switch (t.k) {
      case Tok::Id: {
        if (t.k == Tok::Id && !is_builtin(t.text)) {
          auto it = plan.id2mini.find(t.text);
          chunk = (it != plan.id2mini.end()) ? it->second : t.text.c_str();
        } else {
          chunk = t.text.c_str();
        }
      } break;
      case Tok::Str:
      case Tok::Num:
      case Tok::Sym:
        chunk = t.text.c_str();
        break;
      default: 
        break;
    }

    if (chunk) {
      size_t chunk_len = strlen(chunk);

      // Check if we need space
      if (out_pos > 0 && need_space(prev, t)) {
        if (out_pos + 1 >= estimated_size) {
          estimated_size *= 2;
          out = (char*)realloc(out, estimated_size);
          if (!out) return nullptr;
        }
        out[out_pos++] = ' ';
      }

      // Check if we have enough space
      if (out_pos + chunk_len >= estimated_size) {
        estimated_size = out_pos + chunk_len + 1;
        estimated_size *= 2;
        out = (char*)realloc(out, estimated_size);
        if (!out) return nullptr;
      }

      strcpy(out + out_pos, chunk);
      out_pos += chunk_len;
    }
    prev = t;
  }

  out[out_pos] = '\0';
  return out;
}

static MinifierPlan uglify(const std::vector<Token>& ts) {
  MinifierPlan plan;
  for (size_t i = 0; i + 1 < ts.size(); ++i) {
    if (ts[i].k == Tok::Id && ts[i].text == "func") {
      size_t j = i + 1;
      if (ts[j].k == Tok::Id && (ts[j].text == "void" || ts[j].text == "int" || ts[j].text == "float" ||
                                 ts[j].text == "bool" || ts[j].text == "str" || ts[j].text == "list")) { ++j; }
      while (ts[j].k == Tok::WS) ++j;
      if ((ts[j].k == Tok::Id) && !is_builtin(ts[j].text)) {
        plan.ensure(ts[j].text);
      }
    }
    if (ts[i].k == Tok::Id && ts[i].text == "let") {
      size_t j = i + 1;
      while (ts[j].k == Tok::Id && (ts[j].text == "const" || ts[j].text == "static")) ++j;
      if (ts[j].k == Tok::Id && (ts[j].text == "auto" || ts[j].text == "null" || ts[j].text == "int" || ts[j].text == "float" ||
                                 ts[j].text == "bool" || ts[j].text == "str" || ts[j].text == "list")) ++j;
      while (ts[j].k == Tok::WS) ++j;
      if ((ts[j].k == Tok::Id) && !is_builtin(ts[j].text)) {
        plan.ensure(ts[j].text);
      }
    }
  }
  return plan;
}

// Public API functions that match your header
char* Ugly(const std::vector<Token>& ts) {
  MinifierPlan plan = uglify(ts);
  return rebuild(ts, plan);
}