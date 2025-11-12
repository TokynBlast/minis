#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "token.hpp"

namespace lang {

struct Ugly;
using UglyPlan = Ugly;

UglyPlan* uglify_tokens(const Token* tokens, std::size_t token_count);

UglyPlan* ugly_create();
void ugly_destroy(UglyPlan* plan);

const char* ugly_ensure_c(UglyPlan* plan, const char* name);

char* ugly_rebuild_c(UglyPlan* plan, const Token* tokens, std::size_t token_count);

int is_builtin_c(const char* id);

}
