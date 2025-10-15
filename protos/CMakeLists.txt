# CMakeLists.txt for Protocol Buffer Definitions

# Find the proto files
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")

if(NOT PROTO_FILES)
    message(WARNING "No .proto files found in ${CMAKE_CURRENT_SOURCE_DIR}")
    add_library(af_protos INTERFACE)
    return()
endif()

message(STATUS "Found proto files: ${PROTO_FILES}")

# Check if any proto files contain gRPC service definitions
set(HAS_GRPC_SERVICES FALSE)
foreach(PROTO_FILE ${PROTO_FILES})
    file(READ ${PROTO_FILE} PROTO_CONTENT)
    string(FIND "${PROTO_CONTENT}" "service " SERVICE_POS)
    if(NOT SERVICE_POS EQUAL -1)
        set(HAS_GRPC_SERVICES TRUE)
        break()
    endif()
endforeach()

if(HAS_GRPC_SERVICES)
    message(STATUS "Proto files contain gRPC service definitions")
    generate_grpc_cpp(af_protos "${PROTO_FILES}" "${GENERATED_CODE_DIR}")
    
    add_library(af_protos STATIC
        ${af_protos_PROTO_SRCS}
        ${af_protos_PROTO_HDRS}
        ${af_protos_GRPC_SRCS}
        ${af_protos_GRPC_HDRS}
    )
    
    target_link_libraries(af_protos
        PUBLIC
            protobuf::libprotobuf
            gRPC::grpc++
            gRPC::grpc++_reflection
    )
else()
    message(STATUS "Proto files contain only message definitions")
    generate_protobuf_cpp(af_protos "${PROTO_FILES}" "${GENERATED_CODE_DIR}")
    
    add_library(af_protos STATIC
        ${af_protos_SRCS}
        ${af_protos_HDRS}
    )
    
    target_link_libraries(af_protos
        PUBLIC
            protobuf::libprotobuf
    )
endif()

# Use generator expressions for include directories
target_include_directories(af_protos
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${GENERATED_CODE_DIR}>
        $<INSTALL_INTERFACE:include>
        $<INSTALL_INTERFACE:include/af/protos>
)

set_target_properties(af_protos PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 17
)

message(STATUS "Protobuf library af_protos configured")