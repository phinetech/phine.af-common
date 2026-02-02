# Response building utilities
add_library(af_response
    response_builder.cpp
)

target_include_directories(af_response
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(af_response
    PUBLIC
        nlohmann_json::nlohmann_json
)

install(TARGETS af_response
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES
    response_data.h
    response_builder.h
    DESTINATION include/af/common/response
)
