inline static void write_u8 (FILE*f, uint8_t  v){ fwrite(&v,1,1,f); }
inline static void write_u64(FILE*f, uint64_t v){ fwrite(&v,8,1,f); }
inline static void write_s64(FILE*f, int64_t  v){ fwrite(&v,8,1,f); }
inline static void write_f64(FILE*f, double   v){ fwrite(&v,8,1,f); }
inline static void write_str(FILE*f, const std::string&s){ uint64_t n=s.size(); write_u64(f,n); if(n) fwrite(s.data(),1,n,f); }

inline static uint8_t  read_u8 (FILE*f){ uint8_t v;  fread(&v,1,1,f); return v; }
inline static uint64_t read_u64(FILE*f){ uint64_t v; fread(&v,8,1,f); return v; }
inline static std::string read_str(FILE*f){ uint64_t n=read_u64(f); std::string s; s.resize(n); if(n) fread(&s[0],1,n,f); return s; }
