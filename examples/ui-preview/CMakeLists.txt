# UI Preview — standalone app for validating the rendering pipeline.
# Desktop-only: depends on pulp::inspect which needs Dawn/Skia GPU.
if(APPLE AND NOT PULP_IOS)
    add_executable(pulp-ui-preview main.cpp)
    target_link_libraries(pulp-ui-preview PRIVATE
        pulp::view
        pulp::state
        pulp::inspect
    )
endif()
