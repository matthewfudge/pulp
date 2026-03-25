#include <pulp/view/plugin_view_host.hpp>

#ifdef __APPLE__

#include <pulp/canvas/cg_canvas.hpp>
#import <Cocoa/Cocoa.h>

// ── PulpPluginView: NSView subclass for DAW embedding ────────────────────────

@interface PulpPluginView : NSView
@property (nonatomic, assign) pulp::view::View* rootView;
@end

@implementation PulpPluginView

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    NSRect bounds = self.bounds;

    pulp::canvas::CoreGraphicsCanvas canvas(ctx,
        static_cast<float>(bounds.size.width),
        static_cast<float>(bounds.size.height));

    // Clear with theme background
    canvas.set_fill_color(pulp::canvas::Color::rgba(30, 30, 46));
    canvas.fill_rect(0, 0,
        static_cast<float>(bounds.size.width),
        static_cast<float>(bounds.size.height));

    if (self.rootView) {
        self.rootView->set_bounds({0, 0,
            static_cast<float>(bounds.size.width),
            static_cast<float>(bounds.size.height)});
        self.rootView->layout_children();
        self.rootView->paint_all(canvas);
    }
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    [self setNeedsDisplay:YES];
}

@end

// ── MacPluginViewHost ────────────────────────────────────────────────────────

namespace pulp::view {

class MacPluginViewHost : public PluginViewHost {
public:
    MacPluginViewHost(View& root, Size size)
        : root_(root), size_(size) {
        @autoreleasepool {
            NSRect frame = NSMakeRect(0, 0, size.width, size.height);
            view_ = [[PulpPluginView alloc] initWithFrame:frame];
            view_.rootView = &root_;
        }
    }

    ~MacPluginViewHost() override {
        detach();
    }

    NativeViewHandle native_handle() override {
        return (__bridge void*)view_;
    }

    void attach_to_parent(NativeViewHandle parent) override {
        @autoreleasepool {
            NSView* parent_view = (__bridge NSView*)parent;
            if (parent_view && view_) {
                [parent_view addSubview:view_];
                [view_ setNeedsDisplay:YES];
            }
        }
    }

    void detach() override {
        @autoreleasepool {
            if (view_) {
                [view_ removeFromSuperview];
            }
        }
    }

    void repaint() override {
        @autoreleasepool {
            [view_ setNeedsDisplay:YES];
        }
    }

    void set_size(uint32_t width, uint32_t height) override {
        size_ = {width, height};
        @autoreleasepool {
            [view_ setFrameSize:NSMakeSize(width, height)];
            [view_ setNeedsDisplay:YES];
        }
    }

    Size get_size() const override {
        return size_;
    }

private:
    View& root_;
    Size size_;
    PulpPluginView* view_ = nil;
};

std::unique_ptr<PluginViewHost> PluginViewHost::create(View& root, Size size) {
    return std::make_unique<MacPluginViewHost>(root, size);
}

} // namespace pulp::view

#endif // __APPLE__
