add_library(pulp-tool-audio)
add_library(pulp::tool-audio ALIAS pulp-tool-audio)

target_include_directories(pulp-tool-audio
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_sources(pulp-tool-audio PRIVATE
    src/excerpt_service.cpp
    src/model_registry.cpp
    src/model_store.cpp
    src/service.cpp
)

target_link_libraries(pulp-tool-audio
    PUBLIC
        pulp::audio
        pulp::runtime
)

target_include_directories(pulp-tool-audio PRIVATE
    $<BUILD_INTERFACE:${choc_SOURCE_DIR}>
)
