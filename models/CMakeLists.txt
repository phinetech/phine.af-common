# CMakeLists.txt for Common Models
#
# This file configures the build for common data models used throughout
# the AF microservice application, such as the Message model.

# Create library for common models
add_library(af_models INTERFACE)

target_include_directories(af_models INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(af_models INTERFACE
    af_cpp_utils
    nlohmann_json::nlohmann_json
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/af/models
    FILES_MATCHING PATTERN "*.h"
)