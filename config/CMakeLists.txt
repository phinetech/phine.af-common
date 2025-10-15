# CMakeLists.txt for Configuration Management
#
# This file configures the build for the configuration management component,
# which provides a way to load, access, and modify configuration parameters
# throughout the AF microservice application.

# Find required packages
find_package(yaml-cpp REQUIRED)

# Create library
add_library(af_config INTERFACE)

target_include_directories(af_config INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(af_config INTERFACE
    af_cpp_utils
    af_models
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