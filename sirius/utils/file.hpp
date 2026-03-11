#pragma once

#include "utils/io.hpp"

namespace sirius {
namespace utils {
class File {
 private:
  File() = default;

  ~File() = default;

 public:
  File(const File &) = delete;
  File &operator=(const File &) = delete;

#if !defined(_WIN32) && !defined(_WIN64)
  static mode_t string_to_mode(const std::string &mode_str) {
    return static_cast<mode_t>(std::stoul(mode_str, nullptr, 8));
  }
#endif

  static std::filesystem::perms string_to_perms(std::string_view str) {
    std::string perm_str(str);
    if (perm_str.size() > 1 && perm_str.starts_with('0')) {
      perm_str = perm_str.substr(1);
    }

    uint32_t perm_value;
    try {
      perm_value = std::stoi(perm_str, nullptr, 8);
    } catch (...) {
      return std::filesystem::perms::none;
    }
    return static_cast<std::filesystem::perms>(perm_value);
  }

  static auto set_permissions_safe(const std::filesystem::path &path,
                                   std::string_view perm_str,
                                   bool symlink_perms = false)
    -> std::expected<void, UTrace> {
    if (!std::filesystem::exists(path)) {
      auto es = std::format("\nNo such file or directory: {0}", path.string());
      return std::unexpected(UTrace(std::move(es)));
    }

    try {
      std::filesystem::perms perm = string_to_perms(perm_str);
      std::filesystem::perm_options options =
        std::filesystem::perm_options::replace;
      if (symlink_perms) {
        options |= std::filesystem::perm_options::nofollow;
      }
      std::filesystem::permissions(path, perm, options);
      return {};
    } catch (const std::exception &e) {
      auto es = std::format("\nexception: {0}", e.what());
      return std::unexpected(UTrace(std::move(es)));
    } catch (...) {
      return std::unexpected(UTrace("exception: unknow"));
    }
  }

  static auto get_exe_path_matrix(std::string_view exe_name,
                                  const std::filesystem::path &base_dir)
    -> std::expected<std::filesystem::path, int /* errno */> {
    if (exe_name.empty() || base_dir.empty())
      return std::unexpected(EINVAL);

    std::filesystem::path exe_path = base_dir / exe_name;
    if (std::filesystem::exists(exe_path))
      return exe_path;

#if defined(_WIN32) || defined(_WIN64)
    constexpr std::string_view kSuffix = ".exe";
    if (exe_name.ends_with(kSuffix)) {
      exe_path =
        base_dir / exe_name.substr(0, exe_name.length() - kSuffix.length());
    } else {
      exe_path = base_dir / std::string(exe_name).append(kSuffix);
    }
    if (std::filesystem::exists(exe_path))
      return exe_path;
#endif

    return std::unexpected(ENOENT);
  }
};
} // namespace utils
} // namespace sirius
