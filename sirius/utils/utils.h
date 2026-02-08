#pragma once

#include "utils/attributes.h"
#include "utils/config.h"
#include "utils/decls.h"

#ifdef __cplusplus
#  include <filesystem>
#endif

// --- max / min ---
#ifndef UTILS_MIN
#  define UTILS_MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef UTILS_MAX
#  define UTILS_MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

// --- UTILS_CONCAT ---
#ifdef UTILS_CONCAT_IMPL
#  undef UTILS_CONCAT_IMPL
#endif
#ifdef UTILS_CONCAT
#  undef UTILS_CONCAT
#endif

#define UTILS_CONCAT_IMPL(x, y) x##y
#define UTILS_CONCAT(x, y) UTILS_CONCAT_IMPL(x, y)

// --- UTILS_LOCALTIME_R ---
#ifdef UTILS_LOCALTIME_R
#  undef UTILS_LOCALTIME_R
#endif

#if defined(_WIN32) || defined(_WIN64)
#  define UTILS_LOCALTIME_R(timer, result) localtime_s(result, timer)
#else
#  define UTILS_LOCALTIME_R(timer, result) localtime_r(timer, result)
#endif

static inline size_t utils_string_length_check(const char *string,
                                               size_t max_len) {
  if (!string)
    return 0;

  size_t len = 0;
  while (len < max_len) {
    if (string[len] == '\0')
      break;
    ++len;
  }

  return likely(len < max_len) ? len : max_len;
}

#ifdef __cplusplus
namespace Utils {
namespace Utils {
template<typename T>
concept IntegralOrEnum = std::integral<T> || std::is_enum_v<T>;

constexpr size_t next_power_of_2(size_t n) {
  if (n <= 1)
    return 2;

  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
#  if defined(__SIZEOF_SIZE_T__) && __SIZEOF_SIZE_T__ == 8
  n |= n >> 32;
#  endif
  n++;

  return n;
}
} // namespace Utils
} // namespace Utils
#else
static inline size_t utils_next_power_of_2(size_t n) {
  if (n <= 1)
    return 2;

  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
#  if defined(__SIZEOF_SIZE_T__) && __SIZEOF_SIZE_T__ == 8
  n |= n >> 32;
#  endif
  n++;

  return n;
}
#endif

#ifdef __cplusplus
namespace Utils {
namespace File {
#  if !defined(_WIN32) && !defined(_WIN64)
static inline mode_t string_to_mode(const std::string &mode_str) {
  return static_cast<mode_t>(std::stoul(mode_str, nullptr, 8));
}
#  endif

static inline std::filesystem::perms
string_to_perms(const std::string &perm_str) {
  std::string str = perm_str;
  if (str.size() > 1 && str[0] == '0') {
    str = str.substr(1);
  }

  uint32_t perm_value;
  try {
    perm_value = std::stoi(str, nullptr, 8);
  } catch (...) {
    return std::filesystem::perms::none;
  }

  return static_cast<std::filesystem::perms>(perm_value);
}

static inline bool set_permissions_safe(const std::filesystem::path &path,
                                        const std::string &perm_str,
                                        bool symlink_perms = false) {
  try {
    if (!std::filesystem::exists(path))
      return false;

    std::filesystem::perms perm = string_to_perms(perm_str);

    std::filesystem::perm_options options =
      std::filesystem::perm_options::replace;
    if (symlink_perms) {
      options |= std::filesystem::perm_options::nofollow;
    }

    std::filesystem::permissions(path, perm, options);
    return true;
  } catch (const std::exception &e) {
    std::string es = std::format("`exception`: {}\n", e.what());
#  if defined(_WIN32) || defined(_WIN64)
    _write(2, es.c_str(), (unsigned int)es.length());
#  else
    write(2, es.c_str(), es.length());
#  endif
    return false;
  } catch (...) {
    return false;
  }
}

static inline std::filesystem::path
get_exe_path_matrix(std::string exe_name, std::filesystem::path base_dir) {
  if (exe_name.empty() || base_dir.empty())
    return "";

  std::filesystem::path exe_path = base_dir / exe_name;

  if (std::filesystem::exists(exe_path))
    return exe_path;

#  if defined(_WIN32) || defined(_WIN64)
  const std::string suffix = ".exe";
  if (exe_name.ends_with(suffix)) {
    exe_path =
      base_dir / exe_name.substr(0, exe_name.length() - suffix.length());
  } else {
    exe_path = base_dir / (exe_name + suffix);
  }

  if (std::filesystem::exists(exe_path))
    return exe_path;
#  endif

  return "";
}
} // namespace File
} // namespace Utils
#endif
