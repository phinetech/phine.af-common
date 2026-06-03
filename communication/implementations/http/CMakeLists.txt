# CMakeLists.txt for HTTP/2 Communication Implementation
#
# This file configures the build for the HTTP/2 implementation of the
# communication service using nghttp2 and Boost.Asio.

# Find required packages
find_package(NGHTTP2 QUIET)

if(NOT NGHTTP2_FOUND AND NOT TARGET nghttp2::nghttp2)
    # Try manual find
    find_library(NGHTTP2_LIBRARY NAMES nghttp2 libnghttp2)
    find_path(NGHTTP2_INCLUDE_DIR nghttp2/nghttp2.h)

    if(NGHTTP2_LIBRARY AND NGHTTP2_INCLUDE_DIR)
        set(NGHTTP2_FOUND TRUE)
        if(NOT TARGET nghttp2::nghttp2)
            add_library(nghttp2::nghttp2 UNKNOWN IMPORTED)
            set_target_properties(nghttp2::nghttp2 PROPERTIES
                IMPORTED_LOCATION ${NGHTTP2_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES ${NGHTTP2_INCLUDE_DIR}
            )
        endif()
    endif()
endif()

if(NOT NGHTTP2_FOUND AND NOT TARGET nghttp2::nghttp2)
    message(STATUS "nghttp2 not found - HTTP/2 communication implementation will not be built")
    add_library(af_http_communication INTERFACE)
    target_link_libraries(af_http_communication INTERFACE af_communication_interface)
    return()
endif()

# Find Boost
find_package(Boost REQUIRED COMPONENTS system thread)

# Find OpenSSL (for future TLS support)
if(NOT OpenSSL_FOUND)
    find_package(OpenSSL QUIET)
endif()

# Find nlohmann_json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    # Try to find it via pkg-config or as a header-only include
    find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
        PATHS /usr/include /usr/local/include
    )
endif()

message(STATUS "Building HTTP/2 communication implementation")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(af_http_communication
    src/http2_client_transport.cpp
    src/http2_server_transport.cpp
    src/http_communication_service.cpp
)

target_include_directories(af_http_communication
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_link_libraries(af_http_communication
    PUBLIC
        af_communication_interface
    PRIVATE
        af_cpp_utils
        Boost::system
        Boost::thread
)

# Link spdlog for diagnostic/trace logging
if(NOT TARGET spdlog::spdlog)
    find_package(spdlog QUIET)
endif()
if(TARGET spdlog::spdlog)
    target_link_libraries(af_http_communication PRIVATE spdlog::spdlog)
endif()

# Link nghttp2
if(TARGET nghttp2::nghttp2)
    target_link_libraries(af_http_communication PRIVATE nghttp2::nghttp2)
elseif(NGHTTP2_LIBRARIES)
    target_link_libraries(af_http_communication PRIVATE ${NGHTTP2_LIBRARIES})
    target_include_directories(af_http_communication PRIVATE ${NGHTTP2_INCLUDE_DIRS})
endif()

# Link OpenSSL if available (for TLS support)
if(OpenSSL_FOUND OR TARGET OpenSSL::SSL)
    target_link_libraries(af_http_communication PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(af_http_communication PRIVATE HTTP_TLS_AVAILABLE=1)
endif()

# Link nlohmann_json
if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(af_http_communication PRIVATE nlohmann_json::nlohmann_json)
elseif(NLOHMANN_JSON_INCLUDE_DIR)
    target_include_directories(af_http_communication PRIVATE ${NLOHMANN_JSON_INCLUDE_DIR})
endif()

# Install targets
install(TARGETS af_http_communication
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include/af/communication/implementations/http
)

message(STATUS "HTTP/2 communication implementation configured")
