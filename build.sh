#!/bin/bash

# RayBible Build Script
# Automates the build process for quick development

set -e  # Exit on error

echo "🔨 Building RayBible..."

# Check for curl on Linux/macOS
if [[ "$OSTYPE" != "msys" ]] && [[ "$OSTYPE" != "win32" ]]; then
    if ! command -v curl &> /dev/null; then
        echo "⚠️  libcurl is required for HTTP requests"
        echo "Installing libcurl-dev..."
        
        if [[ "$OSTYPE" == "linux-gnu"* ]]; then
            if command -v apt-get &> /dev/null; then
                sudo apt-get install -y libcurl4-openssl-dev
            elif command -v dnf &> /dev/null; then
                sudo dnf install -y libcurl-devel
            elif command -v yum &> /dev/null; then
                sudo yum install -y libcurl-devel
            fi
        elif [[ "$OSTYPE" == "darwin"* ]]; then
            # curl is built-in on macOS, just need headers
            echo "curl is available on macOS"
        fi
    fi
fi

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    echo "📁 Creating build directory..."
    mkdir build
fi

cd build

# Configure with CMake
echo "⚙️  Configuring with CMake..."
cmake ..

# Build
echo "🏗️  Compiling..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)

echo ""
echo "✅ Build complete!"
echo ""
echo "To run: cd build && ./raybible"
echo "or:     ./build/raybible"
echo ""