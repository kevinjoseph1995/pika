#ifndef PIKA_ERROR_HPP
#define PIKA_ERROR_HPP

#include <string>

auto PrintAssertionMessage(char const *file, int line,
                           char const *function_name,
                           char const *message = nullptr) -> void;
auto PrintBackTrace() -> void;

#define PIKA_ASSERT(expr, ...)                                                 \
  do {                                                                         \
    if ((!(expr))) [[unlikely]] {                                              \
      PrintAssertionMessage(__FILE__, __LINE__,                                \
                            __func__ __VA_OPT__(, ) __VA_ARGS__);              \
      PrintBackTrace();                                                        \
      __builtin_trap();                                                        \
    }                                                                          \
  } while (0)

#define TODO(message) PIKA_ASSERT(false, message)

enum class PikaErrorType {
  Unknown,
  SharedBufferError,
  SyncPrimitiveError,
  SharedRingBufferError
};

struct PikaError {
  PikaErrorType error_type = PikaErrorType::Unknown;
  std::string error_message;
};

#endif