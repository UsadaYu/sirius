#pragma once

#include <unordered_map>

#include "utils/io.h"

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

  bool parse(int argc, char **argv) {
    parsed_.clear();
    positional_.clear();

    auto is_option = [](const std::string &s) {
      return s.size() >= 2 && s[0] == '-' && s[1] == '-';
    };

    auto os_error = [](const std::string &msg) {
      utils_dprintf(STDERR_FILENO,
                    "Error %s\n"
                    "  %s\n",
                    utils_pretty_function, msg.c_str());
    };

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
          os_error("Unknown option `--" + option + "`");
          return false;
        }

        const OptionSpec &spec = it->second;
        if (!spec.requires_value) {
          parsed_[option].push_back("true");
          continue;
        } else {
          if (value.empty()) {
            if (i + 1 >= argc) {
              os_error("Missing value for option `--" + option + "`");
              return false;
            }
            std::string next = argv[++i];
            if (is_option(next)) {
              os_error("Missing value for option `--" + option + "`" +
                       " (next token looks like an option: " + next + ")");
              return false;
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
              os_error("Invalid value for option `--" + option + "`: " + value +
                       ". Allowed: " + join(spec.allowed_values, ", "));
              return false;
            }
          }
          if (!spec.multiple && parsed_.count(option) &&
              !parsed_[option].empty()) {
            os_error("Option `--" + option + "` may only appear once.");
            return false;
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
          os_error("Required option missing `--" + k + "`");
          return false;
        }
      }
    }

    return true;
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
    auto os_info = [](const std::string &msg) {
      utils_dprintf(STDOUT_FILENO, "%s", msg.c_str());
    };

    os_info("Usage: " + prog_name + " [options]\n\nOptions:\n");
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
      os_info(line + "\n");
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
