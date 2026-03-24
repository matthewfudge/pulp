# PulpTone — oscillator synth with MIDI input
# Validates: audio output, MIDI handling, note-on/off, polyphony, envelopes

# Headless tests
add_executable(pulp-tone-test test_pulp_tone.cpp)
target_link_libraries(pulp-tone-test PRIVATE pulp::format Catch2::Catch2WithMain)
target_include_directories(pulp-tone-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
catch_discover_tests(pulp-tone-test)

# Standalone application with musical typing
add_executable(PulpTone main.cpp
    ${CMAKE_SOURCE_DIR}/core/format/src/standalone.cpp
)
target_link_libraries(PulpTone PRIVATE pulp::format pulp::audio pulp::midi)
target_include_directories(PulpTone PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
