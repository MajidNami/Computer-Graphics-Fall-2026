#include "framework/PlatformWindow.h"

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include <array>
#include <cstdlib>
#include <stdexcept>

namespace {
// GLX 1.4 names; older headers only have the _ARB spelling.
#ifndef GLX_SAMPLE_BUFFERS
constexpr int GLX_SAMPLE_BUFFERS = 100000;
constexpr int GLX_SAMPLES = 100001;
#endif
}

struct PlatformWindow::Impl {
    Display* display = nullptr;
    ::Window window = 0;
    GLXContext context = nullptr;
    Atom wmDelete{};
    bool closeRequested = false;
    std::array<bool, Key::Count> keys{};
    InputState input{};
    int previousX = 0, previousY = 0;  // for dragging with a button, when the mouse is not captured
    bool havePreviousMouse = false;
    bool captured = false;
    bool detectableRepeat = false;  // the server does not report auto-repeat as release + press
    Cursor blankCursor = 0;

    Impl(int width, int height, const char* title, int samples) {
        display = XOpenDisplay(nullptr);
        if (!display) {
            throw std::runtime_error(
                "Could not open the X11 display. On a server or in CI, run under "
                "xvfb-run; on WSL, make sure an X server (WSLg) is running.");
        }

        // Without this, a held key is reported as an endless row of releases and presses, and a
        // mouse movement that arrives between the two makes the key look let go for a frame.
        Bool supported = False;
        detectableRepeat = XkbSetDetectableAutoRepeat(display, True, &supported) && supported;

        // Prefer 4x multisampling and a stencil buffer (for planar shadows and
        // reflections); fall back step by step on drivers that refuse.
        int withMsaa[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 24,
                          GLX_STENCIL_SIZE, 8, GLX_SAMPLE_BUFFERS, 1, GLX_SAMPLES, samples > 0 ? samples : 4, None};
        int withStencil[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 24,
                             GLX_STENCIL_SIZE, 8, None};
        int minimal[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 16, None};

        XVisualInfo* visual = samples > 0 ? glXChooseVisual(display, DefaultScreen(display), withMsaa) : nullptr;
        if (!visual) visual = glXChooseVisual(display, DefaultScreen(display), withStencil);
        if (!visual) visual = glXChooseVisual(display, DefaultScreen(display), minimal);
        if (!visual) throw std::runtime_error("Could not choose an OpenGL visual.");

        Colormap cmap = XCreateColormap(display, RootWindow(display, visual->screen),
                                        visual->visual, AllocNone);
        XSetWindowAttributes attrs{};
        attrs.colormap = cmap;
        attrs.event_mask = ExposureMask | StructureNotifyMask |
                           KeyPressMask | KeyReleaseMask |
                           ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask;

        window = XCreateWindow(display, RootWindow(display, visual->screen),
                               0, 0, static_cast<unsigned>(width), static_cast<unsigned>(height),
                               0, visual->depth, InputOutput, visual->visual,
                               CWColormap | CWEventMask, &attrs);
        XStoreName(display, window, title);
        wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(display, window, &wmDelete, 1);
        XMapWindow(display, window);

        context = glXCreateContext(display, visual, nullptr, True);
        XFree(visual);
        if (!context || !glXMakeCurrent(display, window, context))
            throw std::runtime_error("Could not create the OpenGL context.");

        input.framebufferWidth = width;
        input.framebufferHeight = height;
    }

    ~Impl() {
        if (display && captured) capture(false);
        if (display && blankCursor) XFreeCursor(display, blankCursor);
        if (display && context) {
            glXMakeCurrent(display, None, nullptr);
            glXDestroyContext(display, context);
        }
        if (display && window) XDestroyWindow(display, window);
        if (display) XCloseDisplay(display);
    }

    static int translate(KeySym symbol) {
        if (symbol >= XK_A && symbol <= XK_Z) return static_cast<int>(symbol - XK_A + 'a');
        if (symbol >= 32 && symbol <= 126) return static_cast<int>(symbol);
        if (symbol >= XK_F1 && symbol <= XK_F12) return Key::F1 + static_cast<int>(symbol - XK_F1);
        switch (symbol) {
        case XK_Up: return Key::Up;
        case XK_Down: return Key::Down;
        case XK_Left: return Key::Left;
        case XK_Right: return Key::Right;
        case XK_Page_Up: return Key::PageUp;
        case XK_Page_Down: return Key::PageDown;
        case XK_Home: return Key::Home;
        case XK_End: return Key::End;
        case XK_Shift_L: case XK_Shift_R: return Key::Shift;
        case XK_Control_L: case XK_Control_R: return Key::Control;
        case XK_Escape: return Key::Escape;
        case XK_Tab: case XK_ISO_Left_Tab: return Key::Tab;
        case XK_Return: case XK_KP_Enter: return Key::Enter;
        case XK_BackSpace: return Key::Backspace;
        default: return -1;
        }
    }

    void updateKey(KeySym symbol, bool down) {
        const int key = translate(symbol);
        if (key < 0) return;
        if (down && !keys[static_cast<size_t>(key)]) input.pressed.push_back(key);
        keys[static_cast<size_t>(key)] = down;
    }

    // X11 reports auto-repeat as release+press pairs. Peek at the next event so
    // a held key stays "down" instead of flickering.
    bool isAutoRepeat(const XEvent& release) {
        if (XEventsQueued(display, QueuedAfterReading) == 0) return false;
        XEvent next{};
        XPeekEvent(display, &next);
        return next.type == KeyPress && next.xkey.time == release.xkey.time &&
               next.xkey.keycode == release.xkey.keycode;
    }

    void capture(bool on) {
        if (on == captured) return;
        captured = on;
        if (on) {
            if (!blankCursor) {  // a 1 x 1 cursor with nothing in it
                char none = 0;
                const Pixmap empty = XCreateBitmapFromData(display, window, &none, 1, 1);
                XColor black{};
                blankCursor = XCreatePixmapCursor(display, empty, empty, &black, &black, 0, 0);
                XFreePixmap(display, empty);
            }
            XDefineCursor(display, window, blankCursor);
            XGrabPointer(display, window, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                         GrabModeAsync, GrabModeAsync, window, blankCursor, CurrentTime);
            recentre();
            XSync(display, False);
        } else {
            XUngrabPointer(display, CurrentTime);
            XUndefineCursor(display, window);
        }
        XFlush(display);
    }

    // Puts the pointer in the middle of the window.
    void recentre() {
        XWarpPointer(display, None, window, 0, 0, 0, 0, input.framebufferWidth / 2, input.framebufferHeight / 2);
        XFlush(display);
    }

    // Mouse look: how far the pointer has gone from the middle since the last
    // frame, then back to the middle. Asking the server where the pointer is
    // gives all the movement up to now, whatever the motion events say, and
    // the pointer is at the middle at the start of every frame, so a quick
    // flick cannot run into the edge of the window and be lost.
    void takePointerMovement() {
        ::Window root = 0, child = 0;
        int rootX = 0, rootY = 0, x = 0, y = 0;
        unsigned mask = 0;
        if (!XQueryPointer(display, window, &root, &child, &rootX, &rootY, &x, &y, &mask)) return;
        const int middleX = input.framebufferWidth / 2, middleY = input.framebufferHeight / 2;
        if (x == middleX && y == middleY) return;
        input.mouseDeltaX += static_cast<float>(x - middleX);
        input.mouseDeltaY += static_cast<float>(y - middleY);
        recentre();
    }

    void poll() {
        input.mouseDeltaX = input.mouseDeltaY = input.scrollDelta = 0.0f;
        input.resized = false;
        input.leftClicked = false;
        input.pressed.clear();

        while (XPending(display) > 0) {
            XEvent e{};
            XNextEvent(display, &e);
            switch (e.type) {
            case ClientMessage:
                if (static_cast<Atom>(e.xclient.data.l[0]) == wmDelete) closeRequested = true;
                break;
            case ConfigureNotify:
                if (e.xconfigure.width != input.framebufferWidth ||
                    e.xconfigure.height != input.framebufferHeight) {
                    input.framebufferWidth = e.xconfigure.width;
                    input.framebufferHeight = e.xconfigure.height;
                    input.resized = true;
                }
                break;
            case KeyPress: updateKey(XLookupKeysym(&e.xkey, 0), true); break;
            case KeyRelease:
                if (!detectableRepeat && isAutoRepeat(e)) {
                    XEvent repeat{};
                    XNextEvent(display, &repeat);  // swallow the matching press: a held key is not pressed again
                } else {
                    updateKey(XLookupKeysym(&e.xkey, 0), false);
                }
                break;
            case ButtonPress:
                if (e.xbutton.button == Button1) input.leftMouseDown = input.leftClicked = true;
                if (e.xbutton.button == Button2) input.middleMouseDown = true;
                if (e.xbutton.button == Button3) input.rightMouseDown = true;
                if (e.xbutton.button == Button4) input.scrollDelta += 1.0f;
                if (e.xbutton.button == Button5) input.scrollDelta -= 1.0f;
                previousX = e.xbutton.x; previousY = e.xbutton.y; havePreviousMouse = true;
                break;
            case ButtonRelease:
                if (e.xbutton.button == Button1) input.leftMouseDown = false;
                if (e.xbutton.button == Button2) input.middleMouseDown = false;
                if (e.xbutton.button == Button3) input.rightMouseDown = false;
                break;
            case MotionNotify:
                // A captured mouse is read with XQueryPointer below; a button drag moves by the events.
                if (!captured && havePreviousMouse) {
                    input.mouseDeltaX += static_cast<float>(e.xmotion.x - previousX);
                    input.mouseDeltaY += static_cast<float>(e.xmotion.y - previousY);
                }
                previousX = e.xmotion.x; previousY = e.xmotion.y; havePreviousMouse = true;
                input.mouseX = e.xmotion.x; input.mouseY = e.xmotion.y;
                break;
            case FocusOut:
                capture(false);
                keys.fill(false);
                break;
            }
        }
        if (captured) takePointerMovement();
    }
};

PlatformWindow::PlatformWindow(int w, int h, const char* title, int samples)
    : impl_(std::make_unique<Impl>(w, h, title, samples)) {}
PlatformWindow::~PlatformWindow() = default;
void PlatformWindow::pollEvents() { impl_->poll(); }
void PlatformWindow::swapBuffers() { glXSwapBuffers(impl_->display, impl_->window); }
bool PlatformWindow::setVSync(bool on) {
    // GLX_EXT_swap_control, then Mesa's and SGI's older spellings of it.
    const int interval = on ? 1 : 0;
    const auto find = [](const char* name) {
        return reinterpret_cast<void (*)()>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name)));
    };
    if (const auto ext = reinterpret_cast<void (*)(Display*, GLXDrawable, int)>(find("glXSwapIntervalEXT"))) {
        ext(impl_->display, impl_->window, interval);
        return true;
    }
    if (const auto mesa = reinterpret_cast<int (*)(unsigned)>(find("glXSwapIntervalMESA"))) return mesa(static_cast<unsigned>(interval)) == 0;
    if (const auto sgi = reinterpret_cast<int (*)(int)>(find("glXSwapIntervalSGI"))) return sgi(interval) == 0;
    return false;
}
void PlatformWindow::setTitle(const char* title) { XStoreName(impl_->display, impl_->window, title); }
bool PlatformWindow::shouldClose() const { return impl_->closeRequested; }
void PlatformWindow::requestClose() { impl_->closeRequested = true; }
bool PlatformWindow::keyDown(int key) const {
    return key >= 0 && key < Key::Count && impl_->keys[static_cast<size_t>(key)];
}
const PlatformWindow::InputState& PlatformWindow::input() const { return impl_->input; }
void PlatformWindow::setMouseCaptured(bool captured) { impl_->capture(captured); }
bool PlatformWindow::mouseCaptured() const { return impl_->captured; }

#endif
