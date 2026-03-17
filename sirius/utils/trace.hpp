#pragma once
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include <vector>

namespace sirius {
struct UTraceContext {
  std::string function_name;
  std::string file_name;
  uint32_t line;

  std::string to_string() const {
    return std::format("{0:<18} {1}", std::format("{0}:{1}", file_name, line),
                       function_name);
  }
};

class UTrace {
 public:
  explicit UTrace(std::string message,
                  std::source_location loc = std::source_location::current())
      : message_(std::move(message)) {
    add_location(loc);
  }

  void
  add_location(std::source_location loc = std::source_location::current()) {
    std::filesystem::path path(loc.file_name());
    trace_.push_back(
      {loc.function_name(), path.filename().string(), loc.line()});
  }

  const std::string &message() const { return message_; }
  const std::vector<UTraceContext> &trace() const { return trace_; }

  void msg_append(std::string_view message) {
    if (!message.empty()) {
      if (message.starts_with('\n')) {
        message_.append(message);
      } else {
        message_.append(std::format("\n{0}", message));
      }
    }
  }

  std::string
  join_self_all(std::source_location loc = std::source_location::current()) {
    add_location(loc);
    return join_all();
  }

  std::string join_all() const {
    std::string result = message_;
    if (!message_.empty() && !message_.ends_with('\n')) {
      result.push_back('\n');
    }
    return result.append(join_trace());
  }

  std::string join_trace() const {
    std::string result;
    for (auto it = trace_.begin(); it != trace_.end(); ++it) {
      if (it != trace_.begin()) {
        result.push_back('\n');
      }
      result.append(it->to_string());
    }
    return result;
  }

 private:
  std::string message_;
  std::vector<UTraceContext> trace_ {};
};

class UTraceException : public std::runtime_error {
 public:
  explicit UTraceException(
    UTrace utrace, std::source_location loc = std::source_location::current())
      : std::runtime_error(make_message(utrace, loc)),
        utrace_(std::move(utrace)) {}

  const UTrace &utrace() const noexcept { return utrace_; }

 private:
  UTrace utrace_;

  static std::string make_message(UTrace &utrace, std::source_location loc) {
    utrace.add_location(loc);
    return utrace.join_all();
  }
};
} // namespace sirius

#define utrace_return(ret) \
  do { \
    ret.error().add_location(); \
    return std::unexpected(std::move(ret.error())); \
  } while (0)

#define utrace_transform_error(e) \
  do { \
    e.add_location(); \
    return std::move(e); \
  } while (0)

#define utrace_transform_error_default() \
  transform_error([](::sirius::UTrace &&e) { utrace_transform_error(e); })
