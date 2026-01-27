#define VEC(T) struct { T *data; size_t len, cap; }

#define push(v, x) do { \
  if ((v)->len == (v)->cap) { \
    (v)->cap = (v)->cap ? (v)->cap + 2 : 8; \
    (v)->data = realloc((v->data), (v)->cap * sizeof(*(v)->data)); \
  } \
   (v)->data[(v)->len++] = (x); \
} while (0)
