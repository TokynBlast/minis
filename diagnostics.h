// diagnostics.h
#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <cctype>

struct Span { size_t beg=0, end=0; };          // [beg, end)
struct Loc  { int line=1, col=1; };            // 1-based

struct Source {
    std::string name;          // filename or "<memory>"
    std::string text;
    std::vector<size_t> line_starts; // byte offsets of each line start

    Source(std::string name_, std::string text_)
        : name(std::move(name_)), text(std::move(text_)) {
        line_starts.push_back(0);
        for (size_t i=0;i<text.size();++i) if (text[i]=='\n') line_starts.push_back(i+1);
    }
    size_t line_count() const { return line_starts.empty()?0:line_starts.size(); }

    Loc loc_at(size_t index) const {          // O(log n)
        if (index > text.size()) index = text.size();
        auto it = std::upper_bound(line_starts.begin(), line_starts.end(), index);
        size_t ln = (it==line_starts.begin()?0: (it - line_starts.begin()) - 1);
        size_t col0 = index - line_starts[ln];
        return { (int)ln+1, (int)col0+1 };
    }

    std::string line_str(int ln) const {      // 1-based
        if (ln<1 || (size_t)ln>line_starts.size()) return {};
        size_t s = line_starts[ln-1];
        size_t e = (size_t)ln<line_starts.size()? line_starts[ln] : text.size();
        // trim trailing '\n'
        if (e> s && text[e-1]=='\n') --e;
        return text.substr(s, e-s);
    }
};

struct ScriptError : std::exception {
    std::string message;
    Span span;
    std::vector<std::string> notes;        // extra notes
    std::vector<std::string> suggestions;  // “did you mean …”
    ScriptError(std::string msg, Span sp): message(std::move(msg)), span(sp) {}
    const char* what() const noexcept override { return message.c_str(); }
};

// ---------- helpers to infer a span when it's empty ----------

inline static bool is_ident_char(char c){
    return std::isalnum((unsigned char)c) || c=='_' || c=='.';
}

inline static size_t find_last_call_site_outside_comments(const std::string& s, const std::string& name){
    const size_t n = s.size();
    size_t last = std::string::npos;

    bool in_line=false, in_block=false, in_str=false;
    char str_q=0;

    for (size_t i=0; i<n; ++i){
        char c = s[i];

        // handle exiting states first
        if (in_line){
            if (c=='\n') in_line=false;
            continue;
        }
        if (in_block){
            if (c=='*' && i+1<n && s[i+1]=='/'){ in_block=false; ++i; }
            continue;
        }
        if (in_str){
            if (c=='\\'){ if (i+1<n) ++i; continue; }
            if (c==str_q){ in_str=false; }
            continue;
        }

        // entering comments/strings
        if (c=='/' && i+1<n && s[i+1]=='/'){ in_line=true; ++i; continue; }
        if (c=='/' && i+1<n && s[i+1]=='*'){ in_block=true; ++i; continue; }
        if (c=='"' || c=='\''){ in_str=true; str_q=c; continue; }

        // check potential function name match at i
        if (c == name[0]){
            // ensure enough room
            if (i + name.size() <= n && s.compare(i, name.size(), name) == 0){
                // left boundary: not part of a longer identifier
                if (i>0 && is_ident_char(s[i-1])) { /* skip */ }
                else {
                    // skip spaces to find '('
                    size_t j = i + name.size();
                    while (j<n && std::isspace((unsigned char)s[j])) ++j;
                    if (j<n && s[j]=='('){
                        last = i;
                    }
                }
            }
        }
    }
    return last;
}

inline static bool extract_note_callee(const std::vector<std::string>& notes, std::string& outName){
    // look for: in call to 'Name'
    for (auto it = notes.rbegin(); it != notes.rend(); ++it){
        const std::string& s = *it;
        const std::string key = "in call to '";
        auto p = s.find(key);
        if (p != std::string::npos){
            auto q = s.find('\'', p + key.size());
            if (q != std::string::npos){
                outName = s.substr(p + key.size(), q - (p + key.size()));
                return !outName.empty();
            }
        }
    }
    return false;
}

inline static bool extract_name_after_prefix(const std::string& msg, const char* prefix, std::string& out){
    auto p = msg.find(prefix);
    if (p == std::string::npos) return false;
    size_t i = p + std::strlen(prefix);
    // trim leading spaces
    while (i < msg.size() && std::isspace((unsigned char)msg[i])) ++i;
    // read identifier-ish text
    size_t s0 = i;
    while (i < msg.size() && (is_ident_char(msg[i]))) ++i;
    if (i > s0){
        out = msg.substr(s0, i - s0);
        return true;
    }
    return false;
}

inline static bool infer_empty_span(const Source& src, const ScriptError& err, Span& outSpan){
    // 1) Try to infer from "in call to 'Name'"
    std::string callee;
    if (extract_note_callee(err.notes, callee)){
        size_t pos = find_last_call_site_outside_comments(src.text, callee);
        if (pos != std::string::npos){
            outSpan = { pos, pos + callee.size() };
            return true;
        }
    }

    // 2) Try message-based identifiers
    std::string id;
    if (extract_name_after_prefix(err.message, "unknown function:", id) ||
        extract_name_after_prefix(err.message, "unknown variable:", id) ||
        extract_name_after_prefix(err.message, "variable already declared:", id))
    {
        // Find last occurrence outside comments/strings (best effort)
        size_t pos = find_last_call_site_outside_comments(src.text, id);
        if (pos == std::string::npos){
            // fall back to plain search of last token "id" (not necessarily a call)
            size_t last = std::string::npos;
            for (size_t p = 0; ; ){
                p = src.text.find(id, p);
                if (p == std::string::npos) break;
                // ensure token boundary
                bool leftOk  = (p==0) || !is_ident_char(src.text[p-1]);
                bool rightOk = (p+id.size()>=src.text.size()) || !is_ident_char(src.text[p+id.size()]);
                if (leftOk && rightOk) last = p;
                ++p;
            }
            pos = last;
        }
        if (pos != std::string::npos){
            outSpan = { pos, pos + id.size() };
            return true;
        }
    }

    // 3) Give up
    return false;
}

// Render: one primary span, + up to N notes/suggestions.
inline std::string render_diagnostic(const Source& src, const ScriptError& err, int context_lines=1) {
    Span use = err.span;

    // Heuristic: if span is "empty" (0..0), try to infer a better one
    if (use.beg==0 && use.end==0){
        Span inferred{};
        if (infer_empty_span(src, err, inferred)){
            use = inferred;
        }
    }

    // Clamp span to valid range
    if (use.beg > src.text.size()) use.beg = src.text.size();
    if (use.end > src.text.size()) use.end = use.beg;

    std::ostringstream out;
    auto beg = src.loc_at(use.beg);
    out << src.name << ":" << beg.line << ":" << beg.col << ": error: " << err.message << "\n";

    // Show context around the primary line
    int l0 = std::max(1, beg.line - context_lines);
    int l1 = std::min((int)src.line_count(), beg.line + context_lines);

    for (int ln = l0; ln <= l1; ++ln) {
        auto s = src.line_str(ln);
        out << " " << ln << " | " << s << "\n";
        if (ln == beg.line) {
            // underline range on this line
            size_t line_start = src.line_starts[ln-1];
            int col_beg = (int)(use.beg - line_start) + 1;
            int col_end = (int)(use.end - line_start) + 1;
            if (col_beg < 1) col_beg = 1;
            if (col_end < col_beg) col_end = col_beg;
            out << "   " << std::string(std::to_string(ln).size(), ' ')
                << " | " << std::string(col_beg-1, ' ')
                << "^" << std::string(std::max(0, col_end - col_beg), '~') << "\n";
        }
    }
    for (auto& n : err.notes) out << "note: " << n << "\n";
    for (auto& s : err.suggestions) out << "help: did you mean '" << s << "'?\n";
    return out.str();
}
