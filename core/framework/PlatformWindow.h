#pragma once

#include <memory>
#include <vector>

// Cross-platform window, input, and OpenGL-context wrapper.
//
// This is the only part of CityLab that talks to the operating system. The
// three implementations live in framework/platform/ (Win32, X11, Cocoa) and
// all expose exactly this interface.
//
// Key codes: printable keys use their ASCII code (letters are always reported
// in lowercase, so 'a' and 'A' are the same key). Special keys use the codes
// in the Key namespace below, which all start at 256 so they never collide
// with a character.
namespace Key {
enum : int {
    Up = 256, Down, Left, Right,
    PageUp, PageDown, Home, End,
    Shift, Control, Escape, Tab, Enter, Backspace,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Count
};
}

class PlatformWindow {
public:
    struct InputState {
        float mouseDeltaX = 0.0f;
        float mouseDeltaY = 0.0f;
        float scrollDelta = 0.0f;
        int mouseX = 0;
        int mouseY = 0;

        bool leftMouseDown = false;
        bool leftClicked = false;  // the left button went down since the previous pollEvents()
        bool rightMouseDown = false;
        bool middleMouseDown = false;

        int framebufferWidth = 1;
        int framebufferHeight = 1;
        bool resized = false;

        // Keys that went down since the previous pollEvents(), in order.
        // Use this for "toggle" actions; use keyDown() for held keys.
        std::vector<int> pressed;
    };

    // `samples`: the multisampling to ask for (0, 2, 4 or 8); the system gives the nearest it has.
    PlatformWindow(int width, int height, const char* title, int samples = 4);
    ~PlatformWindow();

    PlatformWindow(const PlatformWindow&) = delete;
    PlatformWindow& operator=(const PlatformWindow&) = delete;

    void pollEvents();
    void swapBuffers();
    // Waits for the screen's refresh before showing a frame (true) or shows it
    // at once (false). Returns whether the system allows the choice.
    bool setVSync(bool on);
    void setTitle(const char* title);

    bool shouldClose() const;
    void requestClose();
    bool keyDown(int key) const;
    const InputState& input() const;

    // Mouse look: hides the pointer and keeps it inside the window, so every
    // movement of the mouse turns the view (mouseDeltaX/Y) without holding a
    // button. The system ends it when the window loses focus.
    void setMouseCaptured(bool captured);
    bool mouseCaptured() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
