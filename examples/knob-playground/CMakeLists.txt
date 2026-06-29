# Knob Playground — the Dream Date FX editor as a native Pulp UI.
#
# The visual is DDFX's real captured editor SVG (ddfx-editor.svg via
# --ui-export), rendered pixel-faithfully by DesignFrameView. Interactive
# controls are declared as DesignFrameElements / drawn on the Canvas and wired
# one at a time on top of it. Doubles as the reference target for the
# native-UI-to-web (Emscripten/Canvas2D) port — see
# docs/guides/web-ui-wasm-port.md. A static, no-WASM browser preview of the same
# editor ships alongside as index.html.

pulp_add_plugin(KnobPlayground
    FORMATS           Standalone
    SOURCES           src/view.cpp
    PLUGIN_NAME       "KnobPlayground"
    BUNDLE_ID         "com.ddd.knobplayground"
    MANUFACTURER      "Dream Date Designs"
    VERSION           "0.1.0"
    CATEGORY          Effect
    PLUGIN_CODE       "Knb1"
    MANUFACTURER_CODE "DDDn"
)

# create_view() pulls in the view + canvas stack; src/ headers are local.
if(TARGET KnobPlayground_Core)
    target_include_directories(KnobPlayground_Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(KnobPlayground_Core PUBLIC pulp::view pulp::canvas)
    # The captured editor frame, rendered by DesignFrameView.
    target_compile_definitions(KnobPlayground_Core PRIVATE
        DDFX_SVG="${CMAKE_CURRENT_SOURCE_DIR}/ddfx-editor.svg")
endif()
