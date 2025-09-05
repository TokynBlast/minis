// vimish_minis_embed.cpp — Vim-ish terminal editor, **Minis-only**, compiles/runs via minis.hpp
// Build:  g++ -std=c++17 -O2 -Wall -Wextra -o vimish vimish_minis_embed.cpp
// Run:    ./vimish [filename]
//
// Added per request:
// - Clear screen with "\x1b[2J\x1b[H":
//     • on entering vimish (already)      • on :run (before executing)      • on exit
// - :open <file>   -> open file into editor (warns if buffer is dirty; use :open! to force)
// - :decompile     -> view bytecode (ENTER exits)
// - :help          -> view guide.txt (ENTER exits)
//
// Modes & keys:
// - NORMAL: h j k l, arrows, 0, $, x, dd, i, a, A, o, O, :, u, p
// - INSERT: type, Backspace, Enter, Esc
// - Commands: :w, :w <file>, :q, :q!, :wq/:x, :run, :compile [out.ms], :decompile [bc.ms], :help, :settings, :open <file>, :open! <file>
//
// Minis integration uses "minis.hpp" (header-only) directly: Compiler + VM.
// Optional env overrides if you prefer your own pipeline: VIMISH_MINIS_COMPILE / VIMISH_MINIS_RUN.

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <clocale>
#include <memory>
#include <map>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <iterator>

#ifdef _WIN32
  #define NOMINMAX
  #include <windows.h>
  #include <io.h>
  #include <fcntl.h>
  #include <direct.h>
#else
  #include <sys/ioctl.h>
  #include <termios.h>
  #include <unistd.h>
  #include <libgen.h>
  #include <sys/types.h>
  #include <sys/stat.h>
#endif

#include "minis.hpp"  // your Minis header (Compiler, VM, read_* helpers, etc.)

static inline bool is_printable(unsigned char c){ return c >= 32 && c <= 126; }

enum Key {
  KEY_NULL = 0,
  KEY_ESC = 27,
  KEY_ARROW_LEFT = 1000,
  KEY_ARROW_RIGHT,
  KEY_ARROW_UP,
  KEY_ARROW_DOWN,
  KEY_DEL,
  KEY_HOME,
  KEY_END,
  KEY_PAGE_UP,
  KEY_PAGE_DOWN,
  KEY_BACKSPACE = 127
};

struct Term {
  int screen_rows = 24;
  int screen_cols = 80;
#ifdef _WIN32
  DWORD orig_in_mode = 0;
  DWORD orig_out_mode = 0;
  HANDLE hin = INVALID_HANDLE_VALUE;
  HANDLE hout = INVALID_HANDLE_VALUE;

  void enable_raw(){
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stdin),  _O_BINARY);
    hin = GetStdHandle(STD_INPUT_HANDLE);
    hout = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m;
    GetConsoleMode(hin, &m); orig_in_mode = m;
    m &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    m |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    SetConsoleMode(hin, m);
    GetConsoleMode(hout, &m); orig_out_mode = m;
    m |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hout, m);
    write("\x1b[2J\x1b[H\x1b[?25l", 12); // clear + home + hide cursor
    update_winsize();
  }
  void disable_raw(){
    if (hin) SetConsoleMode(hin, orig_in_mode);
    if (hout) SetConsoleMode(hout, orig_out_mode);
    write("\x1b[?25h", 6);
  }
  void write(const char* s, size_t n){ DWORD w; WriteConsoleA(hout, s, (DWORD)n, &w, nullptr); }
  void write(const std::string& s){ DWORD w; WriteConsoleA(hout, s.c_str(), (DWORD)s.size(), &w, nullptr); }
  int read_key(){
    INPUT_RECORD rec; DWORD nread=0;
    while (ReadConsoleInputA(hin, &rec, 1, &nread)){
      if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown){
        auto &k = rec.Event.KeyEvent;
        if (k.uChar.AsciiChar) return (unsigned char)k.uChar.AsciiChar;
        switch (k.wVirtualKeyCode){
          case VK_LEFT: return KEY_ARROW_LEFT;
          case VK_RIGHT: return KEY_ARROW_RIGHT;
          case VK_UP: return KEY_ARROW_UP;
          case VK_DOWN: return KEY_ARROW_DOWN;
          case VK_HOME: return KEY_HOME;
          case VK_END: return KEY_END;
          case VK_DELETE: return KEY_DEL;
          case VK_BACK: return KEY_BACKSPACE;
        }
      }
    }
    return KEY_NULL;
  }
  void update_winsize(){
    CONSOLE_SCREEN_BUFFER_INFO i; GetConsoleScreenBufferInfo(hout, &i);
    screen_cols = i.srWindow.Right - i.srWindow.Left + 1;
    screen_rows = i.srWindow.Bottom - i.srWindow.Top + 1;
  }
#else
  termios orig{};
  void enable_raw(){
    if (!isatty(STDIN_FILENO)){
      fprintf(stderr, "stdin is not a TTY.\n");
      exit(1);
    }
    if (tcgetattr(STDIN_FILENO, &orig) == -1){ perror("tcgetattr"); exit(1); }
    termios raw = orig;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1){ perror("tcsetattr"); exit(1); }
    write("\x1b[2J\x1b[H\x1b[?25l", 12); // clear + home + hide cursor
    update_winsize();
  }
  void disable_raw(){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
    write("\x1b[?25h", 6);
  }
  void write(const char* s, size_t n){ ::write(STDOUT_FILENO, s, n); }
  void write(const std::string& s){ ::write(STDOUT_FILENO, s.c_str(), s.size()); }
  int read_key(){
    char c;
    while (true){
      ssize_t n = ::read(STDIN_FILENO, &c, 1);
      if (n == 1) break;
      if (n == -1 && errno != EAGAIN) exit(1);
    }
    if (c == '\x1b'){
      char seq[3];
      if (::read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_ESC;
      if (::read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_ESC;
      if (seq[0] == '['){
        if (seq[1] >= '0' && seq[1] <= '9'){
          char seq2;
          if (::read(STDIN_FILENO, &seq2, 1) != 1) return KEY_ESC;
          if (seq2 == '~'){
            switch (seq[1]){
              case '1': return KEY_HOME;
              case '3': return KEY_DEL;
              case '4': return KEY_END;
              case '5': return KEY_PAGE_UP;
              case '6': return KEY_PAGE_DOWN;
              case '7': return KEY_HOME;
              case '8': return KEY_END;
            }
          }
        } else {
          switch (seq[1]){
            case 'A': return KEY_ARROW_UP;
            case 'B': return KEY_ARROW_DOWN;
            case 'C': return KEY_ARROW_RIGHT;
            case 'D': return KEY_ARROW_LEFT;
            case 'H': return KEY_HOME;
            case 'F': return KEY_END;
          }
        }
      } else if (seq[0] == 'O'){
        switch (seq[1]){
          case 'H': return KEY_HOME;
          case 'F': return KEY_END;
        }
      }
      return KEY_ESC;
    }
    return (unsigned char)c;
  }
  void update_winsize(){
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
      screen_cols = 80; screen_rows = 24;
    } else { screen_cols = ws.ws_col; screen_rows = ws.ws_row; }
  }
#endif
} term;

static inline void clear_now(){ term.write("\x1b[2J\x1b[H"); }

static std::string replace_all(std::string s, const std::string& a, const std::string& b){
  size_t p=0; while((p=s.find(a,p))!=std::string::npos){ s.replace(p,a.size(),b); p+=b.size(); } return s;
}

static std::string tmp_ms_path(){
#ifdef _WIN32
  char tmpdir[MAX_PATH]; GetTempPathA(MAX_PATH, tmpdir);
  char tmpfile[MAX_PATH]; GetTempFileNameA(tmpdir, "VMSH", 0, tmpfile);
  std::string s = tmpfile;
  size_t dot = s.find_last_of('.'); if (dot!=std::string::npos) s.erase(dot);
  s += ".ms"; return s;
#else
  static unsigned long counter = 0; counter++;
  char buf[256];
  snprintf(buf, sizeof(buf), "/tmp/vimish-%ld-%lu.ms", (long)getpid(), counter);
  return std::string(buf);
#endif
}

// ===== Minis integration (embedded) =====
static bool minis_compile_to(const std::string& srcPath, const std::string& outPath, std::string& err){
  try{
    std::ifstream in(srcPath, std::ios::binary);
    if(!in){ err = "cannot open source"; return false; }
    std::string text;
    text.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    minis::Source S{srcPath, std::move(text)};
    minis::Builtins B;
    minis::Compiler C(S, &B);
    C.compileToFile(outPath);
    return true;
  }catch(const std::exception& e){
    err = e.what();
    return false;
  }
}

static bool minis_run_bc(const std::string& bcPath, std::string& err){
  try{
    minis::Builtins B;
    minis::VM vm(&B);
    vm.load(bcPath);
    vm.run();
    return true;
  }catch(const std::exception& e){
    err = e.what();
    return false;
  }
}

// ----- Disassembler (decompiler to readable ops) -----
static const char* op_name(uint8_t op){
  using namespace minis;
  switch ((minis::Op)op){
    case minis::OP_NOP: return "NOP";
    case minis::OP_PUSH_I: return "PUSH_I";
    case minis::OP_PUSH_F: return "PUSH_F";
    case minis::OP_PUSH_B: return "PUSH_B";
    case minis::OP_PUSH_S: return "PUSH_S";
    case minis::OP_MAKE_LIST: return "MAKE_LIST";
    case minis::OP_GET: return "GET";
    case minis::OP_SET: return "SET";
    case minis::OP_DECL: return "DECL";
    case minis::OP_POP: return "POP";
    case minis::OP_ADD: return "ADD";
    case minis::OP_SUB: return "SUB";
    case minis::OP_MUL: return "MUL";
    case minis::OP_DIV: return "DIV";
    case minis::OP_EQ: return "EQ";
    case minis::OP_NE: return "NE";
    case minis::OP_LT: return "LT";
    case minis::OP_LE: return "LE";
    case minis::OP_GT: return "GT";
    case minis::OP_GE: return "GE";
    case minis::OP_AND: return "AND";
    case minis::OP_OR: return "OR";
    case minis::OP_UNSET: return "UNSET";
    case minis::OP_JMP: return "JMP";
    case minis::OP_JF: return "JF";
    case minis::OP_CALL_BUILTIN: return "CALL_BUILTIN";
    case minis::OP_CALL_USER: return "CALL_USER";
    case minis::OP_MOUSE: return "MOUSE";
    case minis::OP_RET: return "RET";
    case minis::OP_RET_VOID: return "RET_VOID";
    case minis::OP_HALT: return "HALT";
    default: return "??";
  }
}

static std::string type_name(minis::Type t){
  switch(t){
    case minis::Type::Int:   return "int";
    case minis::Type::Float: return "float";
    case minis::Type::Bool:  return "bool";
    case minis::Type::Str:   return "str";
    case minis::Type::List:  return "list";
    default: return "?";
  }
}

static std::string qstr(const std::string& s){
  std::ostringstream o; o << '"';
  for (unsigned char c : s){
    if (c=='"' || c=='\\') { o << '\\' << (char)c; }
    else if (c >= 32 && c <= 126) { o << (char)c; }
    else { o << "\\x" << std::hex << std::setw(2) << std::setfill('0') << (int)c << std::dec; }
  }
  o << '"'; return o.str();
}

struct FnMeta { std::string name; uint64_t entry=0; bool isVoid=false; bool typed=false; minis::Type ret=minis::Type::Int; std::vector<std::string> params; };

static std::vector<std::string> decompile_to_lines(const std::string& path, std::string& err){
  using namespace minis;
  std::vector<std::string> out;

  FILE* f = fopen(path.c_str(), "rb");
  if (!f){ err = "cannot open bytecode"; return {}; }
  auto closeF = [&](){ fclose(f); };

  char magic[9]={0}; fread(magic,1,8,f); if(std::string(magic)!="AVOCADO1"){ closeF(); err="bad bytecode magic"; return {}; }
  uint64_t table_off = read_u64(f);
  uint64_t fnCount   = read_u64(f);
  uint64_t entry_main= read_u64(f);
  uint64_t code_start= ftell(f);
  uint64_t code_end  = table_off;

  // read function table
  fseek(f, (long)table_off, SEEK_SET);
  std::vector<FnMeta> fns; fns.reserve((size_t)fnCount);
  for(uint64_t i=0;i<fnCount;++i){
    FnMeta m;
    m.name   = read_str(f);
    m.entry  = read_u64(f);
    m.isVoid = read_u8(f)!=0;
    m.typed  = read_u8(f)!=0;
    m.ret    = (Type)read_u8(f);
    uint64_t pcnt = read_u64(f);
    for(uint64_t j=0;j<pcnt;++j) m.params.push_back(read_str(f));
    fns.push_back(std::move(m));
  }
  // map entry->name for labels
  std::map<uint64_t, FnMeta> byEntry;
  for (auto& m : fns) byEntry[m.entry] = m;

  // disassemble code
  fseek(f, (long)code_start, SEEK_SET);
  uint64_t ip = code_start;
  out.push_back(std::string(";; entry_main=") + std::to_string(entry_main) + "  fnCount=" + std::to_string(fnCount));

  while (ip < code_end){
    auto it = byEntry.find(ip);
    if (it != byEntry.end()){
      std::ostringstream l;
      l << "\n" << it->second.name << "(";
      for (size_t i=0;i<it->second.params.size();++i){ if(i) l<<","; l<<it->second.params[i]; }
      l << ") -> " << (it->second.isVoid? "void" : (it->second.typed? type_name(it->second.ret) : "auto")) << ":";
      out.push_back(l.str());
    }

    long pos = ftell(f);
    uint8_t op = read_u8(f); ip++;
    std::ostringstream line;
    line << "[" << std::setw(8) << std::setfill('0') << std::hex << pos << std::dec << "] " << op_name(op);

    switch ((Op)op){
      case OP_PUSH_I: { long long v = read_s64(f); ip += 8; line << " " << v; } break;
      case OP_PUSH_F: { double    v = read_f64(f); ip += 8; line << " " << v; } break;
      case OP_PUSH_B: { uint8_t   v = read_u8 (f); ip += 1; line << " " << (v? "true":"false"); } break;
      case OP_PUSH_S: { std::string s = read_str(f); ip += 8 + s.size(); line << " " << qstr(s); } break;
      case OP_MAKE_LIST: { uint64_t n = read_u64(f); ip += 8; line << " " << n; } break;
      case OP_GET: { std::string id = read_str(f); ip += 8 + id.size(); line << " " << id; } break;
      case OP_SET: { std::string id = read_str(f); ip += 8 + id.size(); line << " " << id; } break;
      case OP_DECL:{
        std::string id = read_str(f); ip += 8 + id.size();
        uint8_t tt = read_u8(f); ip += 1;
        if (tt == 0xFF) line << " " << id << " : infer";
        else            line << " " << id << " : " << type_name((Type)tt);
      } break;
      case OP_POP:
      case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
      case OP_EQ: case OP_NE: case OP_LT: case OP_LE: case OP_GT: case OP_GE:
      case OP_AND: case OP_OR:
      case OP_MOUSE:
      case OP_RET:
      case OP_RET_VOID:
      case OP_HALT:
      case OP_NOP:
        break;
      case OP_JMP: { uint64_t tgt = read_u64(f); ip += 8; line << " " << tgt; } break;
      case OP_JF : { uint64_t tgt = read_u64(f); ip += 8; line << " " << tgt; } break;
      case OP_CALL_BUILTIN: {
        std::string name = read_str(f); ip += 8 + name.size();
        uint64_t argc = read_u64(f); ip += 8;
        line << " " << name << " argc=" << argc;
      } break;
      case OP_CALL_USER: {
        std::string name = read_str(f); ip += 8 + name.size();
        uint64_t argc = read_u64(f); ip += 8;
        line << " " << name << " argc=" << argc;
      } break;
    }
    out.push_back(line.str());
  }

  closeF();
  return out;
}

// ===== Editor core (with viewer) =====
struct Editor {
  std::vector<std::string> rows{1, std::string()};
  std::string filename;
  std::string message;
  bool dirty=false;

  int cx=0, cy=0;
  int pref_x=0;
  int rowoff=0, coloff=0;

  enum Mode { NORMAL, INSERT, COMMAND } mode=NORMAL;
  std::string cmdline;

  // Modal viewer (for :help / :decompile). Only ENTER exits.
  bool view_mode=false;
  std::string view_title;
  std::vector<std::string> view_lines;
  int view_off=0;

  struct Snap { std::vector<std::string> rows; int cx, cy; int rowoff, coloff; std::string filename; bool dirty; Mode mode; };
  std::vector<Snap> undo;
  std::string clipboard_line; bool clipboard_has_line=false;

  int rows_visible() const { return std::max(1, term.screen_rows - 2); }
  void set_message(const char* fmt, ...){
    char buf[512];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    message = buf;
  }
  void push_undo(){ if (undo.size() > 200) undo.erase(undo.begin()); undo.push_back(Snap{rows,cx,cy,rowoff,coloff,filename,dirty,mode}); }

  void open_file(const std::string& name){
    std::ifstream in(name);
    if (!in){ rows.assign(1, ""); filename=name; dirty=false; return; }
    rows.clear();
    std::string line;
    while (std::getline(in, line)){ if(!line.empty() && line.back()=='\r') line.pop_back(); rows.push_back(line); }
    if (rows.empty()) rows.push_back("");
    filename=name; dirty=false; cx=cy=0; pref_x=0; rowoff=coloff=0;
    set_message("Opened %s (%zu lines)", name.c_str(), rows.size());
  }
  bool save_file(const std::string& name){
    std::ofstream out(name);
    if (!out){ set_message("Could not write %s", name.c_str()); return false; }
    for (size_t i=0;i<rows.size();++i){ out<<rows[i]; if (i+1<rows.size()) out<<"\n"; }
    dirty=false; filename=name; set_message("Wrote %s", name.c_str()); return true;
  }

  int line_limit_for_mode(const std::string& line) const {
    if (mode==INSERT) return (int)line.size();
    int len=(int)line.size(); return len>0? len-1 : 0;
  }
  void clamp_cursor(){
    cy = std::clamp(cy, 0, (int)rows.size()-1);
    int lim = line_limit_for_mode(rows[cy]);
    cx = std::clamp(cx, 0, lim);
  }
  void scroll_into_view(){
    int vis = rows_visible();
    if (cy < rowoff) rowoff = cy;
    if (cy >= rowoff + vis) rowoff = cy - vis + 1;
    if (cx < coloff) coloff = cx;
    if (cx >= coloff + term.screen_cols) coloff = cx - term.screen_cols + 1;
  }

  // Movement
  void move_left(){ cx = std::max(0, cx-1); pref_x=cx; }
  void move_right(){ int lim = line_limit_for_mode(rows[cy]); if (cx < lim){ cx++; pref_x=cx; } }
  void move_home(){ cx = 0; pref_x=cx; }
  void move_end(){
    const auto& L=rows[cy];
    cx = (mode==INSERT)? (int)L.size() : std::max((int)L.size()-1,0);
    pref_x=cx;
  }
  void move_up(){ if (cy>0){ cy--; int lim=line_limit_for_mode(rows[cy]); cx=std::min(pref_x, lim);} clamp_cursor(); }
  void move_down(){ if (cy+1<(int)rows.size()){ cy++; int lim=line_limit_for_mode(rows[cy]); cx=std::min(pref_x, lim);} clamp_cursor(); }

  // Editing
  void insert_char(char c){
    push_undo();
    auto &line = rows[cy];
    int pos = std::clamp(cx, 0, (int)line.size());
    line.insert(line.begin()+pos, c);
    cx = pos+1; dirty=true; pref_x=cx;
  }
  void insert_newline(){
    push_undo();
    auto &line = rows[cy];
    int split = std::clamp(cx, 0, (int)line.size());
    std::string right = line.substr(split);
    line.erase(split);
    rows.insert(rows.begin()+cy+1, right);
    cy++; cx=0; dirty=true; pref_x=cx;
  }
  void backspace(){
    if (mode!=INSERT) return;
    push_undo();
    auto &line = rows[cy];
    if (cx>0){
      line.erase(line.begin()+cx-1); cx--; dirty=true; pref_x=cx; return;
    }
    if (cx==0 && cy>0){
      int prevlen=(int)rows[cy-1].size();
      rows[cy-1]+=line; rows.erase(rows.begin()+cy);
      cy--; cx=prevlen; dirty=true; pref_x=cx;
    }
  }
  void delete_char_under_cursor(){
    if (mode!=NORMAL) return;
    auto &line = rows[cy];
    if (cx>=0 && cx<(int)line.size()){
      push_undo();
      line.erase(line.begin()+cx); dirty=true;
      pref_x = std::min(pref_x, line_limit_for_mode(line));
      if (cx>(int)line.size()) cx=(int)line.size();
    }
  }
  void delete_line(){
    push_undo();
    if (rows.size()==1){
      clipboard_line=rows[0]; clipboard_has_line=true;
      rows[0].clear(); cx=0;
    } else {
      clipboard_line=rows[cy]; clipboard_has_line=true;
      rows.erase(rows.begin()+cy);
      if (cy>=(int)rows.size()) cy=(int)rows.size()-1;
      cx = std::min(cx, line_limit_for_mode(rows[cy]));
    }
    dirty=true; pref_x=cx;
  }
  void paste_line_below(){
    if (!clipboard_has_line) return;
    push_undo();
    rows.insert(rows.begin()+cy+1, clipboard_line);
    cy++; cx = std::min(cx, line_limit_for_mode(rows[cy])); dirty=true; pref_x=cx;
  }

  // Rendering (editor or viewer)
  void draw_rows(std::string &ab){
    int vis = rows_visible();
    if (view_mode){
      for (int y=0; y<vis; ++y){
        int idx = view_off + y;
        if (idx >= (int)view_lines.size()){ ab += "~\x1b[K\r\n"; }
        else {
          const std::string &line = view_lines[idx];
          if ((int)line.size() <= coloff) ab += "\x1b[K\r\n";
          else {
            int len = std::min((int)line.size() - coloff, term.screen_cols);
            ab.append(line.data() + coloff, len);
            ab += "\x1b[K\r\n";
          }
        }
      }
      return;
    }

    for (int y=0; y<vis; ++y){
      int filey = rowoff + y;
      if (filey >= (int)rows.size()){
        ab += "~\x1b[K\r\n";
      } else {
        const std::string &line = rows[filey];
        if ((int)line.size() <= coloff){
          ab += "\x1b[K\r\n";
        } else {
          int len = std::min((int)line.size() - coloff, term.screen_cols);
          ab.append(line.data() + coloff, len);
          ab += "\x1b[K\r\n";
        }
      }
    }
  }
  void draw_statusbar(std::string &ab){
    ab += "\x1b[7m";
    std::string left;
    if (view_mode){
      char buf[256];
      snprintf(buf, sizeof(buf), " VIEW  %s  lines:%zu  off:%d ",
               view_title.c_str(), view_lines.size(), view_off);
      left = buf;
    } else {
      const char* m = (mode==NORMAL?"NORMAL": (mode==INSERT?"INSERT":"COMMAND"));
      char buf[256];
      snprintf(buf, sizeof(buf), " %s  %s%s  lines:%zu  pos:%d,%d ",
               m,
               filename.empty()?"[No Name]":filename.c_str(), dirty?"*":"",
               rows.size(), cy+1, cx+1);
      left = buf;
    }
    if ((int)left.size()>term.screen_cols) left.resize(term.screen_cols);
    ab += left;
    if ((int)left.size()<term.screen_cols) ab.append(term.screen_cols - (int)left.size(), ' ');
    ab += "\x1b[m\x1b[K\r\n";
  }
  void draw_messagebar(std::string &ab){
    std::string line;
    if (view_mode){
      line = "[ENTER] to exit viewer  |  Up/Down/Page keys to scroll";
    } else {
      line = (mode==COMMAND?":"+cmdline:message);
    }
    if ((int)line.size()>term.screen_cols) line.resize(term.screen_cols);
    ab += line;
    if ((int)line.size()<term.screen_cols) ab.append(term.screen_cols - (int)line.size(), ' ');
    ab += "\x1b[K";
  }
  void refresh_screen(){
    if (!view_mode) scroll_into_view();
    std::string ab;
    ab.reserve(8192);
    ab += "\x1b[H";
    draw_rows(ab);
    draw_statusbar(ab);
    draw_messagebar(ab);
    if (!view_mode){
      int scry = (cy - rowoff) + 1;
      int scrx = (cx - coloff) + 1;
      if (scry < 1) scry = 1; if (scry > term.screen_rows) scry = term.screen_rows;
      if (scrx < 1) scrx = 1; if (scrx > term.screen_cols) scrx = term.screen_cols;
      char posbuf[64]; snprintf(posbuf, sizeof(posbuf), "\x1b[%d;%dH", scry, scrx);
      ab += posbuf;
      ab += "\x1b[?25h";
    } else {
      ab += "\x1b[?25l";
    }
    term.write(ab);
  }

  // Helpers
  static void split_path(const std::string& f, std::string& dir, std::string& name){
#ifndef _WIN32
    char *dup1 = ::strdup(f.c_str());
    dir = ::dirname(dup1);
    ::free(dup1);
    size_t slash=f.find_last_of('/');
    std::string base=(slash==std::string::npos? f : f.substr(slash+1));
    size_t dot=base.find_last_of('.'); name=(dot==std::string::npos? base : base.substr(0,dot));
#else
    char drive[_MAX_DRIVE], d[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
    _splitpath(f.c_str(), drive, d, fname, ext);
    dir = std::string(drive)+d; name=fname;
#endif
  }

  // Commands
  void do_run_saved(){
    if (filename.empty() || dirty){ set_message("Write buffer first (:w {name})"); return; }
    std::string dir, name; split_path(filename, dir, name);

    // Clear before handing control to the program (enter + run)
    clear_now();

    // Optional env override
    if (const char* tmplt = std::getenv("VIMISH_MINIS_RUN"); tmplt && *tmplt){
      std::string tmp = tmp_ms_path();
      std::string cmd = tmplt;
      cmd = replace_all(cmd, "{{file}}", filename);
      cmd = replace_all(cmd, "{{dir}}", dir);
      cmd = replace_all(cmd, "{{name}}", name);
      cmd = replace_all(cmd, "{{tmp}}", tmp);

      term.disable_raw();
#ifdef _WIN32
      system(cmd.c_str());
#else
      std::string wrapped = "bash -lc '" + cmd + "'";
      system(wrapped.c_str());
#endif
      // On return to editor, prompt to continue
      std::fputs("\x1b[2J\x1b[H", stdout); // clear screen on return boundary too
      std::fputs("\n[press ENTER to return]", stdout);
      std::fflush(stdout);
      while (true){ int c=getchar(); if (c=='\n' || c=='\r' || c==EOF) break; }
      term.enable_raw();
#ifdef _WIN32
      DeleteFileA(tmp.c_str());
#else
      ::unlink(tmp.c_str());
#endif
      return;
    }

    // Embedded compile+run via minis.hpp
    std::string tmp = tmp_ms_path();
    term.disable_raw();
    std::string err;
    bool okc = minis_compile_to(filename, tmp, err);
    if (!okc){
      std::fputs("\n[compile error] ", stdout); std::fputs(err.c_str(), stdout); std::fputs("\n", stdout);
      std::fputs("[press ENTER to return]", stdout); std::fflush(stdout);
      while (true){ int c=getchar(); if (c=='\n' || c=='\r' || c==EOF) break; }
      term.enable_raw();
#ifdef _WIN32
      DeleteFileA(tmp.c_str());
#else
      ::unlink(tmp.c_str());
#endif
      return;
    }
    std::string err2;
    bool okr = minis_run_bc(tmp, err2);
    if (!okr){
      std::fputs("\n[run error] ", stdout); std::fputs(err2.c_str(), stdout); std::fputs("\n", stdout);
    }
    std::fputs("\n[press ENTER to return]", stdout); std::fflush(stdout);
    while (true){ int c=getchar(); if (c=='\n' || c=='\r' || c==EOF) break; }
    term.enable_raw();
#ifdef _WIN32
    DeleteFileA(tmp.c_str());
#else
    ::unlink(tmp.c_str());
#endif
  }

  void do_compile(const std::string& out_arg){
    if (filename.empty() || dirty){ set_message("Write buffer first (:w {name})"); return; }
    std::string dir, name; split_path(filename, dir, name);
#ifdef _WIN32
    std::string sep = (dir.empty() || dir.back()=='\\' || dir.back()=='/') ? "" : "\\";
#else
    std::string sep = (dir.empty() || dir.back()=='/') ? "" : "/";
#endif
    std::string out = out_arg.empty()? (dir + sep + name + ".ms") : out_arg;

    if (const char* tmplt = std::getenv("VIMISH_MINIS_COMPILE"); tmplt && *tmplt){
      term.disable_raw();
      std::string cmd = tmplt;
      cmd = replace_all(cmd, "{{file}}", filename);
      cmd = replace_all(cmd, "{{dir}}", dir);
      cmd = replace_all(cmd, "{{name}}", name);
      cmd = replace_all(cmd, "{{out}}", out);
#ifdef _WIN32
      system(cmd.c_str());
#else
      std::string wrapped = "bash -lc '" + cmd + "'";
      system(wrapped.c_str());
#endif
      std::fputs("\n[press ENTER to return]", stdout); std::fflush(stdout);
      while (true){ int c=getchar(); if (c=='\n' || c=='\r' || c==EOF) break; }
      term.enable_raw();
      return;
    }

    term.disable_raw();
    std::string err;
    bool ok = minis_compile_to(filename, out, err);
    if (!ok){
      std::fputs("\n[compile error] ", stdout); std::fputs(err.c_str(), stdout); std::fputs("\n", stdout);
      std::fputs("[press ENTER to return]", stdout); std::fflush(stdout);
      while (true){ int c=getchar(); if (c=='\n' || c=='\r' || c==EOF) break; }
      term.enable_raw();
      return;
    }
    std::fprintf(stdout, "\n[Wrote %s]\n[press ENTER to return]", out.c_str());
    std::fflush(stdout);
    while (true){ int c=getchar(); if (c=='\n' || c=='\r' || c==EOF) break; }
    term.enable_raw();
  }

  void open_settings_conf(){
    std::ifstream in("vimish.conf");
    if (!in){
      std::ofstream t("vimish.conf");
      t << "# vimish.conf - your settings playground\n"
        << "# Example future toggles:\n"
        << "# color.enable = true\n"
        << "# color.scheme = monokai\n"
        << "# indent.tabs = false\n"
        << "# indent.width = 2\n";
      t.close();
    }
    open_file("vimish.conf");
  }

  void show_help(){
    std::vector<std::string> lines;
    std::ifstream in("guide.txt");
    if (!in){
      std::ofstream t("guide.txt");
      t <<
        "vimish (minis) quick help\n"
        "-------------------------\n"
        "NORMAL: h j k l, 0, $, x, dd, i, a, A, o, O, :, u, p\n"
        "INSERT: type, Backspace, Enter, Esc\n"
        "Commands:\n"
        "  :w, :w <file>, :q, :q!, :wq/:x\n"
        "  :run           (compile temp .ms + run)\n"
        "  :compile [out] (compile to <name>.ms)\n"
        "  :decompile [bc.ms] (view bytecode as text)\n"
        "  :open <file>   (open file)\n"
        "  :settings      (open vimish.conf)\n"
        "\n"
        "Press ENTER to exit this help.\n";
      t.close();
      in.open("guide.txt");
    }
    std::string line;
    while (std::getline(in, line)){ if(!line.empty() && line.back()=='\r') line.pop_back(); lines.push_back(line); }
    enter_view("HELP: guide.txt", std::move(lines));
  }

  void enter_view(std::string title, std::vector<std::string> lines){
    view_mode=true; view_title=std::move(title); view_lines=std::move(lines); view_off=0;
  }

  void do_decompile(const std::string& argFile){
    std::string bc = argFile;
    if (bc.empty()){
      if (filename.empty() || dirty){ set_message("Specify bytecode or :w first"); return; }
      std::string dir, name; split_path(filename, dir, name);
#ifdef _WIN32
      std::string sep = (dir.empty() || dir.back()=='\\' || dir.back()=='/') ? "" : "\\";
#else
      std::string sep = (dir.empty() || dir.back()=='/') ? "" : "/";
#endif
      bc = dir + sep + name + ".ms";
    }
    std::string err;
    auto lines = decompile_to_lines(bc, err);
    if (lines.empty() && !err.empty()){
      set_message("decompile error: %s", err.c_str());
      return;
    }
    enter_view(std::string("DECOMPILE: ") + bc, std::move(lines));
  }

  // Command dispatcher
  void run_command(){
    std::string cmd = cmdline; cmdline.clear();
    auto trim = [](std::string s){
      size_t a=s.find_first_not_of(" \t");
      size_t b=s.find_last_not_of(" \t");
      if (a==std::string::npos) return std::string();
      return s.substr(a, b-a+1);
    };
    cmd = trim(cmd);

    if (cmd=="q"){
      if (dirty){ set_message("No write since last change (:q! to quit)"); return; }
      throw std::runtime_error("quit");
    } else if (cmd=="q!"){
      throw std::runtime_error("quit");
    } else if (cmd=="w"){
      if (filename.empty()){ set_message("No filename (:w {name})"); return; }
      save_file(filename);
    } else if (cmd.rfind("w ",0)==0){
      auto arg = trim(cmd.substr(2));
      if (arg.empty()){ set_message("Usage: :w {filename}"); return; }
      save_file(arg);
    } else if (cmd=="wq" || cmd=="x"){
      if (filename.empty()){ set_message("No filename (:w {name})"); return; }
      if (save_file(filename)) throw std::runtime_error("quit");
    } else if (cmd=="run"){
      do_run_saved();
    } else if (cmd=="compile"){
      do_compile("");
    } else if (cmd.rfind("compile ",0)==0){
      auto arg = trim(cmd.substr(8));
      do_compile(arg);
    } else if (cmd=="settings"){
      open_settings_conf();
    } else if (cmd=="decompile"){
      do_decompile("");
    } else if (cmd.rfind("decompile ",0)==0){
      auto arg = trim(cmd.substr(10));
      do_decompile(arg);
    } else if (cmd=="help"){
      show_help();
    } else if (cmd.rfind("open!",0)==0){
      auto arg = trim(cmd.substr(5));
      if (arg.empty()){ set_message("Usage: :open! {filename}"); return; }
      open_file(arg);
    } else if (cmd.rfind("open",0)==0){
      auto arg = trim(cmd.substr(4));
      if (arg.empty()){ set_message("Usage: :open {filename}"); return; }
      if (dirty){ set_message("No write since last change (:w or :open! to discard)"); return; }
      open_file(arg);
    } else if (cmd.empty()){
      // no-op
    } else {
      set_message("Unknown command: :%s", cmd.c_str());
    }
  }

  // Main loop
  void loop(){
    bool awaiting_dd=false;
    while (true){
      refresh_screen();
      int k = term.read_key();

      if (view_mode){
        if (k=='\r' || k=='\n'){ view_mode=false; continue; } // ENTER exits viewer only
        else if (k==KEY_ARROW_UP){ if (view_off>0) --view_off; }
        else if (k==KEY_ARROW_DOWN){ if (view_off+rows_visible() < (int)view_lines.size()) ++view_off; }
        else if (k==KEY_PAGE_UP){ view_off = std::max(0, view_off - rows_visible()); }
        else if (k==KEY_PAGE_DOWN){ view_off = std::min(std::max(0,(int)view_lines.size()-rows_visible()), view_off + rows_visible()); }
        else if (k==KEY_HOME){ view_off = 0; }
        else if (k==KEY_END){ view_off = std::max(0,(int)view_lines.size()-rows_visible()); }
        continue;
      }

      if (mode == NORMAL){
        if (k=='h' || k==KEY_ARROW_LEFT) move_left();
        else if (k=='l' || k==KEY_ARROW_RIGHT) move_right();
        else if (k=='k' || k==KEY_ARROW_UP) move_up();
        else if (k=='j' || k==KEY_ARROW_DOWN) move_down();
        else if (k=='0' || k==KEY_HOME) move_home();
        else if (k=='$' || k==KEY_END) move_end();
        else if (k=='x' || k==KEY_DEL) { delete_char_under_cursor(); awaiting_dd=false; }
        else if (k=='i') { mode = INSERT; set_message("-- INSERT --"); awaiting_dd=false; }
        else if (k=='a') { int len=(int)rows[cy].size(); cx = std::min(cx+1, (mode==INSERT?len:std::max(len-1,0))); mode=INSERT; set_message("-- INSERT --"); awaiting_dd=false; }
        else if (k=='A') { move_end(); mode=INSERT; set_message("-- INSERT --"); awaiting_dd=false; }
        else if (k=='o') { push_undo(); rows.insert(rows.begin()+cy+1, ""); cy++; cx=0; mode=INSERT; dirty=true; set_message("-- INSERT --"); awaiting_dd=false; }
        else if (k=='O') { push_undo(); rows.insert(rows.begin()+cy, ""); cx=0; mode=INSERT; dirty=true; set_message("-- INSERT --"); awaiting_dd=false; }
        else if (k==':') { mode = COMMAND; cmdline.clear(); awaiting_dd=false; }
        else if (k=='d') { if (awaiting_dd){ delete_line(); awaiting_dd=false; } else { awaiting_dd=true; set_message("d"); } }
        else if (k=='p') { paste_line_below(); awaiting_dd=false; }
        else if (k=='u') { if(!undo.empty()){ auto s=undo.back(); undo.pop_back(); rows=s.rows; cx=s.cx; cy=s.cy; rowoff=s.rowoff; coloff=s.coloff; filename=s.filename; dirty=s.dirty; mode=s.mode; } awaiting_dd=false; }
        else if (k==KEY_ESC) { awaiting_dd=false; }
        else { awaiting_dd=false; }
      }
      else if (mode == INSERT){
        if (k==KEY_ESC){ mode = NORMAL; set_message(""); clamp_cursor(); }
        else if (k=='\r' || k=='\n') { insert_newline(); }
        else if (k==KEY_BACKSPACE || k==8) { backspace(); }
        else if (k==KEY_ARROW_LEFT) { if (cx>0) { cx--; pref_x=cx; } }
        else if (k==KEY_ARROW_RIGHT){ int len=(int)rows[cy].size(); if (cx<len){ cx++; pref_x=cx; } }
        else if (k==KEY_ARROW_UP)   { if (cy>0){ cy--; int lim=line_limit_for_mode(rows[cy]); cx=std::min(pref_x, lim); } }
        else if (k==KEY_ARROW_DOWN) { if (cy+1<(int)rows.size()){ cy++; int lim=line_limit_for_mode(rows[cy]); cx=std::min(pref_x, lim); } }
        else if (is_printable((unsigned char)k) || k=='\t') { insert_char((char)k); }
      }
      else if (mode == COMMAND){
        if (k==KEY_ESC){ mode = NORMAL; set_message(""); }
        else if (k==KEY_BACKSPACE || k==8){ if (!cmdline.empty()) cmdline.pop_back(); }
        else if (k=='\r' || k=='\n'){ try { run_command(); } catch (const std::runtime_error& e){ if (std::string(e.what())=="quit") return; else throw; } mode = NORMAL; }
        else if (is_printable((unsigned char)k) || std::isspace(k)) { cmdline.push_back((char)k); }
      }
    }
  }
};

static void sigwinch_handler(int){ term.update_winsize(); }

int main(int argc, char** argv){
  std::setlocale(LC_ALL, "");
#ifndef _WIN32
  std::signal(SIGWINCH, sigwinch_handler);
#endif
  term.enable_raw(); // also clears screen at startup
  atexit([](){ term.disable_raw(); });
  Editor ed;
  if (argc > 1) ed.open_file(argv[1]); else ed.set_message("NEW buffer - :w {name} to save");

  try {
    ed.loop();
  } catch(...) {
    // On unexpected error, clear then restore terminal
    clear_now();
    term.disable_raw();
    throw;
  }

  // On clean exit (via :q or :wq/:x), clear the screen before restoring terminal
  clear_now();
  term.disable_raw();
  return 0;
}
