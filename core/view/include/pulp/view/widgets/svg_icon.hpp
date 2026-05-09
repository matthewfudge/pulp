#pragma once

/// @file svg_icon.hpp
/// SVG-icon widget backed by Skia's `SkSVGDOM` — renders an entire inline
/// `<svg>...</svg>` string as one widget, sidestepping the per-child
/// `SvgPathWidget` dom-adapter walk that requires viewBox propagation
/// through module-scope JS state.
///
/// The motivating case is React-bundled icon glyphs (Lucide, Heroicons,
/// Material) where the bundle emits raw `<svg viewBox="..."><path
/// d="..."/></svg>` markup. The existing `SvgPathWidget` handles the
/// common one-`<path>` case only if JS propagates the parent's viewBox
/// to each path child; that propagation edit was triggering the
/// `eval_or_throw` crash during `load_script` on some bundles. This
/// widget accepts the entire outerHTML in a single bridge call — the JS
/// adapter emits one `createSvgIcon(id, parent, outerHTML)` per `<svg>`
/// and never walks the subtree, so the crash surface disappears
/// entirely.
///
/// Rendering uses Skia's `SkSVGDOM` (vendored in
/// `external/skia-build/modules/svg/`) when `PULP_HAS_SKIA` is defined.
/// Without Skia the widget still measures its intrinsic size from the
/// SVG root attributes so Yoga layout works identically in headless /
/// CPU-only tests; paint() is a no-op in that configuration.

#include <memory>
#include <string>

#include <pulp/view/view.hpp>

namespace pulp::canvas { class Canvas; }

namespace pulp::view {

/// Renders an inline `<svg>...</svg>` string as a single View. Suitable
/// for static icons / logos where per-element theming isn't needed. For
/// per-path runtime tinting, prefer the `SvgPathWidget` + bridge path.
///
/// Default state: no SVG content, zero intrinsic size, paint() no-ops.
class SvgIconWidget : public View {
public:
    SvgIconWidget();
    ~SvgIconWidget() override;

    SvgIconWidget(const SvgIconWidget&) = delete;
    SvgIconWidget& operator=(const SvgIconWidget&) = delete;

    /// Replace the widget's SVG content. Pass the whole `<svg>...</svg>`
    /// markup — not just path-data. Re-parses immediately: the SVG root
    /// attributes (width / height / viewBox) are extracted into the
    /// intrinsic size, and when Skia is compiled in, the entire DOM is
    /// parsed into `SkSVGDOM` so paint() can draw it.
    ///
    /// Empty string clears the widget (paint becomes a no-op, intrinsic
    /// size drops to 0). Malformed SVG is tolerated: the widget clears
    /// its rendered content, keeps whatever intrinsic size the XML root
    /// advertised, and paint() no-ops.
    void set_svg(std::string svg_xml);

    /// Return the raw SVG string currently held (whatever was last
    /// passed to `set_svg()`, including the empty default).
    const std::string& svg() const { return svg_xml_; }

    /// True when the last `set_svg()` produced a renderable DOM. False
    /// when the widget is empty, the XML parse failed, or Skia is not
    /// compiled in (`PULP_HAS_SKIA` undefined). Tests use this to assert
    /// parse success without pulling in Skia headers.
    bool has_renderable_dom() const;

    void paint(canvas::Canvas& canvas) override;

    /// Intrinsic content size — extracted from the SVG root
    /// (`width`/`height` in user units first, falling back to the
    /// viewBox dimensions). Returns 0 for relative-unit roots (e.g.
    /// `width="100%"`) where the intrinsic is "whatever the container
    /// is", matching browser CSS2 §10.3.8 behaviour.
    ///
    /// Without this override Yoga collapses the widget to 0×0 because
    /// `View::intrinsic_width` defaults to 0.
    float intrinsic_width() const override { return intrinsic_w_; }
    float intrinsic_height() const override { return intrinsic_h_; }

private:
    struct Impl;

    void reparse();

    std::string svg_xml_;
    float intrinsic_w_ = 0.0f;
    float intrinsic_h_ = 0.0f;
    std::unique_ptr<Impl> impl_;
};

}  // namespace pulp::view
