#pragma once
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include <algorithm>
#include <unordered_map>

#include "utils/io.hpp"

namespace sirius {
namespace utils {
namespace args {
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
                  const std::string &help = {}) -> std::expected<void, UTrace> {
    if (first_valid_pos(option) != 0 || option.contains("=")) {
      auto es = std::format(
        "\nInvalid argument. `option`: {0}"
        "\nThe option string cannot start with empty or contain `=`",
        option);
      return std::unexpected(UTrace(std::move(es)));
    }

    if (requires_value) {
      if (allowed_values.empty()) {
        auto es = std::format("Missing value for option `--{0}`", option);
        return std::unexpected(UTrace(std::move(es)));
      }
      for (auto v : allowed_values) {
        if (first_valid_pos(v) != 0 || v.starts_with("--")) {
          auto es = std::format(
            "\nInvalid argument. option `--{0}`, `value`: {1}"
            "\nThe value string cannot start with empty or `--`",
            option, v);
          return std::unexpected(UTrace(std::move(es)));
        }
      }
    }

    OptionSpec s;
    s.requires_value = requires_value;
    s.allowed_values = std::move(allowed_values);
    s.required = required;
    s.multiple = multiple;
    s.help = help;
    specs_[option] = std::move(s);
    return {};
  }

  static bool is_option_token(std::string_view token) {
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
    -> std::expected<void, UTrace> {
    constexpr size_t kOptionPos = 2;

    if (!is_option_token(token) ||
        first_valid_pos(token.substr(kOptionPos)) != 0) {
      auto es = std::format("Invalid argument. `token` (option): {0}", token);
      return std::unexpected(UTrace(std::move(es)));
    }

    if (size_t eq = token.find('=', kOptionPos); eq != std::string_view::npos) {
      option = token.substr(kOptionPos, eq - kOptionPos);
      value = token.substr(eq + 1);
      if (first_valid_pos(value) != 0) {
        auto es = std::format("Missing valid value for option `--{0}`", option);
        return std::unexpected(UTrace(std::move(es)));
      }
      return {};
    }

    option = token.substr(kOptionPos);
    value = {};
    return {};
  }

  auto parse(int argc, char **argv) -> std::expected<void, UTrace> {
    parsed_.clear();

    for (int i = 1; i < argc; ++i) {
      std::string token = argv[i];
      std::string option, value;

      if (auto ret = extract(token, option, value); !ret.has_value()) {
        utrace_return(ret);
      }

      const OptionSpec *spec = nullptr;
      if (auto it = specs_.find(option); it == specs_.end()) {
        auto es = std::format("Invalid argument. `option`: {0}", option);
        return std::unexpected(UTrace(std::move(es)));
      } else {
        spec = &it->second;
      }
      if (!spec->multiple && parsed_.count(option) &&
          !parsed_[option].empty()) {
        auto es = std::format("Option `--{0}` may only appear once", option);
        return std::unexpected(UTrace(std::move(es)));
      }
      if (!spec->requires_value) {
        parsed_[option].push_back("__PLACEHOLDER__");
        continue;
      }

      if (value.empty()) {
        if (i + 1 >= argc || is_option_token(argv[i + 1]) ||
            first_valid_pos(argv[i + 1]) != 0) {
          auto es =
            std::format("Missing valid value for option `--{0}`", option);
          return std::unexpected(UTrace(std::move(es)));
        }
        value = std::string(argv[++i]);
      }

      // requires_value
      if (!std::ranges::contains(spec->allowed_values, value)) {
        auto es = std::format(
          "\nInvalid argument. The `value` for option `--{0}`. "
          "Actual: {1}; Allowed: {2}",
          option, value, vector_join(spec->allowed_values, ", "));
        return std::unexpected(UTrace(std::move(es)));
      }

      parsed_[option].push_back(value);
    }

    for (auto const &[option, spec] : specs_) {
      if (spec.required) {
        if (!parsed_.count(option) || parsed_.at(option).empty()) {
          auto es = std::format("Required option missing `--{0}`", option);
          return std::unexpected(UTrace(std::move(es)));
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

  void print_usage(std::string_view prog_name) const {
    io::println_out(std::string("Usage: ").append(prog_name).append(
      " [options]\n\nOptions:"));
    for (auto const &[option, spec] : specs_) {
      std::string line = std::string("  --").append(option);
      if (spec.requires_value) {
        line.append(" <value>");
      }
      if (!spec.help.empty()) {
        line.append("  ").append(spec.help);
      }
      if (!spec.allowed_values.empty()) {
        line.append(" (allowed: ")
          .append(vector_join(spec.allowed_values, "|"))
          .append(")");
      }
      if (spec.required) {
        line.append(" [required]");
      }
      if (spec.multiple) {
        line.append(" [multiple]");
      }
      io::println_out(line);
    }
  }

 private:
  std::unordered_map<std::string, OptionSpec> specs_ {};
  std::unordered_map<std::string, std::vector<std::string>> parsed_ {};

  /**
   * @return `std::string_view::npos` on failure.
   */
  constexpr size_t first_valid_pos(std::string_view s) {
    for (size_t i = 0; i < s.size(); ++i) {
      if (char ch = s.at(i); !std::isspace(ch) && ch != '\0')
        return i;
    }
    return std::string_view::npos;
  }

  static std::string vector_join(const std::vector<std::string> &v,
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
} // namespace args
} // namespace utils
} // namespace sirius
