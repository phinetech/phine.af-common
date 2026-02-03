# FindBoost.cmake
# Finds or builds Boost installation using FetchContent

# Prevent infinite recursion
if(DEFINED _FIND_BOOST_IN_PROGRESS)
    return()
endif()
set(_FIND_BOOST_IN_PROGRESS TRUE)

include(FetchContent)

# Option to use system Boost or build from source
option(USE_SYSTEM_BOOST "Use system installed Boost" ON)

if(USE_SYSTEM_BOOST)
    # Try to find system-installed Boost using CMake's built-in module
    # Temporarily remove our custom path to use the system FindBoost
    set(_CMAKE_MODULE_PATH_BACKUP ${CMAKE_MODULE_PATH})
    list(FILTER CMAKE_MODULE_PATH EXCLUDE REGEX "common/cmake")

    # Find Boost with required components
    find_package(Boost 1.54.0 QUIET COMPONENTS system thread)

    # Restore module path
    set(CMAKE_MODULE_PATH ${_CMAKE_MODULE_PATH_BACKUP})

    if(NOT Boost_FOUND)
        # Try pkg-config as fallback
        find_package(PkgConfig)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(Boost QUIET boost)
        endif()
    endif()

    # Manual search if still not found
    if(NOT Boost_FOUND)
        find_path(Boost_INCLUDE_DIR
            NAMES boost/version.hpp
            PATHS /usr/include /usr/local/include
        )

        find_library(Boost_SYSTEM_LIBRARY
            NAMES boost_system libboost_system
            PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
        )

        find_library(Boost_THREAD_LIBRARY
            NAMES boost_thread libboost_thread
            PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
        )

        if(Boost_INCLUDE_DIR AND Boost_SYSTEM_LIBRARY AND Boost_THREAD_LIBRARY)
            set(Boost_FOUND TRUE)
            set(Boost_INCLUDE_DIRS ${Boost_INCLUDE_DIR})
            set(Boost_LIBRARIES ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY})

            # Try to extract version
            if(EXISTS "${Boost_INCLUDE_DIR}/boost/version.hpp")
                file(STRINGS "${Boost_INCLUDE_DIR}/boost/version.hpp"
                     BOOST_VERSION_HPP_CONTENTS
                     REGEX "^#define BOOST_VERSION ")
                if(BOOST_VERSION_HPP_CONTENTS MATCHES "BOOST_VERSION ([0-9]+)")
                    math(EXPR Boost_MAJOR_VERSION "${CMAKE_MATCH_1} / 100000")
                    math(EXPR Boost_MINOR_VERSION "${CMAKE_MATCH_1} / 100 % 1000")
                    math(EXPR Boost_SUBMINOR_VERSION "${CMAKE_MATCH_1} % 100")
                    set(Boost_VERSION "${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION}")
                endif()
            endif()
        endif()
    endif()

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(Boost
        REQUIRED_VARS
            Boost_INCLUDE_DIRS
            Boost_LIBRARIES
        VERSION_VAR Boost_VERSION
    )

    # Create imported targets if they don't exist
    if(Boost_FOUND)
        if(NOT TARGET Boost::boost)
            add_library(Boost::boost INTERFACE IMPORTED)
            set_target_properties(Boost::boost PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
            )
        endif()

        if(Boost_SYSTEM_LIBRARY AND NOT TARGET Boost::system)
            add_library(Boost::system UNKNOWN IMPORTED)
            set_target_properties(Boost::system PROPERTIES
                IMPORTED_LOCATION "${Boost_SYSTEM_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
            )
        endif()

        if(Boost_THREAD_LIBRARY AND NOT TARGET Boost::thread)
            add_library(Boost::thread UNKNOWN IMPORTED)
            set_target_properties(Boost::thread PROPERTIES
                IMPORTED_LOCATION "${Boost_THREAD_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
            )
            # Thread library typically depends on system library
            if(TARGET Boost::system)
                set_target_properties(Boost::thread PROPERTIES
                    INTERFACE_LINK_LIBRARIES "Boost::system;pthread"
                )
            endif()
        endif()
    endif()

    mark_as_advanced(
        Boost_INCLUDE_DIR
        Boost_SYSTEM_LIBRARY
        Boost_THREAD_LIBRARY
    )

    message(STATUS "Using system Boost")
    message(STATUS "Boost version: ${Boost_VERSION}")
    message(STATUS "Boost include: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
else()
    # Build Boost from source using FetchContent
    message(STATUS "Building Boost from source using FetchContent...")
    message(WARNING "Building Boost from source is very time-consuming. Consider using system Boost.")

    # Set Boost version
    set(BOOST_VERSION "1.83.0")
    string(REPLACE "." "_" BOOST_VERSION_UNDERSCORE ${BOOST_VERSION})

    # Fetch Boost
    FetchContent_Declare(
        Boost
        URL https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_UNDERSCORE}.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    FetchContent_GetProperties(Boost)
    if(NOT boost_POPULATED)
        FetchContent_Populate(Boost)

        # Find required tools
        find_program(B2_EXECUTABLE NAMES b2 bjam)

        if(NOT B2_EXECUTABLE)
            # Bootstrap Boost.Build
            message(STATUS "Bootstrapping Boost.Build...")
            if(WIN32)
                set(BOOTSTRAP_CMD "${boost_SOURCE_DIR}/bootstrap.bat")
            else()
                set(BOOTSTRAP_CMD "${boost_SOURCE_DIR}/bootstrap.sh")
            endif()

            execute_process(
                COMMAND ${BOOTSTRAP_CMD}
                WORKING_DIRECTORY ${boost_SOURCE_DIR}
                RESULT_VARIABLE BOOTSTRAP_RESULT
                OUTPUT_QUIET
                ERROR_VARIABLE BOOTSTRAP_ERROR
            )

            if(NOT BOOTSTRAP_RESULT EQUAL 0)
                message(FATAL_ERROR "Boost bootstrap failed:\n${BOOTSTRAP_ERROR}")
            endif()

            set(B2_EXECUTABLE "${boost_SOURCE_DIR}/b2")
        endif()

        # Build only required libraries
        message(STATUS "Building Boost libraries (this may take several minutes)...")
        set(BOOST_INSTALL_DIR ${CMAKE_BINARY_DIR}/boost-install)

        execute_process(
            COMMAND ${B2_EXECUTABLE}
                --with-system
                --with-thread
                --prefix=${BOOST_INSTALL_DIR}
                --build-dir=${boost_BINARY_DIR}
                variant=release
                link=shared
                threading=multi
                runtime-link=shared
                -j${CMAKE_BUILD_PARALLEL_LEVEL}
                install
            WORKING_DIRECTORY ${boost_SOURCE_DIR}
            RESULT_VARIABLE B2_RESULT
            OUTPUT_QUIET
            ERROR_VARIABLE B2_ERROR
        )

        if(NOT B2_RESULT EQUAL 0)
            message(FATAL_ERROR "Boost build failed:\n${B2_ERROR}")
        endif()
    endif()

    # Set Boost paths
    set(Boost_INCLUDE_DIRS ${BOOST_INSTALL_DIR}/include)
    set(Boost_LIBRARY_DIRS ${BOOST_INSTALL_DIR}/lib)

    # Find built libraries
    find_library(Boost_SYSTEM_LIBRARY
        NAMES boost_system libboost_system
        PATHS ${Boost_LIBRARY_DIRS}
        NO_DEFAULT_PATH
    )

    find_library(Boost_THREAD_LIBRARY
        NAMES boost_thread libboost_thread
        PATHS ${Boost_LIBRARY_DIRS}
        NO_DEFAULT_PATH
    )

    set(Boost_LIBRARIES ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY})

    # Create imported targets
    add_library(Boost::boost INTERFACE IMPORTED)
    set_target_properties(Boost::boost PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
    )

    add_library(Boost::system SHARED IMPORTED)
    set_target_properties(Boost::system PROPERTIES
        IMPORTED_LOCATION "${Boost_SYSTEM_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
    )

    add_library(Boost::thread SHARED IMPORTED)
    set_target_properties(Boost::thread PROPERTIES
        IMPORTED_LOCATION "${Boost_THREAD_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "Boost::system;pthread"
    )

    # Mark as found
    set(Boost_FOUND TRUE)
    set(Boost_VERSION ${BOOST_VERSION})

    message(STATUS "Boost built from source")
    message(STATUS "Boost version: ${Boost_VERSION}")
    message(STATUS "Boost include: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
endif()

# Ensure required targets exist
if(NOT TARGET Boost::system)
    message(FATAL_ERROR "Boost::system target not found!")
endif()

if(NOT TARGET Boost::thread)
    message(FATAL_ERROR "Boost::thread target not found!")
endif()

message(STATUS "Boost configuration complete")
