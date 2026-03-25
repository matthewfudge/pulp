add_library(pulp-render)
add_library(pulp::render ALIAS pulp-render)

target_include_directories(pulp-render
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_sources(pulp-render PRIVATE
    src/gpu_surface_dawn.cpp
)

target_link_libraries(pulp-render PUBLIC pulp::runtime)

# Link WebGPU if available
if(PULP_HAS_WEBGPU)
    target_link_libraries(pulp-render PUBLIC webgpu)
    target_compile_definitions(pulp-render PUBLIC PULP_HAS_WEBGPU=1)
endif()
