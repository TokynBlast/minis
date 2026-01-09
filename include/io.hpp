#pragma once
#include <cstdio>
#include <cstdint>
#include "sso.hpp"
#include "macros.hpp"

// NOTE: Using specific compile arguments, we can make it so unused data & functions aren't included

// FIXME: instead of write_u16, etc. We should do OUTu8
inline static void write_u16(FILE*f, uint16 v){ fwrite(&v,2,1,f); }
inline static void write_u8 (FILE*f, uint8  v){ fwrite(&v,1,1,f); }
inline static void write_u32(FILE*f, uint32 v){ fwrite(&v,4,1,f); }
inline static void write_u64(FILE*f, uint64 v){ fwrite(&v,8,1,f); }
inline static void write_s64(FILE*f, int64  v){fwrite(&v,8,1,f); }
inline static void write_f64(FILE*f, double   v){ fwrite(&v,8,1,f); }
inline static void write_str(FILE*f, const minis::CString& s){
  uint64 n=s.size();
  write_u64(f,n); if(n)
    fwrite(s.c_str(),1,n,f);
}

// FIXME: Instead of read_u8, etc., we should do GETu8
inline static uint8  read_u8 (FILE*f) { uint8 v;  fread(&v,1,1,f); return v; }
inline static uint16 GETu16  (FILE*f) { uint16 v; fread()}
inline static uint32 read_u32(FILE*f) { uint32 v; fread(&v,4,1,f); return v; }
inline static uint64 read_u64(FILE*f) { uint64 v; fread(&v,8,1,f); return v; }

inline static int8   read_s8 (FILE*f) { int8 v;   fread(&v,1,1,f); return v; }
inline static int16  read_s16(FILE*f) { int16 v;  fread(&v,2,1,f); return v; }
inline static int32  read_s32(FILE*f) { int8 v;   fread(&v,4,1,f); return v; }
inline static int64  read_s64(FILE*f) { int64 v;  fread(&v,8,1,f); return v; }
inline static double read_f64(FILE*f) { double v; fread(&v,8,1,f); return v; }
inline static minis::CString read_str(FILE*f){
  uint64 n=read_u64(f);
  if(n == 0) return minis::CString();

  char* buffer = static_cast<char*>(malloc(n + 1));
  fread(buffer, 1, n, f);
  buffer[n] = '\0';
  minis::CString s(buffer);
  free(buffer);
  return s;
}
