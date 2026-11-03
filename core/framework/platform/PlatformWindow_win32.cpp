#include "framework/PlatformWindow.h"

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <GL/gl.h>

#include <array>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

std::wstring widen(const char* utf8) {
    if (!utf8 || !*utf8) return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    std::wstring out(static_cast<size_t>(n > 0 ? n - 1 : 0), L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), n);
    return out;
}

// Constants of WGL_ARB_pixel_format and WGL_ARB_multisample: Windows' own
// headers stop at OpenGL 1.1, but the values are fixed by the specification.
constexpr int WGL_DRAW_TO_WINDOW_ARB = 0x2001;
constexpr int WGL_ACCELERATION_ARB = 0x2003;
constexpr int WGL_SUPPORT_OPENGL_ARB = 0x2010;
constexpr int WGL_DOUBLE_BUFFER_ARB = 0x2011;
constexpr int WGL_PIXEL_TYPE_ARB = 0x2013;
constexpr int WGL_COLOR_BITS_ARB = 0x2014;
constexpr int WGL_DEPTH_BITS_ARB = 0x2022;
constexpr int WGL_STENCIL_BITS_ARB = 0x2023;
constexpr int WGL_FULL_ACCELERATION_ARB = 0x2027;
constexpr int WGL_TYPE_RGBA_ARB = 0x202B;
constexpr int WGL_SAMPLE_BUFFERS_ARB = 0x2041;
constexpr int WGL_SAMPLES_ARB = 0x2042;

using ChoosePixelFormatArb = BOOL(WINAPI*)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
using SwapIntervalExt = BOOL(WINAPI*)(int);

// wglGetProcAddress says "not there" with 0, 1, 2, 3 or -1, depending on the driver.
void* extension(const char* name) {
    const auto address = reinterpret_cast<INT_PTR>(wglGetProcAddress(name));
    if (address >= -1 && address <= 3) return nullptr;
    return reinterpret_cast<void*>(address);
}

PIXELFORMATDESCRIPTOR basicFormat() {
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;  // for planar shadows and reflections
    pfd.iLayerType = PFD_MAIN_PLANE;
    return pfd;
}

// A pixel format with `samples`-fold multisampling, or 0 if the driver has none.
// ChoosePixelFormat cannot ask for multisampling; wglChoosePixelFormatARB can,
// but an extension function can only be looked up while some OpenGL context is
// current, and a window's pixel format can be set only once. So a throw-away
// window and context are made for the look-up, then removed.
int multisampledFormat(HINSTANCE instance, HDC target, int samples) {
    HWND probe = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
    if (!probe) return 0;
    int format = 0;
    HDC dc = GetDC(probe);
    HGLRC context = nullptr;
    const PIXELFORMATDESCRIPTOR pfd = basicFormat();
    const int basic = ChoosePixelFormat(dc, &pfd);
    if (basic != 0 && SetPixelFormat(dc, basic, &pfd) && (context = wglCreateContext(dc)) != nullptr &&
        wglMakeCurrent(dc, context)) {
        const auto choose = reinterpret_cast<ChoosePixelFormatArb>(extension("wglChoosePixelFormatARB"));
        if (choose) {
            const int attributes[] = {WGL_DRAW_TO_WINDOW_ARB, 1, WGL_SUPPORT_OPENGL_ARB, 1, WGL_DOUBLE_BUFFER_ARB, 1,
                                      WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB, WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
                                      WGL_COLOR_BITS_ARB, 32, WGL_DEPTH_BITS_ARB, 24, WGL_STENCIL_BITS_ARB, 8,
                                      WGL_SAMPLE_BUFFERS_ARB, 1, WGL_SAMPLES_ARB, samples, 0};
            int candidate = 0;
            UINT count = 0;
            if (choose(target, attributes, nullptr, 1, &candidate, &count) && count > 0) format = candidate;
        }
    }
    wglMakeCurrent(nullptr, nullptr);
    if (context) wglDeleteContext(context);
    ReleaseDC(probe, dc);
    DestroyWindow(probe);
    return format;
}

// Without this Windows runs the program at 96 dpi and stretches its window
// when the screen is set to 125 % or 150 %: every picture is soft and blurry.
// A program that says it handles dpi gets the real pixels.
void declareDpiAware() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    // Through void*: a FARPROC is a pointer to a function of no particular type.
    const auto find = [&](const char* name) { return reinterpret_cast<void*>(GetProcAddress(user32, name)); };
    using SetContext = BOOL(WINAPI*)(HANDLE);
    if (const auto set = reinterpret_cast<SetContext>(find("SetProcessDpiAwarenessContext"))) {
        set(reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4)));  // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
        return;
    }
    using SetAware = BOOL(WINAPI*)();
    if (const auto old = reinterpret_cast<SetAware>(find("SetProcessDPIAware"))) old();
}

}  // namespace

struct PlatformWindow::Impl {
    static constexpr wchar_t kClassName[] = L"CityLabWindowClass";

    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    HDC deviceContext = nullptr;
    HGLRC openGLContext = nullptr;

    bool closeRequested = false;
    std::array<bool, Key::Count> keys{};
    InputState input{};
    POINT previousMouse{0, 0};
    bool havePreviousMouse = false;
    bool captured = false;
    bool warpPending = false;  // we moved the pointer; its WM_MOUSEMOVE is not the user's

    // Mouse look uses raw input: the movement of the mouse itself, before
    // Windows applies pointer speed and acceleration and rounds it to whole
    // pixels. It needs no warping of the pointer, so no movement is lost.
    bool rawSeen = false;       // raw movement has arrived since the mouse was captured
    bool pointerDevice = false; // the mouse reports where it is, not how far it went (remote desktop): use WM_MOUSEMOVE
    SwapIntervalExt swapInterval = nullptr;

    Impl(int width, int height, const char* title, int samples) {
        instance = GetModuleHandleW(nullptr);
        declareDpiAware();

        WNDCLASSW wc{};
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = &Impl::windowProcedure;
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kClassName;
        RegisterClassW(&wc);

        // Ask for a client area of exactly width x height.
        RECT rect{0, 0, width, height};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        window = CreateWindowExW(0, kClassName, widen(title).c_str(),
                                 WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                 CW_USEDEFAULT, CW_USEDEFAULT,
                                 rect.right - rect.left, rect.bottom - rect.top,
                                 nullptr, nullptr, instance, this);
        if (!window) throw std::runtime_error("Could not create the Windows window.");

        deviceContext = GetDC(window);
        // Multisampling like on X11 and macOS (4x unless settings.json says otherwise), if
        // the driver offers it; otherwise the plain format, without it.
        bool formatSet = false;
        if (const int format = samples > 0 ? multisampledFormat(instance, deviceContext, samples) : 0; format != 0) {
            PIXELFORMATDESCRIPTOR chosen{};
            DescribePixelFormat(deviceContext, format, sizeof(chosen), &chosen);
            formatSet = SetPixelFormat(deviceContext, format, &chosen) != FALSE;
        }
        if (!formatSet) {
            const PIXELFORMATDESCRIPTOR pfd = basicFormat();
            const int format = ChoosePixelFormat(deviceContext, &pfd);
            if (format == 0 || !SetPixelFormat(deviceContext, format, &pfd))
                throw std::runtime_error("Could not configure the OpenGL pixel format.");
        }

        openGLContext = wglCreateContext(deviceContext);
        if (!openGLContext || !wglMakeCurrent(deviceContext, openGLContext))
            throw std::runtime_error("Could not create the OpenGL context.");

        RECT client{};
        GetClientRect(window, &client);
        input.framebufferWidth = client.right - client.left;
        input.framebufferHeight = client.bottom - client.top;
    }

    ~Impl() {
        if (captured) capture(false);
        if (openGLContext) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(openGLContext);
        }
        if (deviceContext && window) ReleaseDC(window, deviceContext);
        if (window) DestroyWindow(window);
        if (instance) UnregisterClassW(kClassName, instance);
    }

    static LRESULT CALLBACK windowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        Impl* self = nullptr;
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = static_cast<Impl*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }
        if (self) return self->handle(hwnd, msg, wp, lp);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    static int translate(WPARAM vk) {
        if (vk >= 'A' && vk <= 'Z') return static_cast<int>(vk - 'A' + 'a');
        if (vk >= '0' && vk <= '9') return static_cast<int>(vk);
        if (vk >= VK_F1 && vk <= VK_F12) return Key::F1 + static_cast<int>(vk - VK_F1);
        switch (vk) {
        case VK_SPACE: return ' ';
        case VK_UP: return Key::Up;
        case VK_DOWN: return Key::Down;
        case VK_LEFT: return Key::Left;
        case VK_RIGHT: return Key::Right;
        case VK_PRIOR: return Key::PageUp;
        case VK_NEXT: return Key::PageDown;
        case VK_HOME: return Key::Home;
        case VK_END: return Key::End;
        case VK_SHIFT: return Key::Shift;
        case VK_CONTROL: return Key::Control;
        case VK_ESCAPE: return Key::Escape;
        case VK_TAB: return Key::Tab;
        case VK_RETURN: return Key::Enter;
        case VK_BACK: return Key::Backspace;
        case VK_OEM_MINUS: return '-';
        case VK_OEM_PLUS: return '=';
        case VK_OEM_COMMA: return ',';
        case VK_OEM_PERIOD: return '.';
        default: return -1;
        }
    }

    void updateVirtualKey(WPARAM vk, LPARAM lp, bool down) {
        const int key = translate(vk);
        if (key < 0) return;
        const bool wasDown = (lp & (1 << 30)) != 0;  // auto-repeat flag
        // Only the press itself counts as "pressed": a key held for half a second must not
        // fire again and again (a held Esc would free the mouse and then quit the program).
        if (down && !wasDown) input.pressed.push_back(key);
        if (down && wasDown) return;
        keys[static_cast<size_t>(key)] = down;
    }

    LRESULT handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY: closeRequested = true; return 0;
        case WM_SIZE:
            input.framebufferWidth = LOWORD(lp);
            input.framebufferHeight = HIWORD(lp);
            input.resized = true;
            return 0;
        case WM_KILLFOCUS:
            keys.fill(false);
            capture(false);
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            // F10 is the Windows "menu" key; handle it ourselves.
            updateVirtualKey(wp, lp, true);
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP: updateVirtualKey(wp, lp, false); return 0;
        case WM_LBUTTONDOWN:
            input.leftMouseDown = input.leftClicked = true;
            SetCapture(hwnd);
            rememberMouse(lp);
            return 0;
        case WM_LBUTTONUP: input.leftMouseDown = false; releaseIfNeeded(); return 0;
        case WM_RBUTTONDOWN: input.rightMouseDown = true; SetCapture(hwnd); rememberMouse(lp); return 0;
        case WM_RBUTTONUP: input.rightMouseDown = false; releaseIfNeeded(); return 0;
        case WM_MBUTTONDOWN: input.middleMouseDown = true; SetCapture(hwnd); rememberMouse(lp); return 0;
        case WM_MBUTTONUP: input.middleMouseDown = false; releaseIfNeeded(); return 0;
        case WM_INPUT: {
            if (captured) takeRawInput(reinterpret_cast<HRAWINPUT>(lp));
            return DefWindowProcW(hwnd, msg, wp, lp);  // the system needs this to clean up
        }
        case WM_MOUSEMOVE: {
            const POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            input.mouseX = p.x;
            input.mouseY = p.y;
            // While the mouse is captured and reports raw movement, that is
            // where looking comes from; the pointer itself is pinned in place.
            const bool fromRaw = captured && rawSeen && !pointerDevice;
            if (!fromRaw) {
                if (warpPending && p.x == input.framebufferWidth / 2 && p.y == input.framebufferHeight / 2) {
                    warpPending = false;  // our own SetCursorPos, not a movement
                } else if (havePreviousMouse) {
                    input.mouseDeltaX += static_cast<float>(p.x - previousMouse.x);
                    input.mouseDeltaY += static_cast<float>(p.y - previousMouse.y);
                }
            }
            previousMouse = p;
            havePreviousMouse = true;
            return 0;
        }
        case WM_MOUSEWHEEL:
            input.scrollDelta += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) /
                                 static_cast<float>(WHEEL_DELTA);
            return 0;
        default: return DefWindowProcW(hwnd, msg, wp, lp);
        }
    }

    void rememberMouse(LPARAM lp) {
        previousMouse = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        havePreviousMouse = true;
    }

    void releaseIfNeeded() {
        if (!captured && !input.leftMouseDown && !input.rightMouseDown && !input.middleMouseDown)
            ReleaseCapture();
    }

    void registerRawMouse(bool on) {
        RAWINPUTDEVICE device{};
        device.usUsagePage = 0x01;  // generic desktop controls
        device.usUsage = 0x02;      // mouse
        device.dwFlags = on ? 0 : RIDEV_REMOVE;  // the buttons and the wheel still arrive as before
        device.hwndTarget = on ? window : nullptr;
        RegisterRawInputDevices(&device, 1, sizeof(device));
    }

    void takeRawInput(HRAWINPUT handle) {
        alignas(8) BYTE buffer[128];
        UINT size = sizeof(buffer);
        if (GetRawInputData(handle, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1)) return;
        const RAWINPUT* raw = reinterpret_cast<const RAWINPUT*>(buffer);
        if (raw->header.dwType != RIM_TYPEMOUSE) return;
        if (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) {
            // A remote desktop or a tablet: the position is all there is. Fall
            // back to the pointer's own movement, with the pointer kept near the middle.
            if (!pointerDevice) {
                pointerDevice = true;
                recentre();
            }
            return;
        }
        rawSeen = true;
        input.mouseDeltaX += static_cast<float>(raw->data.mouse.lLastX);
        input.mouseDeltaY += static_cast<float>(raw->data.mouse.lLastY);
    }

    // Holds the pointer in one pixel in the middle of the window, so it can
    // never leave the window or reach an edge while the mouse looks around.
    void pinPointer() {
        RECT client{};
        GetClientRect(window, &client);
        POINT middle{(client.left + client.right) / 2, (client.top + client.bottom) / 2};
        ClientToScreen(window, &middle);
        SetCursorPos(middle.x, middle.y);
        const RECT pin{middle.x, middle.y, middle.x + 1, middle.y + 1};
        ClipCursor(&pin);
    }

    void capture(bool on) {
        if (on == captured) return;
        captured = on;
        if (on) {
            rawSeen = false;
            pointerDevice = false;
            registerRawMouse(true);
            ShowCursor(FALSE);
            SetCapture(window);
            pinPointer();
        } else {
            registerRawMouse(false);
            ClipCursor(nullptr);
            ShowCursor(TRUE);
            ReleaseCapture();
        }
    }

    // Keeps the pointer inside the window and puts it back in the middle
    // before it reaches an edge. The WM_MOUSEMOVE this causes is skipped.
    // Only for a mouse that cannot report raw movement.
    void recentre() {
        RECT client{};
        GetClientRect(window, &client);
        POINT topLeft{client.left, client.top}, bottomRight{client.right, client.bottom};
        ClientToScreen(window, &topLeft);
        ClientToScreen(window, &bottomRight);
        const RECT screen{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
        ClipCursor(&screen);
        POINT middle{(client.left + client.right) / 2, (client.top + client.bottom) / 2};
        ClientToScreen(window, &middle);
        SetCursorPos(middle.x, middle.y);
        warpPending = true;
    }

    bool farFromMiddle() const {
        return std::abs(previousMouse.x - input.framebufferWidth / 2) > input.framebufferWidth / 4 ||
               std::abs(previousMouse.y - input.framebufferHeight / 2) > input.framebufferHeight / 4;
    }

    void poll() {
        input.mouseDeltaX = input.mouseDeltaY = input.scrollDelta = 0.0f;
        input.resized = false;
        input.leftClicked = false;
        input.pressed.clear();
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (captured && pointerDevice && !warpPending && farFromMiddle()) recentre();
    }
};

PlatformWindow::PlatformWindow(int w, int h, const char* title, int samples)
    : impl_(std::make_unique<Impl>(w, h, title, samples)) {}
PlatformWindow::~PlatformWindow() = default;
void PlatformWindow::pollEvents() { impl_->poll(); }
void PlatformWindow::swapBuffers() { SwapBuffers(impl_->deviceContext); }
bool PlatformWindow::setVSync(bool on) {
    if (!impl_->swapInterval) impl_->swapInterval = reinterpret_cast<SwapIntervalExt>(extension("wglSwapIntervalEXT"));
    return impl_->swapInterval && impl_->swapInterval(on ? 1 : 0) != FALSE;
}
void PlatformWindow::setTitle(const char* title) {
    SetWindowTextW(impl_->window, widen(title).c_str());
}
bool PlatformWindow::shouldClose() const { return impl_->closeRequested; }
void PlatformWindow::requestClose() { impl_->closeRequested = true; }
namespace {
// The Windows virtual key of the keys the viewer holds down to move, or 0.
int heldKeyCode(int key) {
    if (key >= 'a' && key <= 'z') return key - 'a' + 'A';
    switch (key) {
    case Key::Up: return VK_UP;
    case Key::Down: return VK_DOWN;
    case Key::Left: return VK_LEFT;
    case Key::Right: return VK_RIGHT;
    case Key::PageUp: return VK_PRIOR;
    case Key::PageDown: return VK_NEXT;
    case Key::Shift: return VK_SHIFT;
    case Key::Control: return VK_CONTROL;
    default: return 0;
    }
}
}  // namespace

bool PlatformWindow::keyDown(int key) const {
    // The keys that are held to move are read from the keyboard itself while the window has the
    // focus, so a key-up that a click, the mouse capture or another program swallowed can never
    // leave a key stuck down (or a held key unseen for a frame).
    if (const int code = heldKeyCode(key)) return GetForegroundWindow() == impl_->window && (GetAsyncKeyState(code) & 0x8000) != 0;
    return key >= 0 && key < Key::Count && impl_->keys[static_cast<size_t>(key)];
}
const PlatformWindow::InputState& PlatformWindow::input() const { return impl_->input; }
void PlatformWindow::setMouseCaptured(bool captured) { impl_->capture(captured); }
bool PlatformWindow::mouseCaptured() const { return impl_->captured; }

#endif
