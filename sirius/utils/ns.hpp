#pragma once

#include <algorithm>
#include <map>
#include <mutex>

#include "utils/config.h"
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
inline uint64_t fnv1a64(std::string_view s) {
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
  return std::format("{:016x}", v);
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

inline std::string
generate_namespace_prefix(std::string_view runtime_salt = "") {
  static std::string_view base = std::string(_SIRIUS_NAMESPACE)
                                   .append("|")
                                   .append(_SIRIUS_POSIX_FILE_MODE)
                                   .append("|")
                                   .append(_SIRIUS_USER_KEY);
  std::string s;
  if (!runtime_salt.empty()) {
    s = std::string(base).append("|").append(runtime_salt);
  }
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) -> char {
    return static_cast<char>(std::tolower(c));
  });
  uint64_t h = fnv1a64(s);
  std::string hex = u64_to_hex(h);
  std::string prefix = "ns_" + hex.substr(0, 16);
  return sanitize_name(prefix);
}

namespace shm {
inline std::string generate_name(std::string_view name,
                                 bool is_global /* Windows */ = false) {
#if defined(_WIN32) || defined(_WIN64)
  std::string scope = is_global ? "Global\\" : "Local\\";
  std::string prefix = scope.append(_SIRIUS_NAMESPACE)
                         .append("_")
                         .append(generate_namespace_prefix());
  std::string nm =
    prefix.append("_").append(sanitize_name(name)).append(kSuffixShm);
  return nm;
#else
  std::string prefix = std::string(_SIRIUS_NAMESPACE)
                         .append("_")
                         .append(generate_namespace_prefix());
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
    std::string prefix = scope.append(_SIRIUS_NAMESPACE)
                           .append("_")
                           .append(generate_namespace_prefix());
    std::string nm =
      prefix.append("_").append(sanitize_name(name)).append(kSuffixMutex);
    return nm;
  }
#endif

  auto file_lock_path(std::string_view name)
    -> std::expected<std::filesystem::path, UTrace> {
    std::string m_name(name);
    std::filesystem::path lock_path {};
    std::filesystem::path base {};
    std::vector<std::filesystem::path> prefixes {};

    auto append_prefix = [&](std::filesystem::path path) {
      if (!path.empty() && !std::ranges::contains(prefixes, path)) {
        prefixes.push_back(path);
      }
    };

    std::lock_guard lock(mutex_map_);

    if (auto it = map_.find(m_name); it != map_.end())
      return it->second;

    append_prefix(_SIRIUS_TMP_DIR);
#if defined(_WIN32) || defined(_WIN64)
    TCHAR temp_path[MAX_PATH];
    DWORD dw_ret = GetTempPath(MAX_PATH, temp_path);
    if (dw_ret > 0 && dw_ret <= MAX_PATH) {
      append_prefix(temp_path);
    }
#else
    append_prefix(env::get_env("XDG_RUNTIME_DIR"));
    append_prefix("/var/tmp");
#endif

    for (auto &prefix : prefixes) {
      std::string err_msg;
      try {
        std::filesystem::path b = prefix / _SIRIUS_NAMESPACE;
        std::filesystem::create_directories(b);
        base = b;
#if !defined(_WIN32) && !defined(_WIN64)
        (void)File::set_permissions_safe(base, _SIRIUS_POSIX_FILE_MODE);
#endif
        break;
      } catch (const std::filesystem::filesystem_error &e) {
        err_msg = std::format("\nexception (filesystem_error): {0}", e.what());
      } catch (const std::exception &e) {
        err_msg = std::format("exception: {0}", e.what());
      } catch (...) {
        err_msg = "exception: unknow";
      }
      return std::unexpected(UTrace(std::move(err_msg)));
    }

    if (base.empty())
      return std::unexpected(UTrace("Fail to get the file lock directory"));
    std::string fname = generate_namespace_prefix()
                          .append("_")
                          .append(sanitize_name(m_name))
                          .append(kSuffixMutex)
                          .append(".lock");
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
