add_library(pulp-format)
add_library(pulp::format ALIAS pulp-format)

target_include_directories(pulp-format
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_sources(pulp-format PRIVATE
    src/format.cpp
)

file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/src/format.cpp
    "// pulp-format: processor interface and format adapters\n#include <pulp/format/format.hpp>\nnamespace pulp::format {}\n"
)

target_link_libraries(pulp-format PUBLIC pulp::audio pulp::midi pulp::state)
