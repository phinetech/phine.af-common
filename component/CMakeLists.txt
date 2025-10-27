# CMakeLists.txt for the base component module
cmake_minimum_required(VERSION 3.10)
project(af_common_component VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source files
set(SOURCES
    src/af_component.cpp
)

# Include directories
include_directories(
    include
)

# Find dependencies
# Find spdlog - use system installation from Debian package
if(NOT TARGET spdlog::spdlog)
    find_path(SPDLOG_INCLUDE_DIR spdlog/spdlog.h)
    find_library(SPDLOG_LIBRARY NAMES spdlog)

    if(SPDLOG_INCLUDE_DIR AND SPDLOG_LIBRARY)
        # Create imported target for spdlog
        add_library(spdlog::spdlog UNKNOWN IMPORTED)
        set_target_properties(spdlog::spdlog PROPERTIES
            IMPORTED_LOCATION "${SPDLOG_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SPDLOG_INCLUDE_DIR}"
        )
        message(STATUS "Found spdlog: ${SPDLOG_LIBRARY}")
        message(STATUS "spdlog include: ${SPDLOG_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "spdlog not found. Please install libspdlog-dev")
    endif()
endif()

# Create library
add_library(af_common_component SHARED ${SOURCES})

# Set library properties
set_target_properties(af_common_component PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Link dependencies
target_link_libraries(af_common_component
    PUBLIC
        spdlog::spdlog
)

# Specify include directories for consumers of this library
target_include_directories(af_common_component
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Install targets
install(TARGETS af_common_component
    EXPORT af_common_component-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY include/
    DESTINATION include/oai/af/common/component
    FILES_MATCHING PATTERN "*.h*"
)

# Install export targets
install(EXPORT af_common_component-targets
    FILE af_common_component-targets.cmake
    NAMESPACE af::
    DESTINATION lib/cmake/af_common_component
)