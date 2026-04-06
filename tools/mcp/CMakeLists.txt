# pulp-mcp — MCP server for Pulp
add_executable(pulp-mcp pulp_mcp.cpp)
set_target_properties(pulp-mcp PROPERTIES OUTPUT_NAME "pulp-mcp")
target_compile_features(pulp-mcp PRIVATE cxx_std_20)
target_link_libraries(pulp-mcp PRIVATE pulp::tool-audio)
