build-type := "Debug"

default: debug

# Configure the build with specified build type
configure: 
    cmake -DCMAKE_BUILD_TYPE:STRING={{build-type}} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
        -DCMAKE_CXX_COMPILER:FILEPATH=/opt/homebrew/opt/llvm/bin/clang++ \
        -B ./build \
        -G Ninja

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
	codesign -fs "smhkd-cert" ./build/smhkd
