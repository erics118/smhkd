#include "string_util.hpp"

#include <algorithm>

std::string toLower(std::string_view s) {
    std::string out(s.size(), '\0');
    std::ranges::transform(s, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}
