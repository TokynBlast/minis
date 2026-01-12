#pragma once
#include <cstdio>
#include <cstdint>
#include "macros.hpp"

// NOTE: Using specific compile arguments, we can make it so unused data & functions aren't included

// FIXME: instead of write_u16, etc. We should do OUTu8
inline static void OUTu8 (FILE*f, uint8  v){ fwrite(&v,1,1,f); }
inline static void OUTu16(FILE*f, uint16 v){ fwrite(&v,2,1,f); }
inline static void OUTu32(FILE*f, uint32 v){ fwrite(&v,4,1,f); }
inline static void OUTu64(FILE*f, uint64 v){ fwrite(&v,8,1,f); }

inline static void OUTs8 (FILE*f, int8   v){ fwrite(&v,1,1,f); }
inline static void OUTs16(FILE*f, int16  v){ fwrite(&v,2,1,f); }
inline static void OUTs32(FILE*f, int32  v){ fwrite(&v,4,1,f); }
inline static void OUTs64(FILE*f, int64  v){ fwrite(&v,8,1,f); }

inline static void OUTf64(FILE*f, double v){ fwrite(&v,8,1,f); }
inline static void OUTstr(FILE*f, const std::string& s){
  uint64 n=s.size();
  OUTu64(f,n); if(n)
    fwrite(s.c_str(),1,n,f);
}

// FIXME: Instead of read_u8, etc., we should do GETu8
inline static uint8  GETu8 (FILE*f) { uint8 v;  fread(&v,1,1,f); return v; }
inline static uint16 GETu16(FILE*f) { uint16 v; fread(&v,2,1,f); return v; }
inline static uint32 GETu32(FILE*f) { uint32 v; fread(&v,4,1,f); return v; }
inline static uint64 GETu64(FILE*f) { uint64 v; fread(&v,8,1,f); return v; }

inline static int8   GETs8 (FILE*f) { int8 v;   fread(&v,1,1,f); return v; }
inline static int16  GETs16(FILE*f) { int16 v;  fread(&v,2,1,f); return v; }
inline static int32  GETs32(FILE*f) { int8 v;   fread(&v,4,1,f); return v; }
inline static int64  GETs64(FILE*f) { int64 v;  fread(&v,8,1,f); return v; }

inline static double GETf64(FILE*f) { double v; fread(&v,8,1,f); return v; }
inline static std::string GETstr(FILE*f){
  uint64 n=GETu64(f);
  if(n == 0) return std::string();

  std::string s(n, '\0');
  fread(&s[0], 1, n, f);
  return s;
}
