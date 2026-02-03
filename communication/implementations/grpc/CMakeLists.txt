# CMakeLists.txt for gRPC Communication Implementation
#
# This file configures the build for the gRPC implementation of the
# communication service. It depends on the gRPC and protobuf libraries
# and includes the generated protobuf/gRPC code.

# Find required packages - only if not already found by parent
find_package(gRPC QUIET)

# Skip Protobuf find if already configured by parent CMakeLists
if(NOT Protobuf_FOUND AND NOT TARGET protobuf::libprotobuf)
    find_package(Protobuf REQUIRED)
endif()

# Find OpenSSL (required by gRPC) - only if not already found
if(NOT OpenSSL_FOUND)
    find_package(OpenSSL REQUIRED)
endif()

if(NOT gRPC_FOUND)
    message(STATUS "gRPC not found - gRPC communication implementation will not be built")
    add_library(af_grpc_communication INTERFACE)
    target_link_libraries(af_grpc_communication INTERFACE af_communication_interface)
    return()
endif()

message(STATUS "Building gRPC communication implementation")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(af_grpc_communication
    src/grpc_communication_service.cpp
)

target_include_directories(af_grpc_communication
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${GENERATED_CODE_DIR}>
)

target_link_libraries(af_grpc_communication
    PUBLIC
        af_communication_interface
        af_protos
    PRIVATE
        af_cpp_utils
        gRPC::grpc++
        gRPC::grpc++_reflection
        gRPC::grpc
        gRPC::gpr
        OpenSSL::SSL
        OpenSSL::Crypto
)

install(TARGETS af_grpc_communication
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include/af/communication/implementations/grpc
)

message(STATUS "gRPC communication implementation configured")