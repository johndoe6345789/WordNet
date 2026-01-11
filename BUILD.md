# Building WordNet with Conan

WordNet 3.0 uses the Conan package manager for a modern, cross-platform build system.

## Prerequisites

- Python 3.x (for Conan)
- Conan package manager (version 2.0 or later)
- A C compiler (GCC, Clang, MSVC, etc.)

**Note:** CMake, Ninja, and other build tools will be managed automatically by Conan. You do not need to install them separately.

## Quick Start - Recommended Method

### Build with Conan (Primary Method)

```bash
# Install Conan if not already installed
pip install conan

# Detect/create default profile
conan profile detect --force

# Build with Conan (allows system package installation for Tcl/Tk)
conan build . -of build -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True
```

That's it! Conan will automatically:
- Install CMake and Ninja build tools
- Configure the build system
- Compile the project
- Handle system dependencies (like Tcl/Tk for the GUI)

**Note:** The `tools.system.package_manager:mode=install` flag allows Conan to install system dependencies (Tcl/Tk) automatically. Without this flag, you'll need to install Tcl/Tk manually if you want the GUI browser.

### Test the Build

```bash
# Test the executable
# Note: Conan uses cmake_layout() which creates a nested directory structure
# The build output is in: build/build/<BuildType>/
WNHOME=. ./build/build/Release/src/wn test -over

# On Windows, the path would be:
# set WNHOME=.
# .\build\build\Release\src\wn.exe test -over
```

## Alternative: Manual CMake Build (Not Recommended)

If you prefer to manage dependencies manually and not use Conan:

```bash
# Prerequisites: Install CMake, Ninja, and a C compiler manually
# On Ubuntu: sudo apt-get install cmake ninja-build gcc tcl-dev tk-dev
# On macOS: brew install cmake ninja tcl-tk
# On Windows: choco install cmake ninja

# Create build directory
mkdir build
cd build

# Configure with CMake using Ninja generator
cmake -G Ninja ..

# Build
ninja

# Test the executable
WNHOME=.. ./src/wn test -over
```

**⚠️ Note:** The manual CMake approach requires you to install all dependencies yourself. Using Conan is strongly recommended as it handles all dependencies automatically and provides a consistent build experience across platforms.

## Installation

After building with Conan, you can install WordNet:

```bash
cd build/build/Release
cmake --install . --prefix /path/to/install
```

Or to install to the default location (/usr/local/WordNet-3.0):

```bash
cd build/build/Release
sudo cmake --install .
```

## Environment Variables

After installation, set the following environment variables:

- `PATH`: Add `${prefix}/bin` to your PATH
- `WNHOME`: Set to `${prefix}` if not using default installation location

## What Changed?

The old GNU Autotools build system (configure, Makefile.am, etc.) has been replaced with:

- **Conan**: Modern package manager that handles all dependencies and build tools
- **CMakeLists.txt**: Modern CMake build configuration
- **Ninja**: Fast, parallel build system (managed by Conan)

## Why Conan?

- **No manual dependency installation**: Conan automatically installs CMake, Ninja, and other build tools
- **Cross-platform consistency**: Same build commands work on Linux, macOS, and Windows
- **Reproducible builds**: Conan ensures everyone uses the same tool versions
- **No need for apt-get, brew, or choco**: Conan handles system dependencies automatically

## Components Built

- **libWN.a**: Static WordNet library
- **wn**: Command-line WordNet browser
- **wishwn**: Tcl/Tk GUI browser (only if Tcl/Tk is found)
- **wnb**: Browser launcher script

## Notes

- The Tcl/Tk GUI browser (`wishwn`) is optional and will only be built if Tcl/Tk libraries are detected
- Conan will attempt to install Tcl/Tk automatically on Linux and macOS
- Dictionary files are located in the `dict/` directory
- The build system maintains backwards compatibility with the same runtime behavior
