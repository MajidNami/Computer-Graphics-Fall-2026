#pragma once

#include <optional>
#include <string>
#include <vector>

namespace citylab {

enum class CameraMode { Orbit, Fly, Walk };

// Command-line options. `citylab --help` prints the same list.
struct Options {
    std::string root;                    // repository root; empty = auto-detect
    std::vector<std::string> teams;      // --team: start only these zones
    std::string checkTeam;               // --check: run the plot checker and exit
    std::optional<float> startTime;      // --time, hours
    float timeScale = 47.0f;             // --time-scale, city seconds per real second (by day)
    std::optional<float> fixedDt;        // --fixed-dt, seconds per frame
    bool tour = false;                   // --tour
    std::optional<CameraMode> cameraMode;
    std::string screenshotPath;          // --screenshot
    int frames = 0;                      // --frames (0 = run until closed)
    int width = 1280, height = 720;      // --size
    bool noZones = false;                // --no-zones
    int bench = 0;                       // --bench N: measure N frames, print the frame times, exit
    std::optional<int> msaa;             // --msaa N: multisampling, whatever settings.json says
    bool noVsync = false;                // --no-vsync: swap as fast as possible, whatever settings.json says
    bool list = false;                   // --list
    std::optional<std::vector<float>> camera;  // --camera x,y,z,yawDeg,pitchDeg
    bool showHelp = false;
    bool showVersion = false;
};

// Throws std::invalid_argument with a readable message on bad input.
Options parseOptions(int argc, char** argv);
std::string optionsHelp();

}  // namespace citylab
