#include "cf_string.hpp"

#include <cstring>

std::string cfStringToString(CFStringRef cfString) {
    if (!cfString) return "";
    CFIndex length = CFStringGetLength(cfString);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::string result(static_cast<size_t>(maxSize), '\0');
    if (!CFStringGetCString(cfString, result.data(), maxSize, kCFStringEncodingUTF8)) return "";
    result.resize(strlen(result.c_str()));
    return result;
}
