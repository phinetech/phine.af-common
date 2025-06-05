# CMakeLists.txt for the Common Components
#
# This file configures the build for all common components shared across
# the AF microservice. It includes utilities, models, protobuf definitions,
# dependency injection framework, and communication abstraction layer.
cmake_minimum_required(VERSION 3.10)
project(af_common VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Common compilation flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

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

# Set include directories
target_include_directories(af_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)