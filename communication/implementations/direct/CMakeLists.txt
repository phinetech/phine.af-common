# CMakeLists.txt for Direct Communication Implementation
#
# This file configures the build for the direct function call implementation
# of the communication service. This implementation is used for in-process
# communication between AF components.

# Add position-independent code flag
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Create library
add_library(af_direct_communication SHARED
    src/direct_communication_service.cpp
)

target_include_directories(af_direct_communication PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(af_direct_communication PUBLIC
    af_communication_interface
    af_models
)

# Install
install(TARGETS af_direct_communication
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include/af/communication/implementations/direct
)