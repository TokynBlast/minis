#pragma once
#include <cstring>
#include <cstddef>
#include <cstdlib>
#include <utility>
#include <cstdint>

namespace minis {
  class CString {
    static constexpr std::size_t SMALL_SIZE = 15;

    union Storage {
      char small[SMALL_SIZE + 1];
      char* large;
      Storage() { small[0] = '\0'; }
    } storage;

    std::size_t len = 0;
    bool is_small = true;

    CString* reg_next = nullptr;
    static inline CString* registry_head = nullptr;

    void register_self() noexcept {
      reg_next = registry_head;
      registry_head = this;
    }
    void unregister_self() noexcept {
        CString** cur = &registry_head;
        while (*cur) {
          if (*cur == this) {
            *cur = (*cur)->reg_next;
            reg_next = nullptr;
            return;
          }
          cur = &((*cur)->reg_next);
        }
      }

    static char* alloc_len(std::size_t n) noexcept {
      char* p = static_cast<char*>(std::malloc(n + 1));
      if (p) p[n] = '\0';
      return p;
    }

    public:
      CString() noexcept {
        storage.small[0] = '\0';
        len = 0;
        is_small = true;
        register_self();
      }

      CString(const char* s) noexcept {
        if (!s) s = "";
        len = std::strlen(s);
        is_small = (len <= SMALL_SIZE);
        if (is_small) {
          std::memcpy(storage.small, s, len + 1);
        } else {
          storage.large = alloc_len(len);
          if (storage.large)
              std::memcpy(storage.large, s, len + 1);
          else {
              is_small = true;
              storage.small[0] = '\0';
              len = 0;
          }
        }
        register_self();
      }

      CString(const char* s, std::size_t length) noexcept {
        if (!s) { s = ""; length = 0; }
        len = length;
        is_small = (len <= SMALL_SIZE);
        if (is_small) {
          std::memcpy(storage.small, s, len);
          storage.small[len] = '\0';
        } else {
          storage.large = alloc_len(len);
          if (storage.large) {
            std::memcpy(storage.large, s, len);
            storage.large[len] = '\0';
          } else {
            is_small = true;
            storage.small[0] = '\0';
            len = 0;
          }
        }
        register_self();
      }

      CString(const CString& other) noexcept {
        len = other.len;
        is_small = (len <= SMALL_SIZE);
        if (is_small) {
          std::memcpy(storage.small, other.c_str(), len + 1);
        } else {
          storage.large = alloc_len(len);
          if (storage.large)
            std::memcpy(storage.large, other.c_str(), len + 1);
          else {
            is_small = true;
            storage.small[0] = '\0';
            len = 0;
          }
        }
        register_self();
      }

      CString(CString&& other) noexcept {
        len = other.len;
        is_small = other.is_small;
        if (is_small) {
          std::memcpy(storage.small, other.storage.small, len + 1);
        } else {
          storage.large = other.storage.large;
          other.storage.large = nullptr;
          other.is_small = true;
          other.storage.small[0] = '\0';
          other.len = 0;
        }
        register_self();
      }

      CString& operator=(const CString& other) noexcept {
        if (this == &other) return *this;
        if (!is_small && storage.large) {
          std::free(storage.large);
          storage.large = nullptr;
        }
        len = other.len;
        is_small = (len <= SMALL_SIZE);
        if (is_small) {
          std::memcpy(storage.small, other.c_str(), len + 1);
        } else {
          storage.large = alloc_len(len);
          if (storage.large) std::memcpy(storage.large, other.c_str(), len + 1);
          else {
            is_small = true;
            storage.small[0] = '\0';
            len = 0;
          }
        }
        return *this;
      }

      CString& operator=(CString&& other) noexcept {
        if (this == &other) return *this;
        if (!is_small && storage.large) {
          std::free(storage.large);
          storage.large = nullptr;
        }
        len = other.len;
        is_small = other.is_small;
        if (is_small) {
          std::memcpy(storage.small, other.storage.small, len + 1);
        } else {
          storage.large = other.storage.large;
          other.storage.large = nullptr;
          other.is_small = true;
          other.storage.small[0] = '\0';
          other.len = 0;
        }
        return *this;
      }

      ~CString() noexcept
      {
        if (!is_small && storage.large)
        {
          std::free(storage.large);
          storage.large = nullptr;
        }
        unregister_self();
      }

      void destroy() noexcept {
        if (!is_small && storage.large) {
          std::free(storage.large);
          storage.large = nullptr;
        }
        is_small = true;
        len = 0;
        storage.small[0] = '\0';
        unregister_self();
      }

      const char* c_str() const noexcept { return is_small ? storage.small : storage.large; }
      char* data() noexcept { return is_small ? storage.small : storage.large; }
      std::size_t size() const noexcept { return len; }
      bool empty() const noexcept { return len == 0; }
      bool using_heap() const noexcept { return !is_small && storage.large; }

      char operator[](std::size_t idx) const noexcept {
        return (idx < len) ? (is_small ? storage.small[idx] : storage.large[idx]) : '\0';
      }

      void assign(const char* s) noexcept {
        if (!s) s = "";
        std::size_t new_len = std::strlen(s);
        if (new_len <= SMALL_SIZE) {
          if (!is_small && storage.large) {
            std::free(storage.large);
            storage.large = nullptr;
          }
          is_small = true;
          std::memcpy(storage.small, s, new_len + 1);
          } else {
            if (is_small || !storage.large) {
              storage.large = alloc_len(new_len);
            } else {
              std::free(storage.large);
              storage.large = alloc_len(new_len);
            }
            if (storage.large) {
              is_small = false;
              std::memcpy(storage.large, s, new_len + 1);
            } else {
              is_small = true;
              storage.small[0] = '\0';
              new_len = 0;
            }
        }
        len = new_len;
      }

      void assign(const CString& other) noexcept { assign(other.c_str()); }

      void append(const char* s) noexcept {
        if (!s) return;
        std::size_t add = std::strlen(s);
        if (add == 0) return;
        std::size_t new_len = len + add;

        if (new_len <= SMALL_SIZE) {
          std::memcpy(storage.small + len, s, add + 1);
          len = new_len;
          return;
        }

        char* new_buf = alloc_len(new_len);
        if (!new_buf) return;

        std::memcpy(new_buf, c_str(), len);
        std::memcpy(new_buf + len, s, add + 1);

        if (!is_small && storage.large) std::free(storage.large);
        storage.large = new_buf;
        is_small = false;
        len = new_len;
      }

      void append(const CString& other) noexcept { append(other.c_str()); }

      CString& operator+=(const CString& other) noexcept { append(other); return *this; }
      CString& operator+=(const char* s) noexcept { append(s); return *this; }

      friend CString operator+(const CString& a, const CString& b) noexcept {
        CString out;
        std::size_t total = a.len + b.len;
        if (total <= SMALL_SIZE) {
          out.is_small = true;
          out.len = total;
          std::memcpy(out.storage.small, a.c_str(), a.len);
          std::memcpy(out.storage.small + a.len, b.c_str(), b.len + 1);
        } else {
          out.is_small = false;
          out.storage.large = alloc_len(total);
          if (out.storage.large) {
            std::memcpy(out.storage.large, a.c_str(), a.len);
            std::memcpy(out.storage.large + a.len, b.c_str(), b.len + 1);
            out.len = total;
          } else {
            out.is_small = true;
            out.storage.small[0] = '\0';
            out.len = 0;
          }
        }
        out.register_self();
        return out;
      }

      friend CString operator+(const CString& a, const char* b) noexcept {
        CString tmp(b);
        CString out = a + tmp;
        tmp.destroy();
        return out;
      }

      bool operator==(const CString& other) const noexcept {
        if (len != other.len) return false;
        return std::memcmp(c_str(), other.c_str(), len) == 0;
      }
      bool operator==(const char* s) const noexcept {
        if (!s) return len == 0;
        return std::strcmp(c_str(), s) == 0;
      }

      static void free_all() noexcept {
        while (registry_head) {
          CString* cur = registry_head;
          cur->destroy();
        }
      }
  };
}

namespace std {
  template<> struct hash<minis::CString> {
    std::size_t operator()(const minis::CString& s) const noexcept {
      const char* data = s.c_str();
      std::size_t h = 14695981039346656037ULL;
      for (std::size_t i = 0, n = s.size(); i < n; ++i) {
        h ^= static_cast<unsigned char>(data[i]);
        h *= 1099511628211ULL;
      }
      return h;
    }
  };

  template<> struct equal_to<minis::CString> {
    bool operator()(const minis::CString& a, const minis::CString& b) const noexcept {
      return a == b;
    }
  };
}
