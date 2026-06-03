set(PULP_ELYSIUM_RUIF_BASELINE_GENERATED_DIR
    "${CMAKE_SOURCE_DIR}/planning/artifacts/rust-ui/ruif-1/cpp-baseline/generated")
set(PULP_ELYSIUM_RUIF_BASELINE_GENERATED_CPP
    "${PULP_ELYSIUM_RUIF_BASELINE_GENERATED_DIR}/elysium_imported_ui.cpp")
set(PULP_ELYSIUM_RUIF_BASELINE_FIXTURE
    "${CMAKE_SOURCE_DIR}/planning/fixtures/figma-plugin/real-designs/elysium.pulp.zip")
set(PULP_ELYSIUM_RUIF_BASELINE_ASSET_DIR
    "${CMAKE_BINARY_DIR}/ruif-1-elysium-cpp-baseline")

add_custom_command(
    OUTPUT "${PULP_ELYSIUM_RUIF_BASELINE_ASSET_DIR}/scene.pulp.json"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PULP_ELYSIUM_RUIF_BASELINE_ASSET_DIR}"
    COMMAND ${CMAKE_COMMAND} -E chdir "${PULP_ELYSIUM_RUIF_BASELINE_ASSET_DIR}"
            ${CMAKE_COMMAND} -E tar xzf "${PULP_ELYSIUM_RUIF_BASELINE_FIXTURE}"
    DEPENDS "${PULP_ELYSIUM_RUIF_BASELINE_FIXTURE}"
    COMMENT "Unpacking ELYSIUM RUIF-1 fixture assets")

add_custom_target(pulp-elysium-ruif-assets
    DEPENDS "${PULP_ELYSIUM_RUIF_BASELINE_ASSET_DIR}/scene.pulp.json")

add_executable(pulp-elysium-ruif-cpp-baseline MACOSX_BUNDLE
    main.cpp
    "${PULP_ELYSIUM_RUIF_BASELINE_GENERATED_CPP}")

add_dependencies(pulp-elysium-ruif-cpp-baseline pulp-elysium-ruif-assets)

target_include_directories(pulp-elysium-ruif-cpp-baseline PRIVATE
    "${PULP_ELYSIUM_RUIF_BASELINE_GENERATED_DIR}")

target_link_libraries(pulp-elysium-ruif-cpp-baseline PRIVATE
    pulp::view-core)

target_compile_features(pulp-elysium-ruif-cpp-baseline PRIVATE cxx_std_20)

target_compile_definitions(pulp-elysium-ruif-cpp-baseline PRIVATE
    PULP_ELYSIUM_RUIF_ASSET_DIR="${PULP_ELYSIUM_RUIF_BASELINE_ASSET_DIR}")

set_target_properties(pulp-elysium-ruif-cpp-baseline PROPERTIES
    OUTPUT_NAME "PulpElysiumRuifCppBaseline"
    MACOSX_BUNDLE_BUNDLE_NAME "Pulp ELYSIUM RUIF C++ Baseline"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.pulp.elysium-ruif-cpp-baseline"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0")

set(PULP_ELYSIUM_RUIF_PLUGIN_FORMATS CLAP VST3)

pulp_add_plugin(PulpElysiumRuifCppBaseline
    FORMATS         ${PULP_ELYSIUM_RUIF_PLUGIN_FORMATS}
    SOURCES         elysium_ruif_processor.cpp
                    "${PULP_ELYSIUM_RUIF_BASELINE_GENERATED_CPP}"
    PLUGIN_NAME     "PulpElysiumRuifCppBaseline"
    BUNDLE_ID       "com.pulp.elysium-ruif-cpp-baseline"
    MANUFACTURER    "Pulp"
    VERSION         "0.1.0"
    CATEGORY        Effect
    PLUGIN_CODE     "ERcb"
    MANUFACTURER_CODE "Pulp"
    DESIGN_WIDTH    1000
    DESIGN_HEIGHT   600
    DESIGN_MIN_WIDTH 667
    DESIGN_MIN_HEIGHT 400
    DESIGN_MAX_WIDTH 2000
    DESIGN_MAX_HEIGHT 1200
)

add_dependencies(PulpElysiumRuifCppBaseline_Core pulp-elysium-ruif-assets)

target_include_directories(PulpElysiumRuifCppBaseline_Core PUBLIC
    "${PULP_ELYSIUM_RUIF_BASELINE_GENERATED_DIR}")

target_compile_features(PulpElysiumRuifCppBaseline_Core PUBLIC cxx_std_20)

target_compile_definitions(PulpElysiumRuifCppBaseline_Core PUBLIC
    PULP_ELYSIUM_RUIF_ASSET_DIR="${PULP_ELYSIUM_RUIF_BASELINE_ASSET_DIR}")

if(PULP_HAS_CLAP AND APPLE)
    add_test(
        NAME clap-dlopen-PulpElysiumRuifCppBaseline
        COMMAND bash -c "python3 -c \"import ctypes; ctypes.CDLL('${CMAKE_BINARY_DIR}/CLAP/PulpElysiumRuifCppBaseline.clap/Contents/MacOS/PulpElysiumRuifCppBaseline'); print('OK')\""
    )
    set_tests_properties(clap-dlopen-PulpElysiumRuifCppBaseline PROPERTIES
        LABELS "validation;clap;native-ui;ruif"
        TIMEOUT 10)
endif()

if(PULP_HAS_VST3)
    find_program(PLUGINVAL pluginval)
    if(PLUGINVAL)
        add_test(
            NAME pluginval-PulpElysiumRuifCppBaseline-VST3
            COMMAND ${PLUGINVAL} --validate ${CMAKE_BINARY_DIR}/VST3/PulpElysiumRuifCppBaseline.vst3 --strictness-level 5 --timeout-ms 10000
        )
        set_tests_properties(pluginval-PulpElysiumRuifCppBaseline-VST3 PROPERTIES
            LABELS "validation;vst3;native-ui;ruif"
            TIMEOUT 30
            ENVIRONMENT "PULP_DISABLE_PLUGIN_EDITOR=1;PULP_HEADLESS=1;PULP_TEST_MODE=1")
    endif()
endif()
