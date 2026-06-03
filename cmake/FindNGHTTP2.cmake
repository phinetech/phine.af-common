# FindNGHTTP2.cmake
# Finds or builds nghttp2 installation using FetchContent

include(FetchContent)

# Option to use system nghttp2 or build from source
option(USE_SYSTEM_NGHTTP2 "Use system installed nghttp2" OFF)

if(USE_SYSTEM_NGHTTP2)
    # Try to find system-installed nghttp2
    find_package(PkgConfig)
    
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(NGHTTP2 libnghttp2)
        pkg_check_modules(NGHTTP2_ASIO libnghttp2_asio)
    endif()

    # pkg-config populates NGHTTP2_LIBRARIES with bare names (e.g. "nghttp2"),
    # not absolute paths. Using a bare name as IMPORTED_LOCATION below makes
    # CMake emit it as a Make dependency, producing errors like
    # "No rule to make target 'nghttp2'". Resolve a full path here.
    if(NGHTTP2_FOUND)
        find_library(NGHTTP2_LIBRARY_FULL
            NAMES nghttp2 libnghttp2
            HINTS ${NGHTTP2_LIBRARY_DIRS} ${NGHTTP2_LIBDIR}
        )
        if(NGHTTP2_LIBRARY_FULL)
            set(NGHTTP2_LIBRARIES ${NGHTTP2_LIBRARY_FULL})
        elseif(NGHTTP2_LINK_LIBRARIES)
            set(NGHTTP2_LIBRARIES ${NGHTTP2_LINK_LIBRARIES})
        endif()
    endif()

    # If pkg-config didn't find it, try manual search
    if(NOT NGHTTP2_FOUND)
        find_library(NGHTTP2_LIBRARY NAMES nghttp2 libnghttp2 REQUIRED)
        find_path(NGHTTP2_INCLUDE_DIR nghttp2/nghttp2.h REQUIRED)
        
        if(NGHTTP2_LIBRARY AND NGHTTP2_INCLUDE_DIR)
            set(NGHTTP2_FOUND TRUE)
            set(NGHTTP2_LIBRARIES ${NGHTTP2_LIBRARY})
            set(NGHTTP2_INCLUDE_DIRS ${NGHTTP2_INCLUDE_DIR})
        endif()
    endif()
    
    # Try to find asio library separately if not found
    if(NOT NGHTTP2_ASIO_FOUND)
        find_library(NGHTTP2_ASIO_LIBRARY NAMES nghttp2_asio libnghttp2_asio)
        if(NGHTTP2_ASIO_LIBRARY)
            set(NGHTTP2_ASIO_FOUND TRUE)
            set(NGHTTP2_ASIO_LIBRARIES ${NGHTTP2_ASIO_LIBRARY})
        endif()
    endif()
    
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(NGHTTP2
        REQUIRED_VARS
            NGHTTP2_LIBRARIES
            NGHTTP2_INCLUDE_DIRS
    )
    
    if(NGHTTP2_FOUND AND NOT TARGET nghttp2::nghttp2)
        add_library(nghttp2::nghttp2 UNKNOWN IMPORTED)
        set_target_properties(nghttp2::nghttp2 PROPERTIES
            IMPORTED_LOCATION ${NGHTTP2_LIBRARIES}
            INTERFACE_INCLUDE_DIRECTORIES ${NGHTTP2_INCLUDE_DIRS}
        )
    endif()
    
    if(NGHTTP2_ASIO_FOUND AND NOT TARGET nghttp2::asio_http2)
        add_library(nghttp2::asio_http2 UNKNOWN IMPORTED)
        set_target_properties(nghttp2::asio_http2 PROPERTIES
            IMPORTED_LOCATION ${NGHTTP2_ASIO_LIBRARIES}
            INTERFACE_INCLUDE_DIRECTORIES ${NGHTTP2_INCLUDE_DIRS}
        )
    endif()
    
    mark_as_advanced(
        NGHTTP2_LIBRARY
        NGHTTP2_ASIO_LIBRARY
        NGHTTP2_INCLUDE_DIR
    )
    
    message(STATUS "Using system nghttp2")
    message(STATUS "nghttp2 version: ${NGHTTP2_VERSION}")
    message(STATUS "nghttp2 include: ${NGHTTP2_INCLUDE_DIRS}")
    message(STATUS "nghttp2 libraries: ${NGHTTP2_LIBRARIES}")
    if(NGHTTP2_ASIO_FOUND)
        message(STATUS "nghttp2_asio libraries: ${NGHTTP2_ASIO_LIBRARIES}")
    endif()
else()
    # Build nghttp2 from source using FetchContent
    message(STATUS "Building nghttp2 from source using FetchContent...")
    
    # Set options for nghttp2 build
    set(ENABLE_LIB_ONLY ON CACHE BOOL "Build only library" FORCE)
    set(ENABLE_STATIC_LIB OFF CACHE BOOL "Build static library" FORCE)
    set(ENABLE_SHARED_LIB ON CACHE BOOL "Build shared library" FORCE)
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
        GIT_REPOSITORY https://github.com/nghttp2/nghttp2.git
        GIT_TAG        v1.65.0
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )
    
    FetchContent_MakeAvailable(nghttp2)
    
    # Create aliases if they don't exist
    if(TARGET nghttp2 AND NOT TARGET nghttp2::nghttp2)
        add_library(nghttp2::nghttp2 ALIAS nghttp2)
    endif()
    
    # Check if asio library was built
    if(TARGET nghttp2_asio AND NOT TARGET nghttp2::asio_http2)
        add_library(nghttp2::asio_http2 ALIAS nghttp2_asio)
    endif()
    
    # Set nghttp2 variables for compatibility
    set(NGHTTP2_FOUND TRUE)
    set(NGHTTP2_INCLUDE_DIRS "${nghttp2_SOURCE_DIR}/lib/includes" "${nghttp2_BINARY_DIR}/lib/includes")
    set(NGHTTP2_LIBRARIES nghttp2::nghttp2)
    
    if(TARGET nghttp2_asio)
        set(NGHTTP2_ASIO_FOUND TRUE)
        set(NGHTTP2_ASIO_LIBRARIES nghttp2::asio_http2)
    endif()
    
    message(STATUS "nghttp2 built from source")
    message(STATUS "nghttp2 include: ${NGHTTP2_INCLUDE_DIRS}")
    message(STATUS "nghttp2 libraries: ${NGHTTP2_LIBRARIES}")
    if(NGHTTP2_ASIO_FOUND)
        message(STATUS "nghttp2_asio libraries: ${NGHTTP2_ASIO_LIBRARIES}")
    endif()
endif()

# Ensure required target exists
if(NOT TARGET nghttp2::nghttp2)
    message(FATAL_ERROR "nghttp2 library target not found!")
endif()

message(STATUS "nghttp2 configuration complete")
