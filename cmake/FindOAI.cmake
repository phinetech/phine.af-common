# FindOAI.cmake
# ---------------
# Finds the OAI CN5G Common libraries
#
# This module will look for the OAI CN5G Common libraries and define the following variables:
#
#  OAI_FOUND        - True if OAI libraries are found
#  OAI_LIBRARIES    - List of all OAI libraries
#  OAI_INCLUDE_DIRS - Include directories for OAI headers
#
# This module will also define the following imported targets:
#
#  oai::CONFIG       - CONFIG library
#  oai::PCF          - PCF model library
#  oai::COMMON_MODEL - Common model library
#  oai::LOGGER       - Logger library
#  oai::NAS          - NAS library
#  oai::COMMON       - Common utilities library
#  oai::UTILS        - Utilities library

# Try to find the package using config mode first (preferred)
find_package(oai_cn5g_common CONFIG QUIET)

if(oai_cn5g_common_FOUND)
    # Package found via config mode
    set(OAI_FOUND TRUE)
    set(OAI_LIBRARIES ${OAI_CN5G_COMMON_LIBRARIES})
    
    message(STATUS "Found OAI CN5G Common libraries (config mode)")
    message(STATUS "  Version: ${oai_cn5g_common_VERSION}")
    message(STATUS "  Libraries: ${OAI_LIBRARIES}")
    
else()
    message(STATUS "OAI package config not found, using manual search...")
    
    # Fallback to manual search for libraries
    # Headers are installed to /usr/local/include/oai/
    # Look for any common header file to identify the include directory
    find_path(OAI_INCLUDE_DIR
        NAMES config.hpp logger.hpp
        PATHS
            /usr/local/include/oai
            /usr/include/oai
            ${CMAKE_INSTALL_PREFIX}/include/oai
        NO_DEFAULT_PATH
    )
    
    # If not found with NO_DEFAULT_PATH, try broader search
    if(NOT OAI_INCLUDE_DIR)
        # Find the include directory
        # With structured installation, headers are under /usr/local/include/oai/model/, /usr/local/include/oai/logger/, etc.
        # So we need /usr/local/include as the include path
        find_path(OAI_INCLUDE_DIR
            NAMES oai/config/config.hpp oai/logger/logger.hpp
            PATHS
                /usr/local/include
                /usr/include
                ${CMAKE_INSTALL_PREFIX}/include
            NO_DEFAULT_PATH
        )
    endif()
    
    # Debug output
    if(NOT OAI_INCLUDE_DIR)
        message(STATUS "OAI include directory not found. Searched for oai/config/config.hpp in:")
        message(STATUS "  - /usr/local/include")
        message(STATUS "  - /usr/include")
        message(STATUS "  - ${CMAKE_INSTALL_PREFIX}/include")
    endif()
    
    find_library(OAI_CONFIG_LIBRARY
        NAMES CONFIG libCONFIG
        PATHS
            /usr/local/lib
            /usr/lib
            ${CMAKE_INSTALL_PREFIX}/lib
    )
    message(STATUS "Searching for OAI libraries...")
    message(STATUS "  CONFIG: ${OAI_CONFIG_LIBRARY}")
    
    find_library(OAI_PCF_LIBRARY
        NAMES PCF libPCF
        PATHS
            /usr/local/lib
            /usr/lib
            ${CMAKE_INSTALL_PREFIX}/lib
    )
    message(STATUS "  PCF: ${OAI_PCF_LIBRARY}")
    
    find_library(OAI_COMMON_MODEL_LIBRARY
        NAMES COMMON_MODEL libCOMMON_MODEL
        PATHS
            /usr/local/lib
            /usr/lib
            ${CMAKE_INSTALL_PREFIX}/lib
    )
    message(STATUS "  COMMON_MODEL: ${OAI_COMMON_MODEL_LIBRARY}")
    
    find_library(OAI_LOGGER_LIBRARY
        NAMES LOGGER libLOGGER
        PATHS
            /usr/local/lib
            /usr/lib
            ${CMAKE_INSTALL_PREFIX}/lib
    )
    message(STATUS "  LOGGER: ${OAI_LOGGER_LIBRARY}")
    
    find_library(OAI_NAS_LIBRARY
        NAMES NAS libNAS
        PATHS
            /usr/local/lib
            /usr/lib
            ${CMAKE_INSTALL_PREFIX}/lib
    )
    message(STATUS "  NAS: ${OAI_NAS_LIBRARY}")
    
    find_library(OAI_COMMON_LIBRARY
        NAMES COMMON libCOMMON
        PATHS
            /usr/local/lib
            /usr/lib
            ${CMAKE_INSTALL_PREFIX}/lib
    )
    message(STATUS "  COMMON: ${OAI_COMMON_LIBRARY}")
    
    find_library(OAI_UTILS_LIBRARY
        NAMES UTILS libUTILS
        PATHS
            /usr/local/lib
            /usr/lib
            ${CMAKE_INSTALL_PREFIX}/lib
    )
    message(STATUS "  UTILS: ${OAI_UTILS_LIBRARY}")
    message(STATUS "  Include dir: ${OAI_INCLUDE_DIR}")
    
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(OAI
        REQUIRED_VARS
            OAI_CONFIG_LIBRARY
            OAI_PCF_LIBRARY
            OAI_COMMON_MODEL_LIBRARY
            OAI_LOGGER_LIBRARY
            OAI_NAS_LIBRARY
            OAI_COMMON_LIBRARY
            OAI_UTILS_LIBRARY
        VERSION_VAR OAI_VERSION
    )
    
    if(OAI_FOUND)
        # Set include directory - if not found explicitly, use standard path
        if(NOT OAI_INCLUDE_DIR)
            set(OAI_INCLUDE_DIR /usr/local/include)
            message(STATUS "Using default OAI include directory: ${OAI_INCLUDE_DIR}")
        endif()
        
        # With structured installation, we need to add all model subdirectories
        # so that cross-directory relative includes work (e.g., pcf headers including common_model headers)
        set(OAI_INCLUDE_DIRS 
            ${OAI_INCLUDE_DIR}
            ${OAI_INCLUDE_DIR}/oai/model/common_model
            ${OAI_INCLUDE_DIR}/oai/model/pcf
            ${OAI_INCLUDE_DIR}/oai/model/smf
            ${OAI_INCLUDE_DIR}/oai/model/nrf
        )
        
        set(OAI_LIBRARIES
            ${OAI_CONFIG_LIBRARY}
            ${OAI_PCF_LIBRARY}
            ${OAI_COMMON_MODEL_LIBRARY}
            ${OAI_LOGGER_LIBRARY}
            ${OAI_NAS_LIBRARY}
            ${OAI_COMMON_LIBRARY}
            ${OAI_UTILS_LIBRARY}
        )
        
        # Create imported targets if they don't exist
        # With structured installation, headers are under /usr/local/include/oai/
        # We set INTERFACE_INCLUDE_DIRECTORIES so transitive dependencies get the path
        if(NOT TARGET oai::CONFIG)
            add_library(oai::CONFIG STATIC IMPORTED)
            set_target_properties(oai::CONFIG PROPERTIES
                IMPORTED_LOCATION "${OAI_CONFIG_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${OAI_INCLUDE_DIRS}"
            )
        endif()
        
        if(NOT TARGET oai::PCF)
            add_library(oai::PCF STATIC IMPORTED)
            set_target_properties(oai::PCF PROPERTIES
                IMPORTED_LOCATION "${OAI_PCF_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${OAI_INCLUDE_DIRS}"
            )
        endif()
        
        if(NOT TARGET oai::COMMON_MODEL)
            add_library(oai::COMMON_MODEL STATIC IMPORTED)
            set_target_properties(oai::COMMON_MODEL PROPERTIES
                IMPORTED_LOCATION "${OAI_COMMON_MODEL_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${OAI_INCLUDE_DIRS}"
            )
        endif()
        
        if(NOT TARGET oai::LOGGER)
            add_library(oai::LOGGER STATIC IMPORTED)
            set_target_properties(oai::LOGGER PROPERTIES
                IMPORTED_LOCATION "${OAI_LOGGER_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${OAI_INCLUDE_DIRS}"
            )
        endif()
        
        if(NOT TARGET oai::NAS)
            add_library(oai::NAS STATIC IMPORTED)
            set_target_properties(oai::NAS PROPERTIES
                IMPORTED_LOCATION "${OAI_NAS_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${OAI_INCLUDE_DIRS}"
            )
        endif()
        
        if(NOT TARGET oai::COMMON)
            add_library(oai::COMMON STATIC IMPORTED)
            set_target_properties(oai::COMMON PROPERTIES
                IMPORTED_LOCATION "${OAI_COMMON_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${OAI_INCLUDE_DIRS}"
            )
        endif()
        
        if(NOT TARGET oai::UTILS)
            add_library(oai::UTILS STATIC IMPORTED)
            set_target_properties(oai::UTILS PROPERTIES
                IMPORTED_LOCATION "${OAI_UTILS_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${OAI_INCLUDE_DIRS}"
            )
        endif()
        
        message(STATUS "Found OAI CN5G Common libraries (manual mode)")
        message(STATUS "  Include dir: ${OAI_INCLUDE_DIRS}")
        message(STATUS "  Libraries: ${OAI_LIBRARIES}")
    endif()
endif()

mark_as_advanced(
    OAI_INCLUDE_DIR
    OAI_CONFIG_LIBRARY
    OAI_PCF_LIBRARY
    OAI_COMMON_MODEL_LIBRARY
    OAI_LOGGER_LIBRARY
    OAI_NAS_LIBRARY
    OAI_COMMON_LIBRARY
    OAI_UTILS_LIBRARY
)
