add_library(pulp-scene STATIC
    src/bake_preflight.cpp
    src/gltf_loader.cpp
    src/material_key.cpp
    src/render_packet.cpp
    src/renderer3d_characterization.cpp
    src/scene_graph.cpp
    src/scene_stats.cpp
    src/sidecar.cpp
)
add_library(pulp::scene ALIAS pulp-scene)

target_include_directories(pulp-scene
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(pulp-scene
    PUBLIC
        pulp::runtime
    PRIVATE
        fastgltf::fastgltf
)

target_compile_definitions(pulp-scene
    PUBLIC
        PULP_ENABLE_SCENE3D=1
        PULP_HAS_FASTGLTF=1
)

add_executable(pulp-scene3d-inspect
    src/scene3d_inspect.cpp
)

target_link_libraries(pulp-scene3d-inspect
    PRIVATE
        pulp::scene
)

add_executable(pulp-scene3d-bake-preflight
    src/scene3d_bake_preflight.cpp
)

target_link_libraries(pulp-scene3d-bake-preflight
    PRIVATE
        pulp::scene
)

add_executable(pulp-scene3d-sidecar
    src/scene3d_sidecar.cpp
)

target_link_libraries(pulp-scene3d-sidecar
    PRIVATE
        pulp::scene
)
