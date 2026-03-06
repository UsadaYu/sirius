#pragma once

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <system_error>

#include "utils/env.h"
#include "utils/file.hpp"

namespace sirius {
namespace utils {
namespace ns {
inline constexpr std::string_view kSuffixShm = "_shm";
inline constexpr std::string_view kSuffixMutex = "_mutex";

/**
 * @brief 64-bit FNV-1a hash.
 */
inline uint64_t fnv1a64(const std::string &s) {
  uint64_t h = 14695981039346656037ULL;
  for (unsigned char c : s) {
    h ^= static_cast<uint64_t>(c);
    h *= 1099511628211ULL;
  }

  return h;
}

/**
 * @brief `hex` encoding uint64 -> string.
 */
inline std::string u64_to_hex(uint64_t v) {
  std::ostringstream oss;
  oss << std::hex << std::nouppercase << std::setfill('0') << std::setw(16)
      << v;

  return oss.str();
}

inline std::string sanitize_name(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (unsigned char c : s) {
    if (std::isalnum(c) || c == '_' || c == '-' || c == '.') {
      out += static_cast<char>(c);
    } else {
      out += '_';
    }
  }

  return out;
}

/**
 * @brief Combine and generate prefixes.
 */
inline std::string
generate_namespace_prefix(const std::string &runtime_salt = "") {
  std::string base = std::format("{0}|{1}|{2}", _SIRIUS_NAMESPACE,
                                 _SIRIUS_POSIX_FILE_MODE, _SIRIUS_USER_KEY);
  if (!runtime_salt.empty()) {
    base.append("|").append(runtime_salt);
  }

  std::transform(base.begin(), base.end(), base.begin(),
                 [](unsigned char c) -> char {
                   return static_cast<char>(std::tolower(c));
                 });

  uint64_t h = fnv1a64(base);

  std::string hex = u64_to_hex(h);
  std::string prefix = "ns_" + hex.substr(0, 16);

  return sanitize_name(prefix);
}

namespace shm {
inline std::string generate_name(std::string_view name,
                                 bool is_global = false) {
#if defined(_WIN32) || defined(_WIN64)
  std::string scope = is_global ? "Global\\" : "Local\\";
  std::string prefix = std::format("{0}{1}_{2}", scope, _SIRIUS_NAMESPACE,
                                   generate_namespace_prefix());
  std::string nm =
    prefix.append("_").append(sanitize_name(name)).append(kSuffixShm);

  return nm;
#else
  std::string prefix =
    std::format("{0}_{1}", _SIRIUS_NAMESPACE, generate_namespace_prefix());
  std::string nm =
    prefix.append("_").append(sanitize_name(name)).append(kSuffixShm);

  return nm;
#endif
}
} // namespace shm

/**
 * @implements Singleton pattern.
 */
class Mutex {
 private:
  Mutex() = default;

  ~Mutex() = default;

 public:
  Mutex(const Mutex &) = delete;
  Mutex &operator=(const Mutex &) = delete;
  Mutex(Mutex &&) = delete;
  Mutex &operator=(Mutex &&) = delete;

  static Mutex &instance() {
    static Mutex instance;

    return instance;
  }

#if defined(_WIN32) || defined(_WIN64)
  static std::string win_generate_name(std::string_view name,
                                       bool is_global = false) {
    std::string scope = is_global ? "Global\\" : "Local\\";
    std::string prefix = std::format("{0}{1}_{2}", scope, _SIRIUS_NAMESPACE,
                                     generate_namespace_prefix());
    std::string nm =
      prefix.append("_").append(sanitize_name(name)).append(kSuffixMutex);

    return nm;
  }
#endif

  auto file_lock_path(std::string_view name)
    -> std::expected<std::filesystem::path, std::string> {
    std::string m_name(name);
    std::filesystem::path lock_path = "";

    std::lock_guard lock(mutex_map_);

    if (auto it = map_.find(m_name); it != map_.end())
      return it->second;

    std::filesystem::path base = "";
    std::vector<std::filesystem::path> prefixes;

#if defined(_WIN32) || defined(_WIN64)
    TCHAR temp_path[MAX_PATH];
    DWORD dw_ret = GetTempPath(MAX_PATH, temp_path);
    if (dw_ret > 0 && dw_ret <= MAX_PATH) {
      prefixes.push_back(std::filesystem::path(temp_path));
    }
#else
    prefixes.push_back(_SIRIUS_POSIX_TMP_DIR);

    if (auto xdg = env::get_env("XDG_RUNTIME_DIR"); !xdg.empty()) {
      prefixes.push_back(xdg);
    }

    prefixes.push_back("/var/tmp");
#endif

    for (auto &prefix : prefixes) {
      if (prefix.empty())
        continue;

      try {
        std::filesystem::path b = prefix / _SIRIUS_NAMESPACE;
        std::filesystem::create_directories(b);
        base = b;
#if !defined(_WIN32) && !defined(_WIN64)
        (void)File::set_permissions_safe(base, _SIRIUS_POSIX_FILE_MODE);
#endif
        break;
      } catch (const std::filesystem::filesystem_error &e) {
        return std::unexpected(IO_E("\nfilesystem_error: {0}", e.what()));
      } catch (const std::exception &e) {
        return std::unexpected(IO_E("\nexception: {0}", e.what()));
      } catch (...) {
        return std::unexpected(IO_E("exception: unknow"));
      }
    }

    if (base.empty())
      return std::unexpected(IO_WSP("Fail to get file lock path"));

    std::string fname =
      std::format("{0}_{1}{2}.lock", generate_namespace_prefix(),
                  sanitize_name(m_name), kSuffixMutex);
    lock_path = base / fname;
    map_.insert(std::pair(m_name, lock_path));

    return lock_path;
  }

 private:
  std::mutex mutex_map_;
  std::map<std::string, std::filesystem::path> map_;
};
} // namespace ns
} // namespace utils
} // namespace sirius
