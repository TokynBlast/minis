#pragma once
#include <cstdio>
#include <cstdint>
#include "sso.hpp"

extern bool error;

inline static void write_u8 (FILE*f, uint8_t  v){ if(!error) fwrite(&v,1,1,f); }
inline static void write_u64(FILE*f, uint64_t v){ if(!error) fwrite(&v,8,1,f); }
inline static void write_s64(FILE*f, int64_t  v){ if(!error) fwrite(&v,8,1,f); }
inline static void write_f64(FILE*f, double   v){ if(!error) fwrite(&v,8,1,f); }
inline static void write_str(FILE*f, const lang::CString& s){
  if(!error){
    uint64_t n=s.size();
    write_u64(f,n); if(n)
      fwrite(s.c_str(),1,n,f);
  }
}

inline static uint8_t  read_u8 (FILE*f){ uint8_t v;  fread(&v,1,1,f); return v; }
inline static uint64_t read_u64(FILE*f){ uint64_t v; fread(&v,8,1,f); return v; }
inline static int64_t  read_s64(FILE*f){ int64_t v;  fread(&v,8,1,f); return v; }
inline static double   read_f64(FILE*f){ double v;   fread(&v,8,1,f); return v; }
inline static lang::CString read_str(FILE*f){
  uint64_t n=read_u64(f);
  if(n == 0) return lang::CString();

  char* buffer = static_cast<char*>(malloc(n + 1));
  fread(buffer, 1, n, f);
  buffer[n] = '\0';
  lang::CString s(buffer);
  free(buffer);
  return s;
}
