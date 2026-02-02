# Transport adapters
add_library(af_transport_adapters
    grpc_response_adapter.cpp
    rest_response_adapter.cpp
)

target_include_directories(af_transport_adapters
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(af_transport_adapters
    PUBLIC
        af_response
        af_communication_interface
        nlohmann_json::nlohmann_json
)

install(TARGETS af_transport_adapters
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES
    grpc_response_adapter.h
    rest_response_adapter.h
    DESTINATION include/af/common/communication/adapters
)
