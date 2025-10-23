# FindNGHTTP2_ASIO.cmake
# Finds or builds nghttp2_asio (C++ ASIO HTTP/2 library) installation using FetchContent

include(FetchContent)

# Option to use system nghttp2_asio or build from source
option(USE_SYSTEM_NGHTTP2_ASIO "Use system installed nghttp2_asio" ON)

if(USE_SYSTEM_NGHTTP2_ASIO)
    # Try to find system-installed nghttp2_asio
    find_package(PkgConfig QUIET)
    
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(NGHTTP2_ASIO libnghttp2_asio)
    endif()
    
    # If pkg-config didn't find it, try manual search
    if(NOT NGHTTP2_ASIO_FOUND)
        find_library(NGHTTP2_ASIO_LIBRARY NAMES nghttp2_asio libnghttp2_asio)
        
        if(NGHTTP2_ASIO_LIBRARY)
            message(STATUS "Found nghttp2_asio library at: ${NGHTTP2_ASIO_LIBRARY}")
            
            # nghttp2_asio uses the same headers as nghttp2
            # Check if NGHTTP2_INCLUDE_DIRS was already set by FindNGHTTP2
            if(DEFINED NGHTTP2_INCLUDE_DIRS AND NGHTTP2_INCLUDE_DIRS)
                message(STATUS "Using NGHTTP2_INCLUDE_DIRS from FindNGHTTP2: ${NGHTTP2_INCLUDE_DIRS}")
                set(NGHTTP2_ASIO_FOUND TRUE)
                set(NGHTTP2_ASIO_LIBRARIES ${NGHTTP2_ASIO_LIBRARY})
                set(NGHTTP2_ASIO_INCLUDE_DIRS ${NGHTTP2_INCLUDE_DIRS})
            else()
                # NGHTTP2_INCLUDE_DIRS not set, search for headers ourselves
                find_path(NGHTTP2_ASIO_INCLUDE_DIR 
                    NAMES nghttp2/nghttp2.h
                    PATHS /usr/include /usr/local/include
                    NO_DEFAULT_PATH
                )
                
                if(NGHTTP2_ASIO_INCLUDE_DIR)
                    set(NGHTTP2_ASIO_FOUND TRUE)
                    set(NGHTTP2_ASIO_LIBRARIES ${NGHTTP2_ASIO_LIBRARY})
                    set(NGHTTP2_ASIO_INCLUDE_DIRS ${NGHTTP2_ASIO_INCLUDE_DIR})
                    message(STATUS "Found nghttp2 headers at: ${NGHTTP2_ASIO_INCLUDE_DIR}")
                else()
                    message(WARNING "Found nghttp2_asio library but no headers")
                endif()
            endif()
        else()
            message(STATUS "nghttp2_asio library not found")
        endif()
    endif()
    
    # Debug: Show what we found
    message(STATUS "DEBUG: NGHTTP2_ASIO_LIBRARIES: ${NGHTTP2_ASIO_LIBRARIES}")
    message(STATUS "DEBUG: NGHTTP2_ASIO_INCLUDE_DIRS: ${NGHTTP2_ASIO_INCLUDE_DIRS}")
    message(STATUS "DEBUG: NGHTTP2_INCLUDE_DIRS (from FindNGHTTP2): ${NGHTTP2_INCLUDE_DIRS}")
    
    # If NGHTTP2_ASIO_INCLUDE_DIRS is not set but we have the library and NGHTTP2 headers, use those
    if(NGHTTP2_ASIO_LIBRARIES AND NOT NGHTTP2_ASIO_INCLUDE_DIRS AND NGHTTP2_INCLUDE_DIRS)
        message(STATUS "Using NGHTTP2_INCLUDE_DIRS for nghttp2_asio: ${NGHTTP2_INCLUDE_DIRS}")
        set(NGHTTP2_ASIO_INCLUDE_DIRS ${NGHTTP2_INCLUDE_DIRS})
        set(NGHTTP2_ASIO_FOUND TRUE)
    endif()
    
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(NGHTTP2_ASIO
        REQUIRED_VARS
            NGHTTP2_ASIO_LIBRARIES
            NGHTTP2_ASIO_INCLUDE_DIRS
    )
    
    if(NGHTTP2_ASIO_FOUND AND NOT TARGET nghttp2::asio_http2)
        add_library(nghttp2::asio_http2 UNKNOWN IMPORTED)
        set_target_properties(nghttp2::asio_http2 PROPERTIES
            IMPORTED_LOCATION ${NGHTTP2_ASIO_LIBRARIES}
            INTERFACE_INCLUDE_DIRECTORIES ${NGHTTP2_ASIO_INCLUDE_DIRS}
        )
    endif()
    
    # Also create server and client targets if available
    find_library(NGHTTP2_ASIO_SERVER_LIBRARY NAMES nghttp2_asio_server libnghttp2_asio_server)
    if(NGHTTP2_ASIO_SERVER_LIBRARY AND NOT TARGET nghttp2::asio_http2_server)
        add_library(nghttp2::asio_http2_server UNKNOWN IMPORTED)
        set_target_properties(nghttp2::asio_http2_server PROPERTIES
            IMPORTED_LOCATION ${NGHTTP2_ASIO_SERVER_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${NGHTTP2_ASIO_INCLUDE_DIRS}
        )
    endif()
    
    find_library(NGHTTP2_ASIO_CLIENT_LIBRARY NAMES nghttp2_asio_client libnghttp2_asio_client)
    if(NGHTTP2_ASIO_CLIENT_LIBRARY AND NOT TARGET nghttp2::asio_http2_client)
        add_library(nghttp2::asio_http2_client UNKNOWN IMPORTED)
        set_target_properties(nghttp2::asio_http2_client PROPERTIES
            IMPORTED_LOCATION ${NGHTTP2_ASIO_CLIENT_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${NGHTTP2_ASIO_INCLUDE_DIRS}
        )
    endif()
    
    mark_as_advanced(
        NGHTTP2_ASIO_LIBRARY
        NGHTTP2_ASIO_SERVER_LIBRARY
        NGHTTP2_ASIO_CLIENT_LIBRARY
        NGHTTP2_ASIO_INCLUDE_DIR
    )
    
    message(STATUS "Using system nghttp2_asio")
    message(STATUS "nghttp2_asio version: ${NGHTTP2_ASIO_VERSION}")
    message(STATUS "nghttp2_asio include: ${NGHTTP2_ASIO_INCLUDE_DIRS}")
    message(STATUS "nghttp2_asio libraries: ${NGHTTP2_ASIO_LIBRARIES}")
else()
    # Build nghttp2 with ASIO support from source using FetchContent
    message(STATUS "Building nghttp2_asio from source using FetchContent...")
    
    # Ensure Boost ASIO is available
    find_package(Boost REQUIRED COMPONENTS system thread)
    
    # Set options for nghttp2 build with ASIO support
    set(ENABLE_LIB_ONLY OFF CACHE BOOL "Build only library" FORCE)
    set(ENABLE_STATIC_LIB OFF CACHE BOOL "Build static library" FORCE)
    set(ENABLE_SHARED_LIB ON CACHE BOOL "Build shared library" FORCE)
    set(ENABLE_ASIO_LIB ON CACHE BOOL "Build ASIO library" FORCE)
    set(ENABLE_APP OFF CACHE BOOL "Build application" FORCE)
    set(ENABLE_HPACK_TOOLS OFF CACHE BOOL "Build HPACK tools" FORCE)
    set(ENABLE_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)
    set(ENABLE_PYTHON_BINDINGS OFF CACHE BOOL "Build Python bindings" FORCE)
    set(ENABLE_FAILMALLOC OFF CACHE BOOL "Build failmalloc" FORCE)
    set(ENABLE_WERROR OFF CACHE BOOL "Turn on compile time warnings" FORCE)
    set(ENABLE_DEBUG OFF CACHE BOOL "Turn on debug output" FORCE)
    set(ENABLE_THREADS ON CACHE BOOL "Turn on threading" FORCE)
    set(ENABLE_DOC OFF CACHE BOOL "Build documentation" FORCE)
    
    # Fetch nghttp2
    FetchContent_Declare(
        nghttp2
        GIT_REPOSITORY https://github.com/nghttp2/nghttp2-asio.git
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )

    FetchContent_MakeAvailable(nghttp2-asio)

    # Create aliases if they don't exist
    if(TARGET nghttp2_asio AND NOT TARGET nghttp2::asio_http2)
        add_library(nghttp2::asio_http2 ALIAS nghttp2_asio)
    endif()
    
    # Check for server and client targets
    if(TARGET nghttp2_asio_server AND NOT TARGET nghttp2::asio_http2_server)
        add_library(nghttp2::asio_http2_server ALIAS nghttp2_asio_server)
    endif()
    
    if(TARGET nghttp2_asio_client AND NOT TARGET nghttp2::asio_http2_client)
        add_library(nghttp2::asio_http2_client ALIAS nghttp2_asio_client)
    endif()
    
    # Set nghttp2_asio variables for compatibility
    set(NGHTTP2_ASIO_FOUND TRUE)
    set(NGHTTP2_ASIO_INCLUDE_DIRS 
        "${nghttp2_SOURCE_DIR}/lib/includes" 
        "${nghttp2_BINARY_DIR}/lib/includes"
        "${nghttp2_SOURCE_DIR}/src/includes"
        "${Boost_INCLUDE_DIRS}"
    )
    set(NGHTTP2_ASIO_LIBRARIES nghttp2::asio_http2)
    
    message(STATUS "nghttp2_asio built from source")
    message(STATUS "nghttp2_asio include: ${NGHTTP2_ASIO_INCLUDE_DIRS}")
    message(STATUS "nghttp2_asio libraries: ${NGHTTP2_ASIO_LIBRARIES}")
    if(TARGET nghttp2::asio_http2_server)
        message(STATUS "nghttp2_asio server target available")
    endif()
    if(TARGET nghttp2::asio_http2_client)
        message(STATUS "nghttp2_asio client target available")
    endif()
endif()

# Ensure required target exists
if(NOT TARGET nghttp2::asio_http2)
    message(FATAL_ERROR "nghttp2_asio library target not found!")
endif()

message(STATUS "nghttp2_asio configuration complete")
