# CMakeLists.txt for Configuration Management
#
# This file configures the build for the configuration management component,
# which provides a way to load, access, and modify configuration parameters
# throughout the AF microservice application.

# Find required packages
# Find yaml-cpp - use system installation from Debian package
if(NOT TARGET yaml-cpp)
    find_path(YAML_CPP_INCLUDE_DIR yaml-cpp/yaml.h)
    find_library(YAML_CPP_LIBRARY NAMES yaml-cpp)

    if(YAML_CPP_INCLUDE_DIR AND YAML_CPP_LIBRARY)
        # Create imported target for yaml-cpp
        add_library(yaml-cpp UNKNOWN IMPORTED)
        set_target_properties(yaml-cpp PROPERTIES
            IMPORTED_LOCATION "${YAML_CPP_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${YAML_CPP_INCLUDE_DIR}"
        )
        message(STATUS "Found yaml-cpp: ${YAML_CPP_LIBRARY}")
        message(STATUS "yaml-cpp include: ${YAML_CPP_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "yaml-cpp not found. Please install libyaml-cpp-dev")
    endif()
endif()

# Create library
add_library(af_config STATIC
    src/af_config.cpp
    src/af_typed_config.cpp
)

target_include_directories(af_config PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(af_config
    PUBLIC
        af_cpp_utils
        af_models
    spdlog::spdlog
        yaml-cpp
)

# Install
install(TARGETS af_config
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include/af/config
)