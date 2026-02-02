# Handler utilities
add_library(af_handlers
    service_handler_helpers.cpp
)

target_include_directories(af_handlers
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(af_handlers
    PUBLIC
        af_response
        af_transport_adapters
        af_communication_interface
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

install(TARGETS af_handlers
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES
    request_context.h
    service_handler_helpers.h
    DESTINATION include/af/common/handlers
)
