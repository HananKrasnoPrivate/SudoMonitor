#!/bin/bash

# --- Usage Function ---
show_usage() {
    echo "Usage: $0 [OPTION]"
    echo ""
    echo "Builds the Sudo Audit System components (C++ plugins, Daemon, and Go UI)."
    echo ""
    echo "Options:"
    echo "  -d, --debug      Build with debug symbols and no optimization (default)."
    echo "  -r, --release    Build with optimizations and no debug symbols."
    echo "  -h, --help       Show this help message and exit."
    echo ""
    echo "Examples:"
    echo "  $0 --release     # Build for production"
    echo "  $0               # Build for development"
    exit 0
}

BUILD_TYPE="Debug"

case "$1" in
    -h|--help)
        show_usage
        ;;
    -r|--release)
        BUILD_TYPE="Release"
        ;;
    -d|--debug)
        BUILD_TYPE="Debug"
        ;;
    "")
        BUILD_TYPE="Debug"
        ;;
    *)
        echo "Error: Unknown option '$1'"
        show_usage
        ;;
esac

BUILD_DIR="build"

if [ ! -f "go/go.mod" ]; then
    echo "Initializing Go module..."
    if [ -d "go" ]; then
        cd go && go mod init sudo-monitor && go mod tidy && cd ..
    else
        echo "Error: 'go' directory not found."
        exit 1
    fi
fi

if [ ! -d "$BUILD_DIR" ]; then
    mkdir "$BUILD_DIR"
fi

cd "$BUILD_DIR" || exit

echo "Configuring CMake for [$BUILD_TYPE]..."
cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" ..

if [ $? -ne 0 ]; then
    echo "❌ CMake configuration failed."
    exit 1
fi

make

if [ $? -eq 0 ]; then
    echo "--------------------------------------------"
    echo "✅ Build complete. Artifacts in ./build/:"
    echo " - plugin.so         (Sudo I/O Plugin)"
    echo " - pam_module.so     (PAM Module)"
    echo " - sudo_daemon       (C++ Collector)"
    echo " - ui_monitor        (Go TUI Monitor)"
    echo "--------------------------------------------"
else
    echo "❌ Build failed."
    exit 1
fi