#pragma once

#include <filesystem>

namespace citylab {

// The viewer's own preferences, kept in settings.json at the root of the
// repository (git ignores it). The city writes the file with the defaults
// the first time it runs, so it is easy to find and edit.
struct Settings {
    float mouseSensitivity = 1.0f;  // 1 = default; 2 turns twice as fast
    bool invertMouse = false;       // moving the mouse up looks down
    float fieldOfView = 75.0f;      // degrees, vertical
    float walkSpeed = 7.0f;         // metres per second
    float runSpeed = 16.0f;         // with Shift
    float flySpeed = 30.0f;         // with Shift four times as fast
    bool headBob = true;            // the view rises and falls a little while walking
    bool vsync = true;              // wait for the screen: no tearing, steady frames (false: as fast as possible)
    int msaa = 4;                   // multisampling: 0 (off), 2, 4 or 8; lower it on a slow graphics card
    bool walkThroughObjects = false;  // walk through walls and objects (F11 switches it while running)

    // Reads the file; missing or wrong values keep their defaults. With
    // `create`, a missing file is written with the defaults.
    static Settings load(const std::filesystem::path& file, bool create);
};

}  // namespace citylab
