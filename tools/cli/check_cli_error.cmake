if(NOT DEFINED CLI OR NOT DEFINED WORKING_DIR OR NOT DEFINED FLAG OR NOT DEFINED VALUE
   OR NOT DEFINED EXPECTED_EXIT OR NOT DEFINED EXPECTED_REGEX)
    message(FATAL_ERROR "check_cli_error.cmake requires CLI, WORKING_DIR, FLAG, VALUE, EXPECTED_EXIT, and EXPECTED_REGEX")
endif()

execute_process(
    COMMAND "${CLI}" audio excerpt-find --text demo --input missing.wav "${FLAG}" "${VALUE}"
    WORKING_DIRECTORY "${WORKING_DIR}"
    RESULT_VARIABLE cli_result
    OUTPUT_VARIABLE cli_stdout
    ERROR_VARIABLE cli_stderr
)

set(cli_output "${cli_stdout}${cli_stderr}")

if(NOT cli_result EQUAL EXPECTED_EXIT)
    message(FATAL_ERROR "expected exit ${EXPECTED_EXIT}, got ${cli_result}\n${cli_output}")
endif()

if(NOT cli_output MATCHES "${EXPECTED_REGEX}")
    message(FATAL_ERROR "expected output to match '${EXPECTED_REGEX}'\n${cli_output}")
endif()

message("verified CLI failure contract")
