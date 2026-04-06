if(APPLE AND NOT PULP_IOS)
    add_executable(pulp-design-debug pulp_design_debug.cpp)
    target_link_libraries(pulp-design-debug PRIVATE pulp::view pulp::state)
endif()
