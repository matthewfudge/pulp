# Node.js bindings for Pulp HeadlessHost via Node-API (napi)

# Check for Node.js
find_program(NODE_EXECUTABLE node)
if(NOT NODE_EXECUTABLE)
    message(STATUS "Pulp: Node.js not found — Node.js bindings disabled")
    return()
endif()

# Get node-addon-api headers via npm
execute_process(
    COMMAND ${NODE_EXECUTABLE} -e "console.log(require('node-addon-api').include)"
    OUTPUT_VARIABLE NAPI_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE NAPI_RESULT
)

if(NOT NAPI_RESULT EQUAL 0)
    message(STATUS "Pulp: node-addon-api not found — run 'npm install node-addon-api' first")
    message(STATUS "Pulp: Node.js bindings disabled")
    return()
endif()

# Get Node.js include directory
execute_process(
    COMMAND ${NODE_EXECUTABLE} -e "console.log(require('path').dirname(require('path').dirname(process.execPath)) + '/include/node')"
    OUTPUT_VARIABLE NODE_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_library(pulp_nodejs SHARED bindings.cpp)
set_target_properties(pulp_nodejs PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    OUTPUT_NAME "pulp"
)

target_include_directories(pulp_nodejs PRIVATE
    ${NAPI_INCLUDE_DIR}
    ${NODE_INCLUDE_DIR}
)

target_link_libraries(pulp_nodejs PRIVATE
    pulp::format
    pulp::audio
    pulp::midi
    pulp::state
    pulp::signal
)

target_compile_definitions(pulp_nodejs PRIVATE
    NAPI_VERSION=8
    NAPI_DISABLE_CPP_EXCEPTIONS
)
