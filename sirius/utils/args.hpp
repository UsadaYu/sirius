#pragma once

#include <algorithm>
#include <unordered_map>

#include "utils/io.hpp"

namespace Utils {
namespace Args {
/**
 * @note Most arguments are first stored in stack variables after they are
 * parsed, long-lived arguments cannot be of the `std::string_view` type here.
 */
class Parser {
 public:
  struct OptionSpec {
    /**
     * @brief `true` indicates that a value is needed; `false` indicates it is a
     * flag (switch).
     */
    bool requires_value = true;

    /**
     * @brief Whether multiple occurrences are allowed.
     */
    bool multiple = false;

    /**
     * @brief Whether it is necessary to provide.
     */
    bool required = false;

    /**
     * @brief If not empty, only these values are allowed.
     */
    std::vector<std::string> allowed_values;

    std::string help;
  };

 public:
  Parser() = default;

  ~Parser() = default;

  auto add_option(const std::string &option, bool requires_value,
                  const std::vector<std::string> &allowed_values = {},
                  bool required = false, bool multiple = false,
                  const std::string &help = {})
    -> std::expected<void, std::string> {
    OptionSpec s;

    auto opt_pos = option.find_first_not_of("\r\n\t\b ");
    if (opt_pos == std::string::npos || option.contains("=")) {
      return std::unexpected(
        IO_ERROR("\nInvalid argument. `option`: {0}"
                 "\nThe option string cannot be empty or contains `=`",
                 option));
    }

    if (requires_value) {
      if (allowed_values.empty()) {
        return std::unexpected(
          IO_ERROR("\nMissing value for option `--{0}`", option));
      }

      for (auto v : allowed_values) {
        if (v.find_first_not_of("\r\n\t\b ") == std::string::npos ||
            v.starts_with("--")) {
          return std::unexpected(
            IO_ERROR("\nInvalid argument. option `--{0}`, `value`: {1}"
                     "\nThe value string cannot be empty or starts with `--`",
                     option, v));
        }
      }
    }

    s.requires_value = requires_value;
    s.allowed_values = std::move(allowed_values);
    s.required = required;
    s.multiple = multiple;
    s.help = help;
    specs_[option] = std::move(s);

    return {};
  }

  bool is_option_token(std::string_view token) const {
    return token.starts_with("--");
  }

  /**
   * @brief Parse the `option` and `value` from the `token` string. If the
   * format is `=`, both `option` and `value` will not be empty; otherwise,
   * `option` has a value while `value` is empty.
   *
   * @note Support the form: --option=value.
   */
  auto extract(std::string_view token, std::string &option, std::string &value)
    -> std::expected<void, std::string> {
    if (token.size() < 3 || !is_option_token(token)) {
      return std::unexpected(
        IO_ERROR("\nInvalid argument. `token` (option): {0}", token));
    }

    if (size_t eq = token.find('=', 2); eq != std::string_view::npos) {
      option = token.substr(2, eq - 2);
      value = token.substr(eq + 1);
      return {};
    }

    option = token.substr(2);
    value = {};

    return {};
  }

  auto parse(int argc, char **argv) -> std::expected<void, std::string> {
    parsed_.clear();
    positional_.clear();

    for (int i = 1; i < argc; ++i) {
      std::string token = argv[i];
      if (!is_option_token(token)) {
        positional_.push_back(token);
        continue;
      }

      std::string option, value;
      if (auto ret = extract(token, option, value); !ret.has_value()) {
        return std::unexpected(ret.error());
      }
      if (!specs_.contains(option)) {
        return std::unexpected(
          IO_ERROR("\nInvalid argument. `option`: {0}", option));
      }

      const auto &spec = specs_.find(option)->second;
      if (!spec.requires_value) {
        parsed_[option].push_back("");
        continue;
      }

      if (value.empty()) {
        if (i + 1 >= argc || is_option_token(argv[i + 1])) {
          return std::unexpected(
            IO_ERROR("\nMissing value for option `--{0}`", option));
        }
        value = std::string(argv[++i]);
      }

      // requires_value
      if (!std::ranges::contains(spec.allowed_values, value)) {
        return std::unexpected(IO_ERROR(
          "\nInvalid argument. option `--{0}`, `value`: {1}. Allowed: {2}",
          option, value, join(spec.allowed_values, ", ")));
      }

      if (!spec.multiple && parsed_.count(option) && !parsed_[option].empty()) {
        return std::unexpected(
          IO_ERROR("\nOption `--{0}` may only appear once", option));
      }
      parsed_[option].push_back(value);
    }

    for (auto const &[option, spec] : specs_) {
      if (spec.required) {
        if (!parsed_.count(option) || parsed_.at(option).empty()) {
          return std::unexpected(
            IO_ERROR("\nRequired option missing `--{0}`", option));
        }
      }
    }

    return {};
  }

  bool has(const std::string &option) const {
    return parsed_.count(option) && !parsed_.at(option).empty();
  }

  /**
   * @return Return the first value (an null string if none).
   */
  std::string get(const std::string &option,
                  const std::string &def = "") const {
    if (!has(option))
      return def;

    auto it = specs_.find(option);
    if (it == specs_.end())
      return def;

    return it->second.requires_value ? parsed_.at(option).front() : def;
  }

  /**
   * @return  Return all values (for multiple).
   */
  std::vector<std::string> get_all(const std::string &option) const {
    if (!has(option))
      return {};

    auto it = specs_.find(option);
    if (it == specs_.end())
      return {};

    return it->second.requires_value ? parsed_.at(option)
                                     : std::vector<std::string> {};
  }

  const std::vector<std::string> &get_positional() const {
    return positional_;
  }

  void print_usage(std::string_view prog_name) const {
    auto info = [](std::string_view msg) {
      utils_write(STDOUT_FILENO, msg.data(), msg.size());
    };

    info(std::string("Usage: ").append(prog_name).append(
      " [options]\n\nOptions:\n"));
    for (auto const &[option, spec] : specs_) {
      std::string line = std::string("  --").append(option);
      if (spec.requires_value)
        line += " <value>";
      if (!spec.help.empty()) {
        line += std::string("  ").append(spec.help);
      }
      if (!spec.allowed_values.empty()) {
        line += " (allowed: " + join(spec.allowed_values, "|") + ")";
      }
      if (spec.required) {
        line += " [required]";
      }
      if (spec.multiple) {
        line += " [multiple]";
      }
      info(line + "\n");
    }
  }

 private:
  std::unordered_map<std::string, OptionSpec> specs_;
  std::unordered_map<std::string, std::vector<std::string>> parsed_;
  std::vector<std::string> positional_;

  static std::string join(const std::vector<std::string> &v,
                          std::string_view sep) {
    std::string s;

    for (size_t i = 0; i < v.size(); ++i) {
      if (i) {
        s.append(sep);
      }
      s.append(v[i]);
    }

    return s;
  }
};
} // namespace Args
} // namespace Utils
