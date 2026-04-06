# Native Three.js bridge demo — real three.webgpu.js on Pulp's Dawn-backed bridge

if(PULP_ENABLE_GPU AND PULP_HAS_THREEJS)
    add_executable(pulp-threejs-native-demo main.cpp)
    target_link_libraries(pulp-threejs-native-demo PRIVATE
        pulp::view
        pulp::render
        pulp::state
    )
    target_compile_definitions(pulp-threejs-native-demo PRIVATE
        PULP_THREEJS_SOURCE_DIR="${threejs_SOURCE_DIR}"
    )

    if(APPLE)
        target_link_options(pulp-threejs-native-demo PRIVATE -Wl,-stack_size,0x2000000)
    elseif(UNIX)
        target_link_options(pulp-threejs-native-demo PRIVATE -Wl,-z,stacksize=16777216)
    endif()
endif()
