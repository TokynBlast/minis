#pragma once
#include <cstring>
#include <cstdlib>
#include <utility>
#include <functional>
#include <string_view>

namespace lang {

// Smart C string with small string optimization
// Stores short strings (≤15 chars) inline, longer strings on heap
class CString {
  static constexpr size_t SMALL_SIZE = 15;

  union {
    char small[SMALL_SIZE + 1];  // +1 for null terminator
    char* large;
  };

  size_t len = 0;
  bool is_small = true;

public:
  // Default constructor - empty string
  CString() {
    small[0] = '\0';
  }

  // Construct from C string
  CString(const char* s) {
    if (!s) s = "";
    len = std::strlen(s);

    if (len <= SMALL_SIZE) {
      is_small = true;
      std::strcpy(small, s);
    } else {
      is_small = false;
      large = static_cast<char*>(std::malloc(len + 1));
      std::strcpy(large, s);
    }
  }

  // Construct substring from pointer + length
  CString(const char* s, size_t length) {
    if (!s) { s = ""; length = 0; }

    len = length;
    if (len <= SMALL_SIZE) {
      is_small = true;
      std::strncpy(small, s, length);
      small[length] = '\0';
    } else {
      is_small = false;
      large = static_cast<char*>(std::malloc(len + 1));
      std::strncpy(large, s, length);
      large[length] = '\0';
    }
  }

  // Copy constructor
  CString(const CString& other)
    : len(other.len), is_small(other.is_small) {
    if (is_small) {
      std::strcpy(small, other.small);
    } else {
      large = static_cast<char*>(std::malloc(len + 1));
      std::strcpy(large, other.large);
    }
  }

  // Copy assignment
  CString& operator=(const CString& other) {
    if (this != &other) {
      // Clean up current state if needed
      if (!is_small) {
        std::free(large);
      }

      // Copy from other
      len = other.len;
      is_small = other.is_small;

      if (is_small) {
        std::strcpy(small, other.small);
      } else {
        large = static_cast<char*>(std::malloc(len + 1));
        std::strcpy(large, other.large);
      }
    }
    return *this;
  }

  // Destructor
  ~CString() {
    if (!is_small) {
      std::free(large);
    }
  }

  // Move constructor
  CString(CString&& other) noexcept
    : len(other.len), is_small(other.is_small) {
    if (is_small) {
      std::strcpy(small, other.small);
    } else {
      large = other.large;
      other.large = nullptr;
      other.is_small = true;
      other.small[0] = '\0';
    }
    other.len = 0;
  }

  // Move assignment
  CString& operator=(CString&& other) noexcept {
    if (this != &other) {
      // Clean up current state
      if (!is_small) {
        std::free(large);
      }

      // Move from other
      len = other.len;
      is_small = other.is_small;

      if (is_small) {
        std::strcpy(small, other.small);
      } else {
        large = other.large;
        other.large = nullptr;
        other.is_small = true;
        other.small[0] = '\0';
      }
      other.len = 0;
    }
    return *this;
  }

  // Assignment from C string
  CString& operator=(const char* s) {
    if (!s) s = "";
    size_t new_len = std::strlen(s);

    // Clean up current state if needed
    if (!is_small) {
      std::free(large);
    }

    len = new_len;
    if (len <= SMALL_SIZE) {
      is_small = true;
      std::strcpy(small, s);
    } else {
      is_small = false;
      large = static_cast<char*>(std::malloc(len + 1));
      std::strcpy(large, s);
    }
    return *this;
  }

  // Access
  const char* c_str() const {
    return is_small ? small : large;
  }

  // Mutable access for compatibility (use with caution!)
  char* data() {
    return is_small ? small : large;
  }

  // Implicit conversion to const char* for read-only compatibility
  operator const char*() const {
    return c_str();
  }

  // Implicit conversion to char* for legacy C code compatibility
  operator char*() {
    return data();
  }

  size_t size() const { return len; }
  bool empty() const { return len == 0; }

  // Element access
  char operator[](size_t index) const {
    if (index >= len) return '\0';
    return is_small ? small[index] : large[index];
  }

  // Mutable element access
  char& operator[](size_t index) {
    if (index >= len) {
      static char null_char = '\0';
      return null_char; // Return reference to static null for out-of-bounds
    }
    return is_small ? small[index] : large[index];
  }

  // Comparison
  bool operator==(const CString& other) const {
    return std::strcmp(c_str(), other.c_str()) == 0;
  }

  bool operator==(const char* s) const {
    return std::strcmp(c_str(), s ? s : "") == 0;
  }

  // NEW: String concatenation operators
  CString operator+(const CString& other) const {
    size_t total_len = len + other.len;
    char* buffer = static_cast<char*>(std::malloc(total_len + 1));

    std::strcpy(buffer, c_str());
    std::strcat(buffer, other.c_str());

    CString result(buffer);
    std::free(buffer);
    return result;
  }

  CString operator+(const char* other) const {
    if (!other) return CString(*this);

    size_t other_len = std::strlen(other);
    size_t total_len = len + other_len;
    char* buffer = static_cast<char*>(std::malloc(total_len + 1));

    std::strcpy(buffer, c_str());
    std::strcat(buffer, other);

    CString result(buffer);
    std::free(buffer);
    return result;
  }

  // Compound assignment for concatenation
  CString& operator+=(const CString& other) {
    *this = *this + other;
    return *this;
  }

  CString& operator+=(const char* other) {
    *this = *this + other;
    return *this;
  }

  // Debug info
  bool using_heap() const { return !is_small; }
};

// Helper for creating CStrings from literals
inline CString cstr(const char* s) {
  return CString(s);
}

// Global operators for const char* + CString
inline CString operator+(const char* left, const CString& right) {
  return CString(left) + right;
}

}

// Add hash specialization for std::unordered_map compatibility
namespace std {
  template<>
  struct hash<lang::CString> {
    std::size_t operator()(const lang::CString& s) const noexcept {
      // Use standard string hash on the C string data
      return std::hash<std::string_view>{}(std::string_view(s.c_str(), s.size()));
    }
  };

  template<>
  struct equal_to<lang::CString> {
    bool operator()(const lang::CString& lhs, const lang::CString& rhs) const {
      return lhs == rhs;
    }
  };
}