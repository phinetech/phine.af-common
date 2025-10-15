# FindProtobuf.cmake
# Finds or builds Protobuf using FetchContent

include(FetchContent)

# Option to use system Protobuf or build from source
option(USE_SYSTEM_PROTOBUF "Use system installed Protobuf" OFF)

if(USE_SYSTEM_PROTOBUF)
    # Try to find system-installed Protobuf
    find_package(Protobuf CONFIG QUIET)
    
    if(NOT Protobuf_FOUND)
        find_package(Protobuf MODULE REQUIRED)
    endif()
    
    if(Protobuf_FOUND)
        message(STATUS "Using system Protobuf ${Protobuf_VERSION}")
        message(STATUS "Protobuf include dir: ${Protobuf_INCLUDE_DIRS}")
        message(STATUS "Protobuf libraries: ${Protobuf_LIBRARIES}")
        message(STATUS "Protobuf compiler: ${Protobuf_PROTOC_EXECUTABLE}")
    endif()
    
else()
    # Build Protobuf from source using FetchContent
    message(STATUS "Building Protobuf from source using FetchContent...")
    
    # Set options for Protobuf build
    set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "" FORCE)
    set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
    set(protobuf_WITH_ZLIB ON CACHE BOOL "" FORCE)
    set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
    set(protobuf_ABSL_PROVIDER "module" CACHE STRING "" FORCE)
    
    # Fetch Protobuf - using v21.12 which has better CMake support
    FetchContent_Declare(
        protobuf
        GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
        GIT_TAG        v21.12
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
        SOURCE_SUBDIR  cmake
    )
    
    FetchContent_MakeAvailable(protobuf)
    
    # Debug: List all available targets
    message(STATUS "Checking for protobuf targets...")
    
    # The targets are created without the :: namespace in older versions
    if(TARGET libprotobuf)
        message(STATUS "Found target: libprotobuf")
        # Create the namespaced alias
        if(NOT TARGET protobuf::libprotobuf)
            add_library(protobuf::libprotobuf ALIAS libprotobuf)
        endif()
    elseif(TARGET protobuf::libprotobuf)
        message(STATUS "Found target: protobuf::libprotobuf")
    else()
        message(FATAL_ERROR "Neither libprotobuf nor protobuf::libprotobuf target was created")
    endif()
    
    if(TARGET libprotobuf-lite)
        if(NOT TARGET protobuf::libprotobuf-lite)
            add_library(protobuf::libprotobuf-lite ALIAS libprotobuf-lite)
        endif()
    endif()
    
    if(TARGET protoc)
        message(STATUS "Found target: protoc")
        if(NOT TARGET protobuf::protoc)
            add_executable(protobuf::protoc ALIAS protoc)
        endif()
    elseif(TARGET protobuf::protoc)
        message(STATUS "Found target: protobuf::protoc")
    else()
        message(FATAL_ERROR "Neither protoc nor protobuf::protoc target was created")
    endif()
    
    # Set standard Protobuf variables for compatibility (removed PARENT_SCOPE)
    set(Protobuf_FOUND TRUE)
    set(Protobuf_VERSION "21.12")
    set(Protobuf_INCLUDE_DIRS "${protobuf_SOURCE_DIR}/src")
    set(Protobuf_LIBRARIES protobuf::libprotobuf)
    set(Protobuf_PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc>)
    set(Protobuf_LITE_LIBRARIES protobuf::libprotobuf-lite)
    
    message(STATUS "Protobuf built from source")
    message(STATUS "Protobuf version: ${Protobuf_VERSION}")
    message(STATUS "Protobuf source dir: ${protobuf_SOURCE_DIR}")
    message(STATUS "Protobuf include dir: ${Protobuf_INCLUDE_DIRS}")
    
endif()

# Ensure all required variables are set
if(NOT TARGET protobuf::libprotobuf)
    message(FATAL_ERROR "Protobuf library target not found!")
endif()

if(NOT TARGET protobuf::protoc)
    message(FATAL_ERROR "Protobuf compiler target not found!")
endif()

message(STATUS "Protobuf configuration complete")