#include <pulp/view/widgets/svg_icon.hpp>

#include <pulp/canvas/canvas.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

// PIMPL includes: pugixml is always available for root-attribute
// extraction (intrinsic size) because it's a core/runtime dep; Skia is
// gated on PULP_HAS_SKIA. Headless tests run without Skia and still
// verify intrinsic sizing.
#include <pugixml.hpp>

#ifdef PULP_HAS_SKIA
#include <pulp/canvas/skia_canvas.hpp>

#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkSize.h"
#include "include/core/SkStream.h"
#include "modules/svg/include/SkSVGDOM.h"
#include "modules/svg/include/SkSVGRenderContext.h"
#include "modules/svg/include/SkSVGSVG.h"
#endif

namespace pulp::view {

namespace {

// Parse an SVG length attribute ("24", "24px", "100%", "1.5em") into a
// user-unit float. Returns 0 when the value is relative (percentage /
// em) — those resolve against the CSS containing block, which the
// widget does not know at intrinsic-query time. Matches the
// SkSVGLength semantics: fixed-unit strings round-trip, relative units
// report zero intrinsic and let Yoga fall back to preferred sizing.
//
// Why roll this instead of calling SkSVGAttributeParser: the intrinsic
// extraction path must work in PULP_HAS_SKIA=OFF builds too so
// headless / CI tests measure the same size Yoga would. A 20-line
// strtof-based scan keeps the parser pin-small and avoids pulling
// Skia's SVG module into every TU that sees this header.
float parse_svg_length(const char* s) {
    if (!s) return 0.0f;
    while (*s == ' ' || *s == '\t') ++s;
    if (*s == 0) return 0.0f;
    char* end = nullptr;
    float v = std::strtof(s, &end);
    if (end == s) return 0.0f;
    // Fixed units — px / pt / pc / cm / mm / in — all resolve to a
    // finite user-unit size for intrinsic-query purposes. Skia's SVG
    // module applies the 96 DPI conversion at render time; at measure
    // time we treat the leading number as the logical extent. The
    // Lucide / Heroicons / Material icon bundles all use unitless or
    // "px" roots so the leading-number shortcut is always correct.
    while (*end == ' ' || *end == '\t') ++end;
    // Percentage / em / rem / ex are all relative-unit — a
    // container-dependent intrinsic doesn't exist, so report 0.
    if (*end == '%' || (*end == 'e' && *(end + 1) == 'm') ||
        (*end == 'r' && *(end + 1) == 'e' && *(end + 2) == 'm') ||
        *end == 'e') {
        return 0.0f;
    }
    return v;
}

// Extract a viewBox rect ("minX minY width height") into w/h. Returns
// true when parsing succeeded, false otherwise.
bool parse_viewbox(const char* s, float& vw, float& vh) {
    if (!s) return false;
    char* end = nullptr;
    const float mx = std::strtof(s, &end);
    if (end == s) return false;
    const float my = std::strtof(end, &end);
    const float w  = std::strtof(end, &end);
    const float h  = std::strtof(end, &end);
    (void)mx;
    (void)my;
    if (w <= 0 || h <= 0) return false;
    vw = w;
    vh = h;
    return true;
}

// Walk the SVG root attributes to extract the intrinsic (width,
// height) the widget reports to Yoga. Preference order:
//   1. Explicit width="..."/height="..." with absolute units.
//   2. Fall back to viewBox dimensions when width/height are missing
//      or percentage-relative.
//   3. Return {0, 0} when neither is present — Yoga then applies its
//      usual "no basis → 0" default, which the rest of the Pulp
//      ecosystem (layout engine, CSS, prop applier) handles the same
//      way it does for `<img>` without natural size.
//
// Not using `pulp::runtime::XmlDocument` — its API surfaces text
// content via XPath, but root-attribute extraction is simpler with
// pugi's native node API and we already include pugixml directly.
void extract_intrinsic_from_xml(const std::string& xml, float& w, float& h) {
    w = 0.0f;
    h = 0.0f;
    if (xml.empty()) return;

    pugi::xml_document doc;
    const auto result = doc.load_buffer(xml.data(), xml.size());
    if (!result) return;

    auto root = doc.child("svg");
    if (!root) return;

    float ww = 0.0f, hh = 0.0f;
    if (auto a = root.attribute("width")) {
        ww = parse_svg_length(a.value());
    }
    if (auto a = root.attribute("height")) {
        hh = parse_svg_length(a.value());
    }
    if (ww > 0 && hh > 0) {
        w = ww;
        h = hh;
        return;
    }

    if (auto a = root.attribute("viewBox")) {
        float vw = 0.0f, vh = 0.0f;
        if (parse_viewbox(a.value(), vw, vh)) {
            // When only one of width/height is given, prefer the
            // explicit value on that axis and fall back to the viewBox
            // on the other (SVG2 §8.2 intrinsic-size algorithm).
            w = ww > 0 ? ww : vw;
            h = hh > 0 ? hh : vh;
            return;
        }
    }

    // No viewBox and only partial width/height — still report whatever
    // we have. A zero on one axis collapses that axis in Yoga, which
    // matches `<img>`-without-intrinsic behaviour.
    w = ww;
    h = hh;
}

}  // namespace

struct SvgIconWidget::Impl {
#ifdef PULP_HAS_SKIA
    sk_sp<SkSVGDOM> dom;
    float dom_intrinsic_w = 0.0f;
    float dom_intrinsic_h = 0.0f;
#else
    // Empty in non-Skia builds. Keeping the struct non-zero-sized
    // avoids a zero-size struct warning on some toolchains and reserves
    // room for future CPU-only renderers (lunasvg, nanosvg fix-up, …).
    bool parsed = false;
#endif
};

SvgIconWidget::SvgIconWidget() : impl_(std::make_unique<Impl>()) {}
SvgIconWidget::~SvgIconWidget() = default;

void SvgIconWidget::set_svg(std::string svg_xml) {
    svg_xml_ = std::move(svg_xml);
    reparse();
}

bool SvgIconWidget::has_renderable_dom() const {
#ifdef PULP_HAS_SKIA
    return static_cast<bool>(impl_->dom);
#else
    return false;
#endif
}

void SvgIconWidget::reparse() {
    // Intrinsic sizing is Skia-independent so the widget measures
    // consistently in PULP_HAS_SKIA=OFF test builds. The browser-style
    // precedence (width/height > viewBox > none) is applied in
    // extract_intrinsic_from_xml; the result is latched into the
    // member so intrinsic_width/height are O(1) queries.
    extract_intrinsic_from_xml(svg_xml_, intrinsic_w_, intrinsic_h_);

#ifdef PULP_HAS_SKIA
    impl_->dom.reset();
    impl_->dom_intrinsic_w = 0.0f;
    impl_->dom_intrinsic_h = 0.0f;

    if (svg_xml_.empty()) return;

    // SkSVGDOM::MakeFromStream consumes an SkStream, so we wrap the
    // already-owned string bytes without copying. The stream lives only
    // for the duration of the parse call; SkSVGDOM retains its own
    // in-memory representation of the SVG tree.
    auto data = SkData::MakeWithCopy(svg_xml_.data(), svg_xml_.size());
    SkMemoryStream stream(std::move(data));

    // Builder lets us wire a font manager + shaping factory later; the
    // icon case doesn't need text, so the default builder (no font
    // manager) suffices — <text> elements, if present, render blank
    // rather than crashing.
    SkSVGDOM::Builder builder;
    auto dom = builder.make(stream);
    if (!dom) return;

    impl_->dom = std::move(dom);

    // Query the DOM root for its intrinsic size. For SVG roots with
    // absolute units or a viewBox, SkSVGSVG::intrinsicSize reports the
    // same dimensions our XML walk extracted; cache it so the render
    // path can scale viewBox→widget bounds without re-parsing.
    if (auto* root = impl_->dom->getRoot()) {
        // Use a zero-filled length context — sufficient for the
        // absolute-unit case that covers every icon glyph in the wild.
        // Relative-unit SVGs (width="100%") report 0 here, which is
        // what we want (the widget's painted bounds take over).
        SkSize sz = root->intrinsicSize(SkSVGLengthContext(SkSize::Make(0, 0)));
        impl_->dom_intrinsic_w = sz.width();
        impl_->dom_intrinsic_h = sz.height();

        // If the XML walk produced 0 (e.g. unusual attribute casing
        // that pugi missed but Skia parsed), prefer Skia's answer.
        if (intrinsic_w_ <= 0 && impl_->dom_intrinsic_w > 0) {
            intrinsic_w_ = impl_->dom_intrinsic_w;
        }
        if (intrinsic_h_ <= 0 && impl_->dom_intrinsic_h > 0) {
            intrinsic_h_ = impl_->dom_intrinsic_h;
        }
    }
#endif
}

void SvgIconWidget::paint(canvas::Canvas& canvas) {
    const auto b = local_bounds();
    if (b.width <= 0 || b.height <= 0) return;

#ifdef PULP_HAS_SKIA
    if (!impl_->dom) return;

    // Only the Skia-backed Canvas can render SkSVGDOM. A recording or
    // null canvas (in a test) simply skips paint — matches the
    // SvgImage convention in core/canvas/src/svg.cpp.
    auto* skia_canvas = dynamic_cast<canvas::SkiaCanvas*>(&canvas);
    if (!skia_canvas) return;
    SkCanvas* native = skia_canvas->native_canvas();
    if (!native) return;

    // Figure out the source box. Prefer the DOM's reported intrinsic
    // size; fall back to whatever the XML walk found. Zero on either
    // axis means "no intrinsic" — we draw at 1:1 in widget bounds,
    // same as an `<img>` without natural dimensions.
    float src_w = impl_->dom_intrinsic_w > 0 ? impl_->dom_intrinsic_w : intrinsic_w_;
    float src_h = impl_->dom_intrinsic_h > 0 ? impl_->dom_intrinsic_h : intrinsic_h_;
    const bool has_src = src_w > 0 && src_h > 0;

    // Tell the DOM what container to resolve `%`-relative root
    // dimensions against. SkSVGDOM stores this and applies it lazily
    // during render; setting it per-frame is cheap (it's a single
    // field assign on the DOM).
    impl_->dom->setContainerSize(SkSize::Make(b.width, b.height));

    native->save();

    if (has_src) {
        // xMidYMid meet — preserve aspect, centre, scale to fit. The
        // SVG spec treats this as the default preserveAspectRatio when
        // nothing else is set, and `SvgPathWidget::paint` applies the
        // same policy — keep them consistent so an icon that renders
        // identically at 24x24 in both paths also renders identically
        // at non-square bounds.
        const float sx = b.width / src_w;
        const float sy = b.height / src_h;
        const float scale = std::min(sx, sy);
        const float ox = (b.width  - src_w * scale) * 0.5f;
        const float oy = (b.height - src_h * scale) * 0.5f;
        native->translate(ox, oy);
        native->scale(scale, scale);
    }

    impl_->dom->render(native);
    native->restore();
#else
    (void)canvas;
    // Without Skia, SvgIconWidget is a measure-only widget: Yoga sees
    // the right intrinsic size but nothing paints. Mirrors the
    // SvgImage::render situation in CPU-only headless tests.
#endif
}

}  // namespace pulp::view
