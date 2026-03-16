build-type := "Debug"

default: debug

# Configure the build with specified build type
configure:
    SDKROOT=$(/usr/bin/xcrun --show-sdk-path); \
    cmake -S . -B ./build \
        -G Ninja \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
        -DCMAKE_CXX_COMPILER="/opt/homebrew/opt/llvm/bin/clang++" \
        -DCMAKE_OSX_SYSROOT="$SDKROOT" \
        -DCMAKE_BUILD_TYPE={{build-type}};

# Build with current configuration
build: configure
    cmake --build ./build -j8;

# Build specifically in debug mode
debug:
    just build-type="Debug" build
    just sign

# Build specifically in release mode
release:
    just build-type="Release" build
    just sign

clean:
    rm -rf ./build;

sign:
    codesign --force -i com.erics118.smhkd -s "smhkd-cert" ./build/smhkd
