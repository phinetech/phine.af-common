# FindgRPC.cmake
# Finds or builds gRPC installation using FetchContent

include(FetchContent)

# Option to use system gRPC or build from source
option(USE_SYSTEM_GRPC "Use system installed gRPC" OFF)

if(USE_SYSTEM_GRPC)
    # Try to find system-installed gRPC
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)
    find_program(GRPC_PYTHON_PLUGIN grpc_python_plugin)
    
    find_library(GRPC_LIBRARY NAMES grpc REQUIRED)
    find_library(GRPC_GRPC++_LIBRARY NAMES grpc++ REQUIRED)
    find_library(GRPC_GRPC++_REFLECTION_LIBRARY NAMES grpc++_reflection)
    
    find_path(GRPC_INCLUDE_DIR grpc/grpc.h REQUIRED)
    
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(gRPC
        REQUIRED_VARS
            GRPC_LIBRARY
            GRPC_GRPC++_LIBRARY
            GRPC_INCLUDE_DIR
            GRPC_CPP_PLUGIN
    )
    
    if(gRPC_FOUND AND NOT TARGET gRPC::grpc)
        add_library(gRPC::grpc UNKNOWN IMPORTED)
        set_target_properties(gRPC::grpc PROPERTIES
            IMPORTED_LOCATION ${GRPC_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${GRPC_INCLUDE_DIR}
        )
        
        add_library(gRPC::grpc++ UNKNOWN IMPORTED)
        set_target_properties(gRPC::grpc++ PROPERTIES
            IMPORTED_LOCATION ${GRPC_GRPC++_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${GRPC_INCLUDE_DIR}
        )
        
        if(GRPC_GRPC++_REFLECTION_LIBRARY)
            add_library(gRPC::grpc++_reflection UNKNOWN IMPORTED)
            set_target_properties(gRPC::grpc++_reflection PROPERTIES
                IMPORTED_LOCATION ${GRPC_GRPC++_REFLECTION_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES ${GRPC_INCLUDE_DIR}
            )
        endif()
    endif()
    
    # Find Protobuf (installed alongside gRPC)
    find_package(Protobuf CONFIG QUIET)
    if(NOT Protobuf_FOUND)
        find_package(Protobuf REQUIRED)
    endif()
    
    # Create protobuf aliases if they don't exist
    if(Protobuf_FOUND)
        if(NOT TARGET protobuf::libprotobuf)
            if(TARGET protobuf::protobuf)
                add_library(protobuf::libprotobuf ALIAS protobuf::protobuf)
            elseif(DEFINED Protobuf_LIBRARIES)
                add_library(protobuf::libprotobuf UNKNOWN IMPORTED)
                set_target_properties(protobuf::libprotobuf PROPERTIES
                    IMPORTED_LOCATION "${Protobuf_LIBRARIES}"
                    INTERFACE_INCLUDE_DIRECTORIES "${Protobuf_INCLUDE_DIRS}"
                )
            endif()
        endif()
        
        if(NOT TARGET protobuf::libprotobuf-lite)
            if(TARGET protobuf::protobuf-lite)
                add_library(protobuf::libprotobuf-lite ALIAS protobuf::protobuf-lite)
            elseif(DEFINED Protobuf_LITE_LIBRARIES)
                add_library(protobuf::libprotobuf-lite UNKNOWN IMPORTED)
                set_target_properties(protobuf::libprotobuf-lite PROPERTIES
                    IMPORTED_LOCATION "${Protobuf_LITE_LIBRARIES}"
                    INTERFACE_INCLUDE_DIRECTORIES "${Protobuf_INCLUDE_DIRS}"
                )
            endif()
        endif()
        
        if(NOT TARGET protobuf::protoc)
            if(TARGET protobuf::protoc-bin)
                add_executable(protobuf::protoc ALIAS protobuf::protoc-bin)
            endif()
        endif()
        
        # Set Protobuf variables
        if(NOT DEFINED Protobuf_PROTOC_EXECUTABLE)
            find_program(Protobuf_PROTOC_EXECUTABLE protoc REQUIRED)
        endif()
    endif()
    
    # Make GRPC_CPP_PLUGIN available globally
    set(GRPC_CPP_PLUGIN ${GRPC_CPP_PLUGIN} CACHE FILEPATH "Path to grpc_cpp_plugin")
    
    mark_as_advanced(
        GRPC_LIBRARY
        GRPC_GRPC++_LIBRARY
        GRPC_GRPC++_REFLECTION_LIBRARY
        GRPC_INCLUDE_DIR
        GRPC_CPP_PLUGIN
        GRPC_PYTHON_PLUGIN
    )
    
    message(STATUS "Using system gRPC")
    message(STATUS "gRPC C++ plugin: ${GRPC_CPP_PLUGIN}")
    message(STATUS "Protobuf version: ${Protobuf_VERSION}")
    message(STATUS "Protobuf include: ${Protobuf_INCLUDE_DIRS}")
    message(STATUS "Protobuf compiler: ${Protobuf_PROTOC_EXECUTABLE}")
else()
    # Build gRPC from source using FetchContent
    message(STATUS "Building gRPC from source using FetchContent...")
    message(STATUS "Protobuf will be built as part of gRPC")
    
    # Set options for gRPC build
    set(gRPC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(gRPC_BUILD_CSHARP_EXT OFF CACHE BOOL "" FORCE)
    set(gRPC_BUILD_GRPC_CSHARP_PLUGIN OFF CACHE BOOL "" FORCE)
    set(gRPC_BUILD_GRPC_NODE_PLUGIN OFF CACHE BOOL "" FORCE)
    set(gRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN OFF CACHE BOOL "" FORCE)
    set(gRPC_BUILD_GRPC_PHP_PLUGIN OFF CACHE BOOL "" FORCE)
    set(gRPC_BUILD_GRPC_PYTHON_PLUGIN OFF CACHE BOOL "" FORCE)
    set(gRPC_BUILD_GRPC_RUBY_PLUGIN OFF CACHE BOOL "" FORCE)
    set(gRPC_INSTALL OFF CACHE BOOL "" FORCE)
    
    # Use bundled dependencies
    set(gRPC_PROTOBUF_PROVIDER "module" CACHE STRING "" FORCE)
    set(gRPC_SSL_PROVIDER "package" CACHE STRING "" FORCE)
    set(gRPC_ZLIB_PROVIDER "package" CACHE STRING "" FORCE)
    set(gRPC_ABSL_PROVIDER "module" CACHE STRING "" FORCE)
    set(gRPC_RE2_PROVIDER "module" CACHE STRING "" FORCE)
    
    # Fetch gRPC
    FetchContent_Declare(
        grpc
        GIT_REPOSITORY https://github.com/grpc/grpc.git
        GIT_TAG        v1.48.0
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )
    
    FetchContent_MakeAvailable(grpc)
    
    # Create aliases if they don't exist
    if(TARGET grpc++ AND NOT TARGET gRPC::grpc++)
        add_library(gRPC::grpc++ ALIAS grpc++)
    endif()
    
    if(TARGET grpc++_reflection AND NOT TARGET gRPC::grpc++_reflection)
        add_library(gRPC::grpc++_reflection ALIAS grpc++_reflection)
    endif()
    
    if(TARGET grpc AND NOT TARGET gRPC::grpc)
        add_library(gRPC::grpc ALIAS grpc)
    endif()
    
    # Create protobuf aliases (gRPC's bundled protobuf)
    if(TARGET libprotobuf AND NOT TARGET protobuf::libprotobuf)
        add_library(protobuf::libprotobuf ALIAS libprotobuf)
    endif()
    
    if(TARGET libprotobuf-lite AND NOT TARGET protobuf::libprotobuf-lite)
        add_library(protobuf::libprotobuf-lite ALIAS libprotobuf-lite)
    endif()
    
    if(TARGET protoc AND NOT TARGET protobuf::protoc)
        add_executable(protobuf::protoc ALIAS protoc)
    endif()
    
    # Set Protobuf variables for compatibility
    set(Protobuf_FOUND TRUE)
    set(Protobuf_INCLUDE_DIRS "${grpc_SOURCE_DIR}/third_party/protobuf/src")
    set(Protobuf_LIBRARIES protobuf::libprotobuf)
    set(Protobuf_PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc>)
    set(Protobuf_LITE_LIBRARIES protobuf::libprotobuf-lite)
    
    # Set the plugin paths - use generator expression for built targets
    set(GRPC_CPP_PLUGIN $<TARGET_FILE:grpc_cpp_plugin> CACHE STRING "" FORCE)
    set(GRPC_PYTHON_PLUGIN $<TARGET_FILE:grpc_python_plugin> CACHE STRING "" FORCE)
    
    # Mark as found
    set(gRPC_FOUND TRUE)
    set(Protobuf_FOUND TRUE)
    
    message(STATUS "gRPC built from source")
    message(STATUS "Protobuf built from gRPC's bundled version")
    message(STATUS "gRPC C++ plugin: ${GRPC_CPP_PLUGIN}")
    message(STATUS "Protobuf include: ${Protobuf_INCLUDE_DIRS}")
endif()

# Ensure all required targets exist
if(NOT TARGET protobuf::libprotobuf)
    message(FATAL_ERROR "Protobuf library target not found!")
endif()

if(NOT TARGET protobuf::protoc)
    message(FATAL_ERROR "Protobuf compiler target not found!")
endif()

if(NOT TARGET gRPC::grpc++)
    message(FATAL_ERROR "gRPC++ target not found!")
endif()

message(STATUS "gRPC and Protobuf configuration complete")