build-type := "Debug"

# Configure the build with specified build type
configure: 
    cmake -DCMAKE_BUILD_TYPE:STRING={{build-type}} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
        -DCMAKE_CXX_COMPILER:FILEPATH=/opt/homebrew/opt/llvm/bin/clang++ \
        -DCMAKE_CXX_FLAGS_RELEASE:STRING="-O3" \
        -B ./build \
        -G Ninja

# Build with current configuration
build: configure
    cmake --build ./build -j8;

# Build specifically in debug mode
debug:
    just build-type="Debug" build

# Build specifically in release mode
release:
    just build-type="Release" build

clean:
    rm -rf ./build;

# Default to debug build
default: debug
