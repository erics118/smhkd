module;

#include <format>
#include <iostream>
#include <print>
#include <string>

export module smhkd.log;

enum class level : std::uint8_t {
    debug,
    info,
    warn,
    error,
};

template <typename T>
void print(const level ll, const T& msg) {
#ifdef NDEBUG
    if (ll <= level::debug) return;
#endif
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

export template <typename T>
void debug(const T& msg) { print(level::debug, msg); }

export template <typename... Args>
void debug(const std::format_string<Args...> fmt, Args&&... args) {
    print(level::debug, fmt, std::forward<Args>(args)...);
}

export template <typename T>
void info(const T& msg) { print(level::info, msg); }

export template <typename... Args>
void info(const std::format_string<Args...> fmt, Args&&... args) {
    print(level::info, fmt, std::forward<Args>(args)...);
}

export template <typename T>
void warn(const T& msg) { print(level::warn, msg); }

export template <typename... Args>
void warn(const std::format_string<Args...> fmt, Args&&... args) {
    print(level::warn, fmt, std::forward<Args>(args)...);
}

export template <typename T>
void error(const T& msg) { print(level::error, msg); }

export template <typename... Args>
void error(const std::format_string<Args...> fmt, Args&&... args) {
    print(level::error, fmt, std::forward<Args>(args)...);
}
