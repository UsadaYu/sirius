#pragma once

#include <unordered_map>

#include "utils/io.hpp"

namespace Utils {
namespace Args {
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

class Parser {
 public:
  Parser() = default;

  ~Parser() = default;

  void add_option(const std::string &option,
                  std::vector<std::string> allowed_values = {},
                  bool required = false, bool multiple = false,
                  const std::string &help = {}) {
    OptionSpec s;

    s.requires_value = true;
    s.allowed_values = std::move(allowed_values);
    s.required = required;
    s.multiple = multiple;
    s.help = help;
    specs_[option] = std::move(s);
  }

  void add_flag(const std::string &option, const std::string &help = {}) {
    OptionSpec s;

    s.requires_value = false;
    s.multiple = false;
    s.required = false;
    s.help = help;
    specs_[option] = std::move(s);
  }

  auto parse(int argc, char **argv) -> std::expected<void, std::string> {
    parsed_.clear();
    positional_.clear();

    auto is_option = [](const std::string &s) {
      return s.size() >= 2 && s[0] == '-' && s[1] == '-';
    };

#define PARSE_ERROR (Io::io().s_error(SIRIUS_FILE_NAME, __LINE__))

    for (int i = 1; i < argc; ++i) {
      std::string token = argv[i];

      if (is_option(token)) {
        /**
         * @note Support the form: option=value.
         */
        std::string option, value;
        size_t eq = token.find('=');
        if (eq != std::string::npos) {
          option = token.substr(2, eq - 2);
          value = token.substr(eq + 1);
        } else {
          option = token.substr(2);
        }

        auto it = specs_.find(option);
        if (it == specs_.end()) {
          return std::unexpected(
            PARSE_ERROR + Io::row_gs("\nUnknown option `--{0}`", option));
        }

        const OptionSpec &spec = it->second;
        if (!spec.requires_value) {
          parsed_[option].push_back("true");
          continue;
        } else {
          if (value.empty()) {
            if (i + 1 >= argc) {
              return std::unexpected(
                PARSE_ERROR +
                Io::row_gs("\nMissing value for option `--{0}`", option));
            }
            std::string next = argv[++i];
            if (is_option(next)) {
              return std::unexpected(
                PARSE_ERROR +
                Io::row_gs("\nMissing value for option `--{0}` "
                           "(next token looks like an option: {1})",
                           option, next));
            }
            value = next;
          }
          if (!spec.allowed_values.empty()) {
            bool ok = false;
            for (auto const &av : spec.allowed_values) {
              if (av == value) {
                ok = true;
                break;
              }
            }
            if (!ok) {
              return std::unexpected(
                PARSE_ERROR +
                Io::row_gs("\nInvalid value for option `--{0}`: {1}"
                           "\nAllowed: {2}",
                           option, value, join(spec.allowed_values, ", ")));
            }
          }
          if (!spec.multiple && parsed_.count(option) &&
              !parsed_[option].empty()) {
            return std::unexpected(
              PARSE_ERROR +
              Io::row_gs("\nOption `--{0}` may only appear once", option));
          }
          parsed_[option].push_back(value);
        }
      } else {
        positional_.push_back(token);
      }
    }

    for (auto const &[k, spec] : specs_) {
      if (spec.required) {
        if (!parsed_.count(k) || parsed_.at(k).empty()) {
          return std::unexpected(
            PARSE_ERROR + Io::row_gs("\nRequired option missing `--{0}`", k));
        }
      }
    }

    return {};
#undef PARSE_ERROR
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

    return parsed_.at(option).front();
  }

  /**
   * @return  Return all values (for multiple).
   */
  std::vector<std::string> get_all(const std::string &option) const {
    if (!has(option))
      return {};

    return parsed_.at(option);
  }

  const std::vector<std::string> &get_positional() const {
    return positional_;
  }

  void print_usage(const std::string &prog_name) const {
    auto info = [](const std::string &msg) {
      UTILS_WRITE(STDOUT_FILENO, msg.c_str(), msg.size());
    };

    info("Usage: " + prog_name + " [options]\n\nOptions:\n");
    for (auto const &[option, spec] : specs_) {
      std::string line = "  --" + option;
      if (spec.requires_value)
        line += " <value>";
      if (!spec.help.empty()) {
        line += "  " + spec.help;
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
                          const std::string &sep) {
    std::string s;

    for (size_t i = 0; i < v.size(); ++i) {
      if (i) {
        s += sep;
      }
      s += v[i];
    }

    return s;
  }
};
} // namespace Args
} // namespace Utils
