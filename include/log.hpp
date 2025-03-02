#pragma once

#include <format>
#include <iostream>
#include <string>

enum class level : std::uint8_t {
    debug,
    info,
    warn,
    error,
};

// helper function for non-format string cases
template <typename T>
inline void print(level ll, const T& msg) {
    std::string type{};

    switch (ll) {
        case level::debug: type = "\u001b[36mdebug\u001b[0m"; break;
        case level::info: type = "\u001b[32minfo\u001b[0m "; break;
        case level::warn: type = "\u001b[33mwarn\u001b[0m "; break;
        case level::error: type = "\u001b[31merror\u001b[0m"; break;
    }

    auto& out = (ll >= level::warn) ? std::cerr : std::clog;
    out << type << ": " << msg << '\n';
}

// format string version
template <typename... Args>
inline void print(level ll, const std::format_string<Args...> fmt, Args&&... args) {
    std::string formatted = std::format(fmt, std::forward<Args>(args)...);
    print(ll, formatted);
}

#define MAKE_LOG_FUNC(name, level)                                            \
    template <typename T>                                                     \
    inline void name(const T& msg) {                                          \
        print(level, msg);                                                    \
    }                                                                         \
    template <typename... Args>                                               \
    inline void name(const std::format_string<Args...> fmt, Args&&... args) { \
        print(level, fmt, std::forward<Args>(args)...);                       \
    }

MAKE_LOG_FUNC(debug, level::debug)
MAKE_LOG_FUNC(info, level::info)
MAKE_LOG_FUNC(warn, level::warn)
MAKE_LOG_FUNC(error, level::error)

#undef MAKE_LOG_FUNC
