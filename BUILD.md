# Building WordNet with CMake, Ninja, and Conan

WordNet 3.0 now uses a modern build system based on CMake, Ninja, and Conan.

## Prerequisites

- CMake 3.15 or later
- Ninja build system
- Conan package manager (optional, for Conan-based builds)
- GCC or other C compiler
- Tcl/Tk (optional, for building the GUI browser `wishwn`)

## Quick Start

### Option 1: Build with CMake and Ninja (recommended)

```bash
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

### Option 2: Build with Conan

```bash
# Install Conan if not already installed
pip install conan

# Detect/create default profile
conan profile detect --force

# Build with Conan
conan build . -of build
```

## Installation

After building, you can install WordNet to the default location:

```bash
cd build
ninja install
```

Or install to a custom location:

```bash
cd build
cmake -DCMAKE_INSTALL_PREFIX=/path/to/install ..
ninja install
```

## Environment Variables

After installation, set the following environment variables:

- `PATH`: Add `${prefix}/bin` to your PATH
- `WNHOME`: Set to `${prefix}` if not using default installation location

## What Changed?

The old GNU Autotools build system (configure, Makefile.am, etc.) has been replaced with:

- **CMakeLists.txt**: Modern CMake build configuration
- **conanfile.py**: Conan package manager integration
- **Ninja**: Fast, parallel build system

## Components Built

- **libWN.a**: Static WordNet library
- **wn**: Command-line WordNet browser
- **wishwn**: Tcl/Tk GUI browser (only if Tcl/Tk is found)
- **wnb**: Browser launcher script

## Notes

- The Tcl/Tk GUI browser (`wishwn`) is optional and will only be built if Tcl/Tk libraries are detected
- Dictionary files are still located in the `dict/` directory
- The build system is backwards compatible with the same runtime behavior
