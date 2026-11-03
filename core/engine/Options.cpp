#include "engine/Options.h"

#include <sstream>
#include <stdexcept>

namespace citylab {
namespace {

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, separator))
        if (!part.empty()) parts.push_back(part);
    return parts;
}

float toFloat(const std::string& option, const std::string& value) {
    try {
        size_t used = 0;
        const float f = std::stof(value, &used);
        if (used == value.size()) return f;
    } catch (const std::logic_error&) {
    }
    throw std::invalid_argument(option + " expects a number, got '" + value + "'");
}

float parseClock(const std::string& value) {
    const auto colon = value.find(':');
    if (colon == std::string::npos) return toFloat("--time", value);
    const float hours = toFloat("--time", value.substr(0, colon));
    const float minutes = toFloat("--time", value.substr(colon + 1));
    return hours + minutes / 60.0f;
}

}  // namespace

std::string optionsHelp() {
    return R"(Usage: citylab [options]

  --team ID[,ID...]    run only these zones (e.g. --team team07); the camera
                       starts at that plot
  --check ID           check one zone automatically, write
                       build/check/ID.json and screenshots, then exit
  --walk | --fly | --orbit
                       starting camera mode (F9 switches while running)
  --tour               walk past every plot automatically
  --time HH[:MM]       start the city clock at this time, e.g. --time 19:30
  --time-scale N       city seconds per real second by day (default 47);
                       nights run 3.5 times faster
  --fixed-dt S         advance exactly S seconds per frame (reproducible)
  --screenshot FILE    save a PNG after --frames frames and exit
  --frames N           number of frames to run (default 90 with --screenshot)
  --camera X,Y,Z,YAW,PITCH
                       start the camera here (degrees; yaw 0 looks north)
  --size WxH           window size (default 1280x720)
  --no-zones           draw the city without any team zones
  --no-vsync           do not wait for the screen (settings.json: "vsync")
  --msaa N             multisampling 0, 2, 4 or 8 (settings.json: "msaa")
  --bench N            staff: run N frames as fast as possible and print the
                       frame times (min/avg/max) and what the city draws;
                       use with --camera, --time, --fixed-dt, --no-zones
  --list               print the plots and teams, then exit
  --root DIR           repository root (default: found automatically)
  --version            print the version
  --help               show this text

Keys:  Space jump (walk mode; hold to bunny hop)
       F1 help   F2 plot borders   F3 statistics   F4 map   F5 clock speed
       F6 pause clock   F7 +1 hour   F8 reset zone   F9 camera mode   F10 HUD
       F12 screenshot
       Tab / Shift+Tab next / previous place   Home back to start
       Click to look with the mouse; Esc frees the mouse, a second Esc quits
)";
}

Options parseOptions(int argc, char** argv) {
    Options o;
    for (int k = 1; k < argc; ++k) {
        const std::string arg = argv[k];
        auto value = [&]() -> std::string {
            if (k + 1 >= argc) throw std::invalid_argument(arg + " needs a value");
            return argv[++k];
        };
        if (arg == "--help" || arg == "-h") o.showHelp = true;
        else if (arg == "--version") o.showVersion = true;
        else if (arg == "--root") o.root = value();
        else if (arg == "--team" || arg == "--teams") {
            for (const auto& t : split(value(), ',')) o.teams.push_back(t);
        } else if (arg == "--check") o.checkTeam = value();
        else if (arg == "--time") o.startTime = parseClock(value());
        else if (arg == "--time-scale") o.timeScale = toFloat(arg, value());
        else if (arg == "--fixed-dt") o.fixedDt = toFloat(arg, value());
        else if (arg == "--tour") o.tour = true;
        else if (arg == "--walk") o.cameraMode = CameraMode::Walk;
        else if (arg == "--fly") o.cameraMode = CameraMode::Fly;
        else if (arg == "--orbit") o.cameraMode = CameraMode::Orbit;
        else if (arg == "--screenshot") o.screenshotPath = value();
        else if (arg == "--frames") o.frames = static_cast<int>(toFloat(arg, value()));
        else if (arg == "--no-zones") o.noZones = true;
        else if (arg == "--no-vsync") o.noVsync = true;
        else if (arg == "--msaa") o.msaa = static_cast<int>(toFloat(arg, value()));
        else if (arg == "--bench") o.bench = static_cast<int>(toFloat(arg, value()));
        else if (arg == "--list") o.list = true;
        else if (arg == "--camera") {
            std::vector<float> c;
            for (const auto& p : split(value(), ',')) c.push_back(toFloat(arg, p));
            if (c.size() != 5) throw std::invalid_argument("--camera expects X,Y,Z,YAW,PITCH");
            o.camera = c;
        } else if (arg == "--size") {
            const auto parts = split(value(), 'x');
            if (parts.size() != 2) throw std::invalid_argument("--size expects WxH, e.g. 1600x900");
            o.width = static_cast<int>(toFloat(arg, parts[0]));
            o.height = static_cast<int>(toFloat(arg, parts[1]));
        } else {
            throw std::invalid_argument("unknown option '" + arg + "' (see --help)");
        }
    }
    if (!o.screenshotPath.empty() && o.frames <= 0) o.frames = 90;
    if (o.bench < 0) throw std::invalid_argument("--bench expects a number of frames");
    if (o.bench > 0 && o.frames > 0) throw std::invalid_argument("use either --bench or --frames, not both");
    if (o.timeScale < 0.0f) throw std::invalid_argument("--time-scale must not be negative");
    if (o.fixedDt && (*o.fixedDt <= 0.0f || *o.fixedDt > 1.0f))
        throw std::invalid_argument("--fixed-dt must be between 0 and 1 second");
    return o;
}

}  // namespace citylab
