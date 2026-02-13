
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Minis I/O Runtime - C99 compatible */

typedef enum {
  MINIS_FILE = 0,
  MINIS_MEM = 1
} MinisHandleKind;

typedef struct {
  MinisHandleKind kind;
  FILE* file;
  char* buffer;
  size_t buf_size;
  size_t buf_cap;
  size_t pos;
  int readable;
  int writable;
  int closed;
} MinisHandle;

/* Global handle table */
static MinisHandle* g_handles = NULL;
static size_t g_handle_count = 0;
static size_t g_handle_cap = 0;
static uint64_t g_open_count = 0;

static void minis_error(const char* msg) {
  fputs(msg, stderr);
  fputs("\n", stderr);
  exit(1);
}

static void ensure_handle_space(void) {
  if (g_handle_count >= g_handle_cap) {
    size_t new_cap = (g_handle_cap == 0) ? 16 : g_handle_cap * 2;
    MinisHandle* new_handles = realloc(g_handles, new_cap * sizeof(MinisHandle));
    if (!new_handles) minis_error("FATAL ERROR: out of memory");
    g_handles = new_handles;
    g_handle_cap = new_cap;
  }
}

static uint64_t register_handle(MinisHandle h) {
  ensure_handle_space();
  g_handles[g_handle_count] = h;
  g_open_count++;
  return (uint64_t)(g_handle_count++ + 1);
}

static MinisHandle* get_handle(uint64_t id) {
  if (id == 0 || id > g_handle_count) return NULL;
  MinisHandle* h = &g_handles[id - 1];
  if (h->closed) return NULL;
  return h;
}

static char* alloc_and_copy(const char* src, size_t len) {
  char* out = malloc(len + 1);
  if (!out) minis_error("FATAL ERROR: out of memory");
  if (len > 0) memcpy(out, src, len);
  out[len] = '\0';
  return out;
}

uint64_t minis_open(const char* path, const char* mode) {
  FILE* f;
  MinisHandle h;
  if (!path || !mode) minis_error("FATAL ERROR: open expects path and mode");
  f = fopen(path, mode);
  if (!f) minis_error("FATAL ERROR: failed to open file");

  h.kind = MINIS_FILE;
  h.file = f;
  h.buffer = NULL;
  h.buf_size = 0;
  h.buf_cap = 0;
  h.pos = 0;
  h.readable = (strchr(mode, 'r') != NULL) ? 1 : 0;
  h.writable = (strchr(mode, 'w') != NULL || strchr(mode, 'a') != NULL) ? 1 : 0;
  h.closed = 0;

  return register_handle(h);
}

uint64_t minis_mem_open(const char* data, const char* mode) {
  MinisHandle h;
  size_t len;
  if (!mode) minis_error("FATAL ERROR: mem_open expects mode");

  len = data ? strlen(data) : 0;
  h.kind = MINIS_MEM;
  h.file = NULL;
  h.buf_size = len;
  h.buffer = alloc_and_copy(data ? data : "", len);
  h.buf_cap = len + 1;
  h.readable = (strchr(mode, 'r') != NULL) ? 1 : 0;
  h.writable = (strchr(mode, 'w') != NULL || strchr(mode, 'a') != NULL) ? 1 : 0;
  h.pos = (strchr(mode, 'a') != NULL) ? len : 0;
  if (h.writable && !h.readable) {
    h.buf_size = 0;
    h.pos = 0;
  }
  h.closed = 0;

  return register_handle(h);
}

const char* minis_mem_get(uint64_t id) {
  MinisHandle* h = get_handle(id);
  if (!h || h->kind != MINIS_MEM) {
    minis_error("FATAL ERROR: mem_get expects memory handle");
  }
  return alloc_and_copy(h->buffer, h->buf_size);
}

const char* minis_read_file(const char* path) {
  FILE* f;
  char buf[4096];
  char* out = NULL;
  size_t out_len = 0, out_cap = 0;
  size_t n;

  if (!path) minis_error("FATAL ERROR: read expects path");
  f = fopen(path, "rb");
  if (!f) minis_error("FATAL ERROR: failed to open file");

  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
    if (out_len + n > out_cap) {
      out_cap = (out_cap == 0) ? 4096 : out_cap * 2;
      if (out_len + n > out_cap) out_cap = out_len + n;
      out = realloc(out, out_cap);
      if (!out) minis_error("FATAL ERROR: out of memory");
    }
    memcpy(out + out_len, buf, n);
    out_len += n;
  }
  fclose(f);

  if (!out) out = malloc(1);
  else out = realloc(out, out_len + 1);
  if (!out) minis_error("FATAL ERROR: out of memory");
  out[out_len] = '\0';
  return out;
}

const char* minis_read_handle(uint64_t id, uint64_t n) {
  MinisHandle* h = get_handle(id);
  char* out = NULL;
  size_t take = 0;

  if (!h) minis_error("FATAL ERROR: invalid or closed handle");
  if (!h->readable) minis_error("FATAL ERROR: handle not readable");

  if (h->kind == MINIS_FILE) {
    char buf[4096];
    size_t read_n;
    size_t out_len = 0, out_cap = 0;

    if (n == 0) {
      while ((read_n = fread(buf, 1, sizeof(buf), h->file)) > 0) {
        if (out_len + read_n > out_cap) {
          out_cap = (out_cap == 0) ? 4096 : out_cap * 2;
          if (out_len + read_n > out_cap) out_cap = out_len + read_n;
          out = realloc(out, out_cap);
          if (!out) minis_error("FATAL ERROR: out of memory");
        }
        memcpy(out + out_len, buf, read_n);
        out_len += read_n;
      }
    } else {
      out = malloc((size_t)n);
      if (!out) minis_error("FATAL ERROR: out of memory");
      out_len = fread(out, 1, (size_t)n, h->file);
    }

    if (!out) out = malloc(1);
    else out = realloc(out, out_len + 1);
    if (!out) minis_error("FATAL ERROR: out of memory");
    out[out_len] = '\0';
    return out;
  }

  if (h->pos > h->buf_size) h->pos = h->buf_size;
  take = (n == 0) ? (h->buf_size - h->pos) : ((uint64_t)(h->buf_size - h->pos) < n ? h->buf_size - h->pos : n);
  out = alloc_and_copy(h->buffer + h->pos, take);
  h->pos += take;
  return out;
}

uint64_t minis_write(uint64_t id, const char* data) {
  MinisHandle* h = get_handle(id);
  size_t len;

  if (!h) minis_error("FATAL ERROR: invalid or closed handle");
  if (!h->writable) minis_error("FATAL ERROR: handle not writable");
  if (!data) return 0;

  len = strlen(data);

  if (h->kind == MINIS_FILE) {
    return (uint64_t)fwrite(data, 1, len, h->file);
  }

  if (h->pos > h->buf_size) {
    if (h->pos > h->buf_cap) {
      h->buf_cap = h->pos + 1;
      h->buffer = realloc(h->buffer, h->buf_cap);
      if (!h->buffer) minis_error("FATAL ERROR: out of memory");
    }
    memset(h->buffer + h->buf_size, '\0', h->pos - h->buf_size);
    h->buf_size = h->pos;
  }

  if (h->pos == h->buf_size) {
    if (h->buf_size + len > h->buf_cap) {
      h->buf_cap = ((h->buf_size + len) * 3) / 2;
      h->buffer = realloc(h->buffer, h->buf_cap);
      if (!h->buffer) minis_error("FATAL ERROR: out of memory");
    }
    memcpy(h->buffer + h->pos, data, len);
    h->buf_size = h->pos + len;
  } else {
    size_t available = h->buf_size - h->pos;
    size_t to_copy = (available < len) ? available : len;
    memcpy(h->buffer + h->pos, data, to_copy);
    h->pos += to_copy;
    if (to_copy < len) {
      if (h->buf_size + (len - to_copy) > h->buf_cap) {
        h->buf_cap = ((h->buf_size + len) * 3) / 2;
        h->buffer = realloc(h->buffer, h->buf_cap);
        if (!h->buffer) minis_error("FATAL ERROR: out of memory");
      }
      memcpy(h->buffer + h->buf_size, data + to_copy, len - to_copy);
      h->buf_size = h->buf_size + (len - to_copy);
    }
  }
  h->pos = h->buf_size;
  return (uint64_t)len;
}

uint64_t minis_close(uint64_t id) {
  MinisHandle* h = get_handle(id);
  if (!h) return 0;
  if (h->kind == MINIS_FILE && h->file) {
    fclose(h->file);
    h->file = NULL;
  }
  if (h->kind == MINIS_MEM && h->buffer) {
    free(h->buffer);
    h->buffer = NULL;
  }
  h->closed = 1;
  if (g_open_count > 0) g_open_count--;
  return 1;
}

void minis_check_leaks(void) {
  if (g_open_count > 0) {
    fprintf(stderr, "FATAL ERROR: %llu file handle(s) were never closed\n",
      (unsigned long long)g_open_count);
    exit(1);
  }
}

void print(const char* text) {
  if (!text) return;
  fputs(text, stdout);
}
