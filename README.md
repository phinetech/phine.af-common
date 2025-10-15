# AF Common Library

A shared library providing common components for the AF (Application Function) microservice, including utilities, models, protobuf definitions, dependency injection framework, and communication abstraction layer.

## Table of Contents

- [Overview](#overview)
- [Project Structure](#project-structure)
- [Build Options](#build-options)
- [Building the Library](#building-the-library)
  - [Standalone Build](#standalone-build)
  - [As a Submodule](#as-a-submodule)
- [Installing the Library](#installing-the-library)
- [Using in Your Project](#using-in-your-project)
- [Dependencies](#dependencies)

## Overview

The AF Common Library provides reusable components for building 5G Application Function services. It includes:

- **cpp_utils**: Header-only C++ utilities
- **models**: Data models and structures
- **protos**: Protocol Buffer definitions and generated code
- **di**: Dependency injection framework
- **communication**: Abstraction layer for gRPC and direct communication
- **config**: Configuration management
- **component**: Base component framework

## Project Structure

```
common/
├── CMakeLists.txt              # Main CMake configuration
├── cmake/
│   ├── FindgRPC.cmake          # Custom gRPC finder (builds or finds system gRPC)
│   ├── GenerateProtos.cmake    # Helper functions for proto code generation
│   └── af_commonConfig.cmake.in # Package config template
├── cpp_utils/                  # C++ utilities (header-only)
│   ├── CMakeLists.txt
│   └── include/
├── models/                     # Data models (header-only)
│   ├── CMakeLists.txt
│   └── include/
├── protos/                     # Protocol Buffer definitions
│   ├── CMakeLists.txt
│   ├── *.proto
│   └── Generated code (in build/generated/)
├── di/                         # Dependency injection (header-only)
│   ├── CMakeLists.txt
│   └── include/
├── communication/              # Communication layer
│   ├── CMakeLists.txt
│   ├── include/
│   └── implementations/
│       ├── direct/
│       └── grpc/
├── config/                     # Configuration management (header-only)
│   ├── CMakeLists.txt
│   └── include/
└── component/                  # Base component framework
    ├── CMakeLists.txt
    ├── include/
    └── src/
```

## Build Options

The library supports several CMake options to control dependency management:

| Option | Default | Description |
|--------|---------|-------------|
| `USE_SYSTEM_GRPC` | `OFF` | Use system-installed gRPC instead of building from source |
| `USE_SYSTEM_PROTOBUF` | `OFF` | Use system-installed Protobuf (only when using system gRPC) |

### Dependency Resolution

- **When `USE_SYSTEM_GRPC=OFF` (default)**:
  - gRPC and Protobuf are automatically downloaded and built using CMake's FetchContent
  - Uses gRPC v1.48.0 with its bundled Protobuf v21.12
  - No system installation required
  - Longer initial build time

- **When `USE_SYSTEM_GRPC=ON`**:
  - Searches for system-installed gRPC and Protobuf
  - Requires gRPC and Protobuf to be pre-installed on the system
  - Faster build time
  - Recommended for Docker builds and CI/CD

## Building the Library

### Standalone Build

```bash
cd common

# Option 1: Build with auto-downloaded gRPC (default, recommended for development)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Option 2: Build with system gRPC (recommended for Docker/production)
cmake -B build -DCMAKE_BUILD_TYPE=Release \
               -DUSE_SYSTEM_GRPC=ON

# Build the library
cmake --build build -j$(nproc)

# Run tests (if available)
cd build && ctest
```

### As a Submodule

When using `af_common` as a Git submodule in your project:

#### 1. Add as Submodule

```bash
cd your-project
git submodule add https://github.com/your-org/af-common.git common
git submodule update --init --recursive
```

#### 2. Update Your CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(your_project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option A: Let af_common manage all dependencies (recommended)
add_subdirectory(common)

# Option B: Use system gRPC for faster Docker builds
set(USE_SYSTEM_GRPC ON CACHE BOOL "Use system gRPC" FORCE)
add_subdirectory(common)

# Your executable/library
add_executable(your_app
    main.cpp
    # ... other sources
)

# Link against af_common (automatically includes all dependencies)
target_link_libraries(your_app
    PRIVATE
        af_common
)

# af_common automatically provides:
# - All include directories
# - gRPC and Protobuf libraries
# - Generated proto code
# - All common utilities
```

#### 3. Use in Your Code

```cpp
#include "af/common/utils/logger.h"
#include "af/common/models/session.h"
#include "af/common/communication/communication_factory.h"
#include "message.grpc.pb.h"  // Generated proto code

int main() {
    // Use af_common components
    auto comm = af::common::CommunicationFactory::CreateGrpcCommunication();
    // ...
}
```

## Installing the Library

For system-wide installation (useful for Docker images):

```bash
cd common

# Configure with system dependencies
cmake -B build -DCMAKE_BUILD_TYPE=Release \
               -DUSE_SYSTEM_GRPC=ON \
               -DCMAKE_INSTALL_PREFIX=/usr/local

# Build
cmake --build build -j$(nproc)

# Install (requires sudo for /usr/local)
sudo cmake --install build
```

This installs:
- Libraries to `/usr/local/lib/`
- Headers to `/usr/local/include/`
- CMake config files to `/usr/local/lib/cmake/af_common/`

### Using Installed Library

```cmake
# In your project's CMakeLists.txt
find_package(af_common REQUIRED)

add_executable(your_app main.cpp)

target_link_libraries(your_app
    PRIVATE
        af::af_common
)
```

## Dependencies

### Required Dependencies

The library requires the following dependencies, which are either auto-downloaded or must be installed:

#### Auto-downloaded (when `USE_SYSTEM_GRPC=OFF`):
- gRPC v1.48.0 (includes Protobuf v21.12, Abseil, RE2, etc.)

#### System Dependencies:
- CMake >= 3.14
- C++17 compatible compiler (GCC 7+, Clang 5+)
- OpenSSL (for gRPC TLS support)
- zlib (for compression)
- yaml-cpp (for configuration management)

#### Optional (for testing):
- Google Test (GTest)

### Installing System Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    zlib1g-dev \
    libyaml-cpp-dev
```

**For system gRPC (optional):**
```bash
# Install gRPC and Protobuf from source or package manager
# See https://grpc.io/docs/languages/cpp/quickstart/
```

## Docker Build Example

```dockerfile
# Builder stage
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake git \
    libssl-dev zlib1g-dev libyaml-cpp-dev

# Install gRPC and Protobuf (system-wide)
RUN git clone --recurse-submodules -b v1.58.0 --depth 1 \
    https://github.com/grpc/grpc && \
    cd grpc && mkdir -p cmake/build && cd cmake/build && \
    cmake -DgRPC_INSTALL=ON \
          -DgRPC_BUILD_TESTS=OFF \
          -DCMAKE_INSTALL_PREFIX=/usr/local \
          -DCMAKE_BUILD_TYPE=Release ../.. && \
    make -j$(nproc) && make install && ldconfig

WORKDIR /workspace

# Copy and build af_common
COPY common/ common/
RUN cd common && \
    cmake -B build -DCMAKE_BUILD_TYPE=Release \
                   -DUSE_SYSTEM_GRPC=ON \
                   -DCMAKE_INSTALL_PREFIX=/usr/local && \
    cmake --build build -j$(nproc) && \
    cmake --install build && \
    ldconfig

# Build your application
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j$(nproc)
```

## CMake Architecture

### Key Features

1. **Flexible Dependency Management**:
   - Automatically downloads and builds dependencies using FetchContent
   - Or uses system-installed dependencies for faster Docker builds

2. **Generator Expressions**:
   - Uses `$<BUILD_INTERFACE:...>` and `$<INSTALL_INTERFACE:...>` for proper include path resolution
   - Supports both in-tree builds and installed library usage

3. **Custom Find Modules**:
   - `FindgRPC.cmake`: Intelligently finds or builds gRPC with Protobuf
   - `GenerateProtos.cmake`: Helper functions for proto code generation

4. **Interface Libraries**:
   - Most components are header-only interface libraries
   - Minimal binary footprint
   - No ABI compatibility issues

5. **Proper Target Export**:
   - All targets are properly exported for installation
   - Package config files generated automatically
   - Supports `find_package(af_common)`

## Troubleshooting

### Issue: Proto files not found

**Solution**: Ensure proto files are in `common/protos/` and run CMake configure again.

### Issue: gRPC/Protobuf version mismatch

**Solution**: Use `USE_SYSTEM_GRPC=OFF` to let CMake manage compatible versions, or ensure system gRPC and Protobuf are compatible versions.

### Issue: Long initial build time

**Solution**: 
- Use `USE_SYSTEM_GRPC=ON` with pre-installed gRPC
- Use cache for faster rebuilds
- Use Docker layer caching for CI/CD

### Issue: Cannot find af_common package

**Solution**: Ensure the library is installed or added as a subdirectory. Check `CMAKE_PREFIX_PATH` if installed to a custom location.

## License

[TODO]

## Contributing

[TODO]