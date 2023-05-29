#ifndef PIKA_UTILS_HPP
#define PIKA_UTILS_HPP

#include <utility>

template<typename Function>
struct Defer {
    Defer(Function function)
        : m_function(std::move(function))
    {
    }
    ~Defer()
    {
        m_function();
    }
    Function m_function;
};

#endif