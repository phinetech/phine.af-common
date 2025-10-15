# CMakeLists.txt for the Common Components
#
# This file configures the build for all common components shared across
# the AF microservice. It includes utilities, models, protobuf definitions,
# dependency injection framework, and communication abstraction layer.
cmake_minimum_required(VERSION 3.14)  # FetchContent requires 3.14+
project(af_common VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add cmake module path for custom Find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Common compilation flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Generated code directory
set(GENERATED_CODE_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_CODE_DIR})

# Find or build gRPC (will also handle Protobuf)
find_package(gRPC REQUIRED)

# Include helper functions for protobuf/gRPC code generation
include(GenerateProtos)

# Add subdirectories for each common component
add_subdirectory(cpp_utils)
add_subdirectory(models)
add_subdirectory(protos)
add_subdirectory(di)
add_subdirectory(communication)
add_subdirectory(config)
add_subdirectory(component)
# add_subdirectory(logging)

# Create a common library target that depends on all common components
add_library(af_common INTERFACE)
target_link_libraries(af_common INTERFACE
    af_cpp_utils
    af_models
    af_protos
    af_di
    af_communication
    af_config
    # af_logging
)

# Use generator expressions for include directories to support both build and install
target_include_directories(af_common INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${GENERATED_CODE_DIR}>
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/af/protos>
)

# Install targets - include all sub-targets needed by af_communication
install(TARGETS 
    af_common
    af_protos
    af_cpp_utils
    af_models
    af_di
    af_communication
    af_communication_interface
    af_communication_factory
    af_direct_communication
    af_grpc_communication
    af_config
    EXPORT af_commonTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Install headers from each component
install(DIRECTORY 
    cpp_utils/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(DIRECTORY 
    models/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(DIRECTORY 
    di/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(DIRECTORY 
    communication/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(DIRECTORY 
    config/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Install generated proto headers
install(DIRECTORY ${GENERATED_CODE_DIR}/
    DESTINATION include/af/protos
    FILES_MATCHING PATTERN "*.h"
)

# Export targets
install(EXPORT af_commonTargets
    FILE af_commonTargets.cmake
    NAMESPACE af::
    DESTINATION lib/cmake/af_common
)

# Create a config file for find_package
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/af_commonConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/af_commonConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/af_commonConfig.cmake"
    INSTALL_DESTINATION lib/cmake/af_common
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/af_commonConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/af_commonConfigVersion.cmake"
    DESTINATION lib/cmake/af_common
)