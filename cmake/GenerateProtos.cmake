# GenerateProtos.cmake
# Helper functions for generating protobuf and gRPC code

function(generate_protobuf_cpp TARGET PROTO_FILES OUTPUT_DIR)
    set(PROTO_SRCS)
    set(PROTO_HDRS)

    # Get protoc executable
    if(TARGET protobuf::protoc)
        set(PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc>)
    else()
        set(PROTOC_EXECUTABLE ${Protobuf_PROTOC_EXECUTABLE})
    endif()

    foreach(PROTO_FILE ${PROTO_FILES})
        get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
        get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)

        set(PROTO_SRC "${OUTPUT_DIR}/${PROTO_NAME}.pb.cc")
        set(PROTO_HDR "${OUTPUT_DIR}/${PROTO_NAME}.pb.h")

        list(APPEND PROTO_SRCS ${PROTO_SRC})
        list(APPEND PROTO_HDRS ${PROTO_HDR})

        add_custom_command(
            OUTPUT ${PROTO_SRC} ${PROTO_HDR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
            COMMAND ${PROTOC_EXECUTABLE}
            ARGS --cpp_out=${OUTPUT_DIR}
                 --proto_path=${PROTO_DIR}
                 ${PROTO_FILE}
            DEPENDS ${PROTO_FILE}
            COMMENT "Generating C++ protobuf files for ${PROTO_NAME}"
            VERBATIM
        )
    endforeach()

    set(${TARGET}_SRCS ${PROTO_SRCS} PARENT_SCOPE)
    set(${TARGET}_HDRS ${PROTO_HDRS} PARENT_SCOPE)
endfunction()

function(generate_grpc_cpp TARGET PROTO_FILES OUTPUT_DIR)
    set(GRPC_SRCS)
    set(GRPC_HDRS)
    set(PROTO_SRCS)
    set(PROTO_HDRS)

    # Get protoc executable
    if(TARGET protobuf::protoc)
        set(PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc>)
    else()
        set(PROTOC_EXECUTABLE ${Protobuf_PROTOC_EXECUTABLE})
    endif()

    # Get grpc_cpp_plugin executable
    if(TARGET grpc_cpp_plugin)
        set(GRPC_CPP_PLUGIN $<TARGET_FILE:grpc_cpp_plugin>)
    elseif(TARGET gRPC::grpc_cpp_plugin)
        # Use find_program since generator expressions for LOCATION don't work reliably
        find_program(GRPC_CPP_PLUGIN_FOUND grpc_cpp_plugin
            PATHS /usr/local/bin /usr/bin
            NO_DEFAULT_PATH)
        if(NOT GRPC_CPP_PLUGIN_FOUND)
            find_program(GRPC_CPP_PLUGIN_FOUND grpc_cpp_plugin)
        endif()
        set(GRPC_CPP_PLUGIN ${GRPC_CPP_PLUGIN_FOUND})
    else()
        # Fallback: search in common paths
        find_program(GRPC_CPP_PLUGIN_FOUND grpc_cpp_plugin
            PATHS /usr/local/bin /usr/bin
            NO_DEFAULT_PATH)
        if(NOT GRPC_CPP_PLUGIN_FOUND)
            find_program(GRPC_CPP_PLUGIN_FOUND grpc_cpp_plugin)
        endif()
        set(GRPC_CPP_PLUGIN ${GRPC_CPP_PLUGIN_FOUND})
    endif()

    if(NOT GRPC_CPP_PLUGIN)
        message(FATAL_ERROR "gRPC C++ plugin (grpc_cpp_plugin) not found!")
    endif()
    message(STATUS "Using gRPC plugin: ${GRPC_CPP_PLUGIN}")

    foreach(PROTO_FILE ${PROTO_FILES})
        get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
        get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)

        set(PROTO_SRC "${OUTPUT_DIR}/${PROTO_NAME}.pb.cc")
        set(PROTO_HDR "${OUTPUT_DIR}/${PROTO_NAME}.pb.h")
        set(GRPC_SRC "${OUTPUT_DIR}/${PROTO_NAME}.grpc.pb.cc")
        set(GRPC_HDR "${OUTPUT_DIR}/${PROTO_NAME}.grpc.pb.h")

        list(APPEND PROTO_SRCS ${PROTO_SRC})
        list(APPEND PROTO_HDRS ${PROTO_HDR})
        list(APPEND GRPC_SRCS ${GRPC_SRC})
        list(APPEND GRPC_HDRS ${GRPC_HDR})

        # Generate protobuf files
        add_custom_command(
            OUTPUT ${PROTO_SRC} ${PROTO_HDR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
            COMMAND ${PROTOC_EXECUTABLE}
            ARGS --cpp_out=${OUTPUT_DIR}
                 --proto_path=${PROTO_DIR}
                 ${PROTO_FILE}
            DEPENDS ${PROTO_FILE}
            COMMENT "Generating C++ protobuf files for ${PROTO_NAME}"
            VERBATIM
        )

        # Generate gRPC files
        add_custom_command(
            OUTPUT ${GRPC_SRC} ${GRPC_HDR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
            COMMAND ${PROTOC_EXECUTABLE}
            ARGS --grpc_out=${OUTPUT_DIR}
                 --cpp_out=${OUTPUT_DIR}
                 --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
                 --proto_path=${PROTO_DIR}
                 ${PROTO_FILE}
            DEPENDS ${PROTO_FILE} ${PROTO_SRC} ${PROTO_HDR}
            COMMENT "Generating gRPC C++ files for ${PROTO_NAME}"
            VERBATIM
        )
    endforeach()

    set(${TARGET}_PROTO_SRCS ${PROTO_SRCS} PARENT_SCOPE)
    set(${TARGET}_PROTO_HDRS ${PROTO_HDRS} PARENT_SCOPE)
    set(${TARGET}_GRPC_SRCS ${GRPC_SRCS} PARENT_SCOPE)
    set(${TARGET}_GRPC_HDRS ${GRPC_HDRS} PARENT_SCOPE)
endfunction()