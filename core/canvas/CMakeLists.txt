add_library(pulp-canvas)
add_library(pulp::canvas ALIAS pulp-canvas)

target_include_directories(pulp-canvas
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_sources(pulp-canvas PRIVATE
    src/recording_canvas.cpp
)

target_link_libraries(pulp-canvas PUBLIC pulp::runtime)
