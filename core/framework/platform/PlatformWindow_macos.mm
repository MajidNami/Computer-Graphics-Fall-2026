#include "framework/PlatformWindow.h"

#if defined(__APPLE__)

#define GL_SILENCE_DEPRECATION
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>

#include <array>
#include <stdexcept>
#include <string>

struct PlatformWindow::Impl {
    NSWindow* window = nil;
    NSOpenGLView* view = nil;
    bool closeRequested = false;
    std::array<bool, Key::Count> keys{};
    InputState input{};
    bool captured = false;

    Impl(int width, int height, const char* title, int samples) {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSRect frame = NSMakeRect(0, 0, width, height);
        window = [[NSWindow alloc]
            initWithContentRect:frame
                      styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                 NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
                        backing:NSBackingStoreBuffered
                          defer:NO];
        [window setTitle:[NSString stringWithUTF8String:title]];

        // The legacy profile keeps the classic (fixed-function) OpenGL API that
        // the course uses. Students never need a shader language.
        NSOpenGLPixelFormatAttribute withMsaa[] = {
            NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersionLegacy,
            NSOpenGLPFADoubleBuffer, NSOpenGLPFADepthSize, 24, NSOpenGLPFAStencilSize, 8,
            NSOpenGLPFAMultisample, NSOpenGLPFASampleBuffers, 1, NSOpenGLPFASamples,
            static_cast<NSOpenGLPixelFormatAttribute>(samples > 0 ? samples : 4), 0};
        NSOpenGLPixelFormatAttribute basic[] = {
            NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersionLegacy,
            NSOpenGLPFADoubleBuffer, NSOpenGLPFADepthSize, 24, NSOpenGLPFAStencilSize, 8, 0};
        NSOpenGLPixelFormat* format = samples > 0 ? [[NSOpenGLPixelFormat alloc] initWithAttributes:withMsaa] : nil;
        if (!format) format = [[NSOpenGLPixelFormat alloc] initWithAttributes:basic];
        if (!format) throw std::runtime_error("Could not choose a macOS OpenGL pixel format.");

        view = [[NSOpenGLView alloc] initWithFrame:frame pixelFormat:format];
        [view setWantsBestResolutionOpenGLSurface:YES];
        [window setContentView:view];
        [[view openGLContext] makeCurrentContext];
        [window center];
        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        [window setAcceptsMouseMovedEvents:YES];

        updateSize();
    }

    ~Impl() {
        if (captured) capture(false);
        [[view openGLContext] clearDrawable];
        [window close];
    }

    void updateSize() {
        NSRect backing = [view convertRectToBacking:[view bounds]];
        const int w = static_cast<int>(backing.size.width);
        const int h = static_cast<int>(backing.size.height);
        if (w != input.framebufferWidth || h != input.framebufferHeight) {
            input.framebufferWidth = w;
            input.framebufferHeight = h;
            input.resized = true;
            [[view openGLContext] update];
        }
    }

    static int translate(unichar c) {
        if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
        if (c >= 32 && c <= 126) return c;
        if (c >= NSF1FunctionKey && c <= NSF12FunctionKey) return Key::F1 + (c - NSF1FunctionKey);
        switch (c) {
        case NSUpArrowFunctionKey: return Key::Up;
        case NSDownArrowFunctionKey: return Key::Down;
        case NSLeftArrowFunctionKey: return Key::Left;
        case NSRightArrowFunctionKey: return Key::Right;
        case NSPageUpFunctionKey: return Key::PageUp;
        case NSPageDownFunctionKey: return Key::PageDown;
        case NSHomeFunctionKey: return Key::Home;
        case NSEndFunctionKey: return Key::End;
        case 27: return Key::Escape;
        case 9: case 25: return Key::Tab;
        case 13: case 3: return Key::Enter;
        case 127: return Key::Backspace;
        default: return -1;
        }
    }

    void updateKey(NSEvent* event, bool down) {
        NSString* chars = [event charactersIgnoringModifiers];
        if ([chars length] == 0) return;
        const int key = translate([chars characterAtIndex:0]);
        if (key < 0) return;
        if (down && ![event isARepeat]) input.pressed.push_back(key);  // a held key does not fire again
        if (down && [event isARepeat]) return;
        keys[static_cast<size_t>(key)] = down;
    }

    void updateModifiers(NSEventModifierFlags flags) {
        keys[Key::Shift] = (flags & NSEventModifierFlagShift) != 0;
        keys[Key::Control] = (flags & NSEventModifierFlagControl) != 0;
    }

    // While captured the pointer is hidden and does not move: macOS still
    // reports how far the mouse moved in every event's deltaX/deltaY.
    void capture(bool on) {
        if (on == captured) return;
        captured = on;
        if (on) {
            [NSCursor hide];
            CGAssociateMouseAndMouseCursorPosition(false);
        } else {
            CGAssociateMouseAndMouseCursorPosition(true);
            [NSCursor unhide];
        }
    }

    void poll() {
        input.mouseDeltaX = input.mouseDeltaY = input.scrollDelta = 0.0f;
        input.resized = false;
        input.leftClicked = false;
        input.pressed.clear();

        NSEvent* event = nil;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:[NSDate distantPast]
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES])) {
            bool forward = true;
            switch ([event type]) {
            case NSEventTypeKeyDown: updateKey(event, true); forward = false; break;
            case NSEventTypeKeyUp: updateKey(event, false); forward = false; break;
            case NSEventTypeFlagsChanged: updateModifiers([event modifierFlags]); break;
            case NSEventTypeLeftMouseDown: input.leftMouseDown = input.leftClicked = true; break;
            case NSEventTypeLeftMouseUp: input.leftMouseDown = false; break;
            case NSEventTypeRightMouseDown: input.rightMouseDown = true; break;
            case NSEventTypeRightMouseUp: input.rightMouseDown = false; break;
            case NSEventTypeOtherMouseDown: input.middleMouseDown = true; break;
            case NSEventTypeOtherMouseUp: input.middleMouseDown = false; break;
            case NSEventTypeLeftMouseDragged:
            case NSEventTypeRightMouseDragged:
            case NSEventTypeOtherMouseDragged:
            case NSEventTypeMouseMoved: {
                input.mouseDeltaX += static_cast<float>([event deltaX]);
                input.mouseDeltaY += static_cast<float>([event deltaY]);
                // In framebuffer pixels, like the HUD (a Retina screen has two per point).
                const NSPoint p = [view convertPointToBacking:[view convertPoint:[event locationInWindow] fromView:nil]];
                input.mouseX = static_cast<int>(p.x);
                input.mouseY = static_cast<int>(input.framebufferHeight - p.y);
                break;
            }
            case NSEventTypeScrollWheel:
                input.scrollDelta += static_cast<float>([event scrollingDeltaY]) * 0.1f;
                break;
            default: break;
            }
            // Key events are not forwarded so macOS does not beep for keys the
            // window does not "handle" as text input.
            if (forward) [NSApp sendEvent:event];
        }

        if (![window isVisible]) closeRequested = true;
        if (captured && ![window isKeyWindow]) capture(false);
        updateSize();
    }
};

PlatformWindow::PlatformWindow(int w, int h, const char* title, int samples)
    : impl_(std::make_unique<Impl>(w, h, title, samples)) {}
PlatformWindow::~PlatformWindow() = default;
void PlatformWindow::pollEvents() { impl_->poll(); }
void PlatformWindow::swapBuffers() { [[impl_->view openGLContext] flushBuffer]; }
bool PlatformWindow::setVSync(bool on) {
    GLint interval = on ? 1 : 0;
    [[impl_->view openGLContext] setValues:&interval forParameter:NSOpenGLContextParameterSwapInterval];
    return true;
}
void PlatformWindow::setTitle(const char* title) {
    [impl_->window setTitle:[NSString stringWithUTF8String:title]];
}
bool PlatformWindow::shouldClose() const { return impl_->closeRequested; }
void PlatformWindow::requestClose() { impl_->closeRequested = true; }
bool PlatformWindow::keyDown(int key) const {
    return key >= 0 && key < Key::Count && impl_->keys[static_cast<size_t>(key)];
}
const PlatformWindow::InputState& PlatformWindow::input() const { return impl_->input; }
void PlatformWindow::setMouseCaptured(bool captured) { impl_->capture(captured); }
bool PlatformWindow::mouseCaptured() const { return impl_->captured; }

#endif
