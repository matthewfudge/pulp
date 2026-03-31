# embed_js.cmake — Convert JS files to C++ string literal header
# Usage: cmake -DFILE1=a.js -DFILE2=b.js -DFILE3=c.js -DOUTPUT_FILE=out.hpp -P embed_js.cmake

if(NOT OUTPUT_FILE)
    message(FATAL_ERROR "embed_js.cmake requires OUTPUT_FILE")
endif()

# Collect files from numbered args
set(INPUT_FILES "")
foreach(I RANGE 1 10)
    if(DEFINED FILE${I})
        list(APPEND INPUT_FILES "${FILE${I}}")
    endif()
endforeach()

if(NOT INPUT_FILES)
    message(FATAL_ERROR "embed_js.cmake requires at least FILE1")
endif()

set(CONTENT "#pragma once\n// Auto-generated from JS prelude files — do not edit\nnamespace pulp::view::preludes {\n\n")
set(PULP_JS_LITERAL_CHUNK_SIZE 12000)

foreach(FILE ${INPUT_FILES})
    get_filename_component(NAME ${FILE} NAME_WE)
    string(REPLACE "-" "_" VAR_NAME ${NAME})

    file(READ ${FILE} JS_CONTENT)
    string(LENGTH "${JS_CONTENT}" JS_LENGTH)
    string(APPEND CONTENT "static const char* ${VAR_NAME} =\n")

    math(EXPR CHUNK_COUNT "(${JS_LENGTH} + ${PULP_JS_LITERAL_CHUNK_SIZE} - 1) / ${PULP_JS_LITERAL_CHUNK_SIZE}")
    math(EXPR LAST_CHUNK_INDEX "${CHUNK_COUNT} - 1")
    foreach(CHUNK_INDEX RANGE ${LAST_CHUNK_INDEX})
        math(EXPR OFFSET "${CHUNK_INDEX} * ${PULP_JS_LITERAL_CHUNK_SIZE}")
        math(EXPR REMAINING "${JS_LENGTH} - ${OFFSET}")
        set(CHUNK_LENGTH ${PULP_JS_LITERAL_CHUNK_SIZE})
        if(${REMAINING} LESS ${PULP_JS_LITERAL_CHUNK_SIZE})
            set(CHUNK_LENGTH ${REMAINING})
        endif()
        string(SUBSTRING "${JS_CONTENT}" ${OFFSET} ${CHUNK_LENGTH} JS_CHUNK)
        string(APPEND CONTENT "R\"__JS__(\n${JS_CHUNK})__JS__\"\n")
    endforeach()

    string(APPEND CONTENT ";\n\n")
endforeach()

string(APPEND CONTENT "} // namespace pulp::view::preludes\n")

file(WRITE ${OUTPUT_FILE} "${CONTENT}")
