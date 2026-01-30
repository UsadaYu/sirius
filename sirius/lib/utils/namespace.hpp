#pragma once

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <sstream>
#include <system_error>

#include "utils/config.h"
#include "utils/decls.h"
#include "utils/utils.h"

namespace Utils {
namespace Ns {
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

inline std::string sanitize_name(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (unsigned char c : s) {
    if (std::isalnum(c) || c == '_' || c == '-' || c == '.') {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('_');
    }
  }

  return out;
}

/**
 * @brief Combine and generate prefixes.
 */
inline std::string
generate_namespace_prefix(const std::string &runtime_salt = "") {
  std::string base = std::string(sirius_namespace) + "|" +
    std::string(sirius_posix_file_mode) + "|" + std::string(sirius_user_key);
  if (!runtime_salt.empty())
    base += "|" + runtime_salt;

  std::transform(base.begin(), base.end(), base.begin(), [](unsigned char c) {
    return std::tolower(c);
  });

  uint64_t h = fnv1a64(base);

  std::string hex = u64_to_hex(h);
  std::string prefix = "ns_" + hex.substr(0, 16);

  return sanitize_name(prefix);
}

inline std::string get_shm_name(const std::string &name) {
#if defined(_WIN32) || defined(_WIN64)
  /**
   * @note Windows uses `Local\\` or `Global\\` for some prefixes of named
   * objects, which cannot be included as a partition.
   */

  std::string prefix = std::string("Local\\") + std::string(sirius_namespace) +
    "_" + generate_namespace_prefix();
  std::string nm = prefix + "_" + sanitize_name(name);

  return nm;
#else
  std::string prefix =
    std::string(sirius_namespace) + "_" + generate_namespace_prefix();
  std::string nm = prefix + "_" + sanitize_name(name);

  return nm;
#endif
}

#if defined(_WIN32) || defined(_WIN64)
inline std::string win_lock_name(const std::string &name) {
  std::string prefix = std::string("Local\\") + std::string(sirius_namespace) +
    "_" + generate_namespace_prefix();
  std::string nm = prefix + "_" + sanitize_name(name) + "_lock";

  return nm;
}
#else
inline std::map<std::string, std::filesystem::path> map_lock_name;
inline std::filesystem::path posix_lockfile_path(const std::string &name) {
  auto it = map_lock_name.find(name);
  if (it != map_lock_name.end())
    return it->second;

  std::filesystem::path lock_path = "";
  std::filesystem::path base = "";
  std::vector<std::filesystem::path> prefixes;

  prefixes.push_back(sirius_posix_tmp_dir);

  const char *xdg = std::getenv("XDG_RUNTIME_DIR");
  if (xdg) {
    prefixes.push_back(xdg);
  }

  prefixes.push_back("/var/tmp");

  for (auto &prefix : prefixes) {
    if (prefix.empty())
      continue;

    try {
      std::filesystem::path b = prefix / sirius_namespace;
      std::filesystem::create_directories(b);
      base = b;
      File::set_permissions_safe(base, sirius_posix_file_mode);
      break;
    } catch (const std::filesystem::filesystem_error &e) {
#  if (sirius_log_level >= sirius_log_level_debug)
      UTILS_DPRINTF(STDERR_FILENO, "`filesystem_error`: %s\n", e.what());
#  endif
    } catch (const std::exception &e) {
#  if (sirius_log_level >= sirius_log_level_debug)
      UTILS_DPRINTF(STDERR_FILENO, "`exception`: %s\n", e.what());
#  endif
    }
  }

  if (base.empty())
    return "";

  std::string prefix = generate_namespace_prefix();
  std::string fname = prefix + "_" + sanitize_name(name) + ".lock";
  lock_path = base / fname;
  map_lock_name.insert(
    std::pair<std::string, std::filesystem::path>(name, lock_path));

  return lock_path;
}
#endif
} // namespace Ns
} // namespace Utils
