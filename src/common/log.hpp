#pragma once

#include <cerrno>
#include <format>
#include <iostream>
#include <print>
#include <string>
#include <system_error>

enum class level : std::uint8_t {
    debug,
    info,
    warn,
    error,
};

inline bool verbose_logging_enabled = false;

inline void set_verbose_logging(bool enabled) {
    verbose_logging_enabled = enabled;
}

[[nodiscard]] inline bool is_verbose_logging_enabled() {
    return verbose_logging_enabled;
}

template <typename T>
void print(const level ll, const T& msg) {
    if (ll == level::debug && !is_verbose_logging_enabled()) return;
    std::string type{};
    switch (ll) {
        case level::debug: type = "\u001b[36mdebug\u001b[0m"; break;
        case level::info: type = "\u001b[32minfo\u001b[0m "; break;
        case level::warn: type = "\u001b[33mwarn\u001b[0m "; break;
        case level::error: type = "\u001b[31merror\u001b[0m"; break;
    }
    std::print(ll >= level::warn ? std::cerr : std::clog, "{}: {}\n", type, msg);
    if (ll == level::error) {
        std::exit(1);
    }
}

template <typename... Args>
void print(const level ll, const std::format_string<Args...> fmt, Args&&... args) {
    print(ll, std::format(fmt, std::forward<Args>(args)...));
}

inline std::string error_reason(int errnum) {
    return std::system_category().message(errnum);
}

inline std::string error_reason(const std::error_code& ec) {
    return ec.message();
}

inline void print_errno(level ll, const std::string& context, int errnum = errno) {
    print(ll, "{}: {}", context, error_reason(errnum));
}

inline void print_error_code(level ll, const std::string& context, const std::error_code& ec) {
    print(ll, "{}: {}", context, error_reason(ec));
}

template <typename T>
void debug(const T& msg) {
    print(level::debug, msg);
}

template <typename... Args>
void debug(const std::format_string<Args...> fmt, Args&&... args) {
    print(level::debug, fmt, std::forward<Args>(args)...);
}

template <typename T>
void info(const T& msg) {
    print(level::info, msg);
}

template <typename... Args>
void info(const std::format_string<Args...> fmt, Args&&... args) {
    print(level::info, fmt, std::forward<Args>(args)...);
}

template <typename T>
void warn(const T& msg) {
    print(level::warn, msg);
}

template <typename... Args>
void warn(const std::format_string<Args...> fmt, Args&&... args) {
    print(level::warn, fmt, std::forward<Args>(args)...);
}

template <typename T>
void warn_errno(const T& msg, int errnum = errno) {
    print_errno(level::warn, std::string{msg}, errnum);
}

template <typename... Args>
void warn_errno(const std::format_string<Args...> fmt, Args&&... args) {
    const int errnum = errno;
    print_errno(level::warn, std::format(fmt, std::forward<Args>(args)...), errnum);
}

inline void warn_error_code(const std::string& context, const std::error_code& ec) {
    print_error_code(level::warn, context, ec);
}

template <typename T>
void error(const T& msg) {
    print(level::error, msg);
}

template <typename... Args>
void error(const std::format_string<Args...> fmt, Args&&... args) {
    print(level::error, fmt, std::forward<Args>(args)...);
}

template <typename T>
[[noreturn]] void error_errno(const T& msg, int errnum = errno) {
    print_errno(level::error, std::string{msg}, errnum);
    std::unreachable();
}

template <typename... Args>
[[noreturn]] void error_errno(const std::format_string<Args...> fmt, Args&&... args) {
    const int errnum = errno;
    print_errno(level::error, std::format(fmt, std::forward<Args>(args)...), errnum);
    std::unreachable();
}

[[noreturn]] inline void error_error_code(const std::string& context, const std::error_code& ec) {
    print_error_code(level::error, context, ec);
    std::unreachable();
}
