#include "error.hpp"
#ifdef ENABLE_BACKTRACE
// clang-format off
#include <backward.hpp>
// clang-format on
#endif
#include <exception>
#include <fmt/core.h>

void PrintAssertionMessage(
    char const* file, int line, char const* function_name, char const* message)
{
    if (message == nullptr) {
        fmt::print(stderr, "Assertion failed at {}:{} in {}", file, line, function_name);
    } else {
        fmt::print(stderr, "Assertion failed at {}:{} in FUNC:\"{}\" with MESSAGE:\"{}\"", file,
            line, function_name, message);
    }
}

auto PrintBackTrace() -> void
{
#ifdef ENABLE_BACKTRACE
    using namespace backward;
    StackTrace st;
    st.load_here(8);
    Printer p;
    p.print(st);
#endif
}