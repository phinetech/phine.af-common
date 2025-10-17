# FindOpenSSL.cmake
# Finds or builds OpenSSL installation using FetchContent

include(FetchContent)

# Option to use system OpenSSL or build from source
option(USE_SYSTEM_OPENSSL "Use system installed OpenSSL" ON)

if(USE_SYSTEM_OPENSSL)
    # Try to find system-installed OpenSSL using CMake's built-in module
    # Temporarily remove our custom path to use the system FindOpenSSL
    set(_CMAKE_MODULE_PATH_BACKUP ${CMAKE_MODULE_PATH})
    list(REMOVE_ITEM CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
    
    # Find OpenSSL using system module
    find_package(OpenSSL QUIET)
    
    # Restore module path
    set(CMAKE_MODULE_PATH ${_CMAKE_MODULE_PATH_BACKUP})
    
    if(NOT OpenSSL_FOUND)
        # Manual search for OpenSSL
        find_library(OPENSSL_SSL_LIBRARY NAMES ssl libssl REQUIRED)
        find_library(OPENSSL_CRYPTO_LIBRARY NAMES crypto libcrypto REQUIRED)
        find_path(OPENSSL_INCLUDE_DIR openssl/ssl.h 
                  PATHS /usr/include /usr/local/include
                  REQUIRED)
        
        if(OPENSSL_SSL_LIBRARY AND OPENSSL_CRYPTO_LIBRARY AND OPENSSL_INCLUDE_DIR)
            set(OpenSSL_FOUND TRUE)
            set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
            set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
            
            # Try to determine version
            if(EXISTS "${OPENSSL_INCLUDE_DIR}/openssl/opensslv.h")
                file(STRINGS "${OPENSSL_INCLUDE_DIR}/openssl/opensslv.h" 
                     OPENSSL_VERSION_STR 
                     REGEX "^#[ ]*define[ ]+OPENSSL_VERSION_TEXT")
                if(OPENSSL_VERSION_STR MATCHES "OpenSSL ([0-9]+\\.[0-9]+\\.[0-9]+[a-z]?)")
                    set(OPENSSL_VERSION "${CMAKE_MATCH_1}")
                endif()
            endif()
        endif()
    else()
        # Use variables from find_package
        set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
        set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
    endif()
    
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(OpenSSL
        REQUIRED_VARS
            OPENSSL_LIBRARIES
            OPENSSL_INCLUDE_DIRS
        VERSION_VAR OPENSSL_VERSION
    )
    
    # Create imported targets if they don't exist
    if(OpenSSL_FOUND)
        if(NOT TARGET OpenSSL::SSL)
            add_library(OpenSSL::SSL UNKNOWN IMPORTED)
            set_target_properties(OpenSSL::SSL PROPERTIES
                IMPORTED_LOCATION ${OPENSSL_SSL_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIRS}
            )
            if(OPENSSL_CRYPTO_LIBRARY)
                set_target_properties(OpenSSL::SSL PROPERTIES
                    INTERFACE_LINK_LIBRARIES OpenSSL::Crypto
                )
            endif()
        endif()
        
        if(NOT TARGET OpenSSL::Crypto)
            add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
            set_target_properties(OpenSSL::Crypto PROPERTIES
                IMPORTED_LOCATION ${OPENSSL_CRYPTO_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIRS}
            )
        endif()
        
        # Create unified OpenSSL target
        if(NOT TARGET OpenSSL::OpenSSL)
            add_library(OpenSSL::OpenSSL INTERFACE IMPORTED)
            set_target_properties(OpenSSL::OpenSSL PROPERTIES
                INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto"
                INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIRS}
            )
        endif()
    endif()
    
    mark_as_advanced(
        OPENSSL_SSL_LIBRARY
        OPENSSL_CRYPTO_LIBRARY
        OPENSSL_INCLUDE_DIR
    )
    
    message(STATUS "Using system OpenSSL")
    message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")
    message(STATUS "OpenSSL include: ${OPENSSL_INCLUDE_DIRS}")
    message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")
else()
    # Build OpenSSL from source using FetchContent
    message(STATUS "Building OpenSSL from source using FetchContent...")
    message(WARNING "Building OpenSSL from source is complex and time-consuming. Consider using system OpenSSL.")
    
    # Check for required build tools
    find_program(PERL_EXECUTABLE perl REQUIRED)
    find_program(MAKE_EXECUTABLE make REQUIRED)
    
    # Determine platform-specific configuration
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            set(OPENSSL_CONFIGURE_TARGET linux-x86_64)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
            set(OPENSSL_CONFIGURE_TARGET linux-aarch64)
        else()
            set(OPENSSL_CONFIGURE_TARGET linux-generic64)
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            set(OPENSSL_CONFIGURE_TARGET darwin64-x86_64-cc)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(OPENSSL_CONFIGURE_TARGET darwin64-arm64-cc)
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
            set(OPENSSL_CONFIGURE_TARGET VC-WIN64A)
        else()
            set(OPENSSL_CONFIGURE_TARGET VC-WIN32)
        endif()
    else()
        message(FATAL_ERROR "Unsupported platform for building OpenSSL from source")
    endif()
    
    # Set installation directory
    set(OPENSSL_INSTALL_DIR ${CMAKE_BINARY_DIR}/openssl-install)
    
    # Fetch OpenSSL
    FetchContent_Declare(
        openssl
        GIT_REPOSITORY https://github.com/openssl/openssl.git
        GIT_TAG        openssl-3.0.12
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )
    
    FetchContent_GetProperties(openssl)
    if(NOT openssl_POPULATED)
        FetchContent_Populate(openssl)
        
        # Configure and build OpenSSL
        message(STATUS "Configuring OpenSSL...")
        execute_process(
            COMMAND ${PERL_EXECUTABLE} Configure 
                ${OPENSSL_CONFIGURE_TARGET}
                --prefix=${OPENSSL_INSTALL_DIR}
                --openssldir=${OPENSSL_INSTALL_DIR}
                no-tests
                no-docs
            WORKING_DIRECTORY ${openssl_SOURCE_DIR}
            RESULT_VARIABLE OPENSSL_CONFIGURE_RESULT
            OUTPUT_VARIABLE OPENSSL_CONFIGURE_OUTPUT
            ERROR_VARIABLE OPENSSL_CONFIGURE_ERROR
        )
        
        if(NOT OPENSSL_CONFIGURE_RESULT EQUAL 0)
            message(FATAL_ERROR "OpenSSL configuration failed:\n${OPENSSL_CONFIGURE_ERROR}")
        endif()
        
        message(STATUS "Building OpenSSL (this may take several minutes)...")
        execute_process(
            COMMAND ${MAKE_EXECUTABLE} -j${CMAKE_BUILD_PARALLEL_LEVEL}
            WORKING_DIRECTORY ${openssl_SOURCE_DIR}
            RESULT_VARIABLE OPENSSL_BUILD_RESULT
            OUTPUT_QUIET
            ERROR_VARIABLE OPENSSL_BUILD_ERROR
        )
        
        if(NOT OPENSSL_BUILD_RESULT EQUAL 0)
            message(FATAL_ERROR "OpenSSL build failed:\n${OPENSSL_BUILD_ERROR}")
        endif()
        
        message(STATUS "Installing OpenSSL...")
        execute_process(
            COMMAND ${MAKE_EXECUTABLE} install_sw install_ssldirs
            WORKING_DIRECTORY ${openssl_SOURCE_DIR}
            RESULT_VARIABLE OPENSSL_INSTALL_RESULT
            OUTPUT_QUIET
            ERROR_VARIABLE OPENSSL_INSTALL_ERROR
        )
        
        if(NOT OPENSSL_INSTALL_RESULT EQUAL 0)
            message(FATAL_ERROR "OpenSSL installation failed:\n${OPENSSL_INSTALL_ERROR}")
        endif()
    endif()
    
    # Set OpenSSL paths
    set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INSTALL_DIR}/include)
    set(OPENSSL_SSL_LIBRARY ${OPENSSL_INSTALL_DIR}/lib/libssl.a)
    set(OPENSSL_CRYPTO_LIBRARY ${OPENSSL_INSTALL_DIR}/lib/libcrypto.a)
    set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
    
    # Create imported targets
    add_library(OpenSSL::Crypto STATIC IMPORTED)
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION ${OPENSSL_CRYPTO_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIRS}
    )
    
    add_library(OpenSSL::SSL STATIC IMPORTED)
    set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LOCATION ${OPENSSL_SSL_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIRS}
        INTERFACE_LINK_LIBRARIES OpenSSL::Crypto
    )
    
    # Create unified OpenSSL target
    add_library(OpenSSL::OpenSSL INTERFACE IMPORTED)
    set_target_properties(OpenSSL::OpenSSL PROPERTIES
        INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto"
        INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIRS}
    )
    
    # Mark as found
    set(OpenSSL_FOUND TRUE)
    set(OPENSSL_VERSION "3.0.12")
    
    message(STATUS "OpenSSL built from source")
    message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")
    message(STATUS "OpenSSL include: ${OPENSSL_INCLUDE_DIRS}")
    message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")
endif()

# Ensure required targets exist
if(NOT TARGET OpenSSL::SSL)
    message(FATAL_ERROR "OpenSSL::SSL target not found!")
endif()

if(NOT TARGET OpenSSL::Crypto)
    message(FATAL_ERROR "OpenSSL::Crypto target not found!")
endif()

message(STATUS "OpenSSL configuration complete")
