#include "string_util.hpp"

#import <Foundation/Foundation.h>

// Obj-C implementation of toLower so unicode characters are handled correctly
std::string toLower(std::string_view s) {
    @autoreleasepool {
        NSString* in = [[NSString alloc] initWithBytes:s.data()
                                                length:s.size()
                                              encoding:NSUTF8StringEncoding];
        if (!in) return std::string{s};
        const char* utf8 = in.lowercaseString.UTF8String;
        return utf8 ? std::string{utf8} : std::string{};
    }
}
