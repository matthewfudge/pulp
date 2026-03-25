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

# Platform-specific rendering backends
if(APPLE)
    target_sources(pulp-canvas PRIVATE
        platform/mac/cg_canvas.mm
    )
    target_link_libraries(pulp-canvas PRIVATE
        "-framework CoreGraphics"
        "-framework CoreText"
        "-framework Foundation"
    )
endif()

target_link_libraries(pulp-canvas PUBLIC pulp::runtime)
