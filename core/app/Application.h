#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/Tour.h"
#include "city/CityScene.h"
#include "city/Layout.h"
#include "city/Life.h"
#include "city/Registry.h"
#include "engine/Camera.h"
#include "engine/Clock.h"
#include "engine/Collision.h"
#include "engine/FirstPersonHands.h"
#include "engine/Hud.h"
#include "engine/Options.h"
#include "engine/Settings.h"
#include "engine/Sky.h"
#include "zones/ZoneHost.h"

class PlatformWindow;

namespace citylab {

// One entry of city/places.json: a kind of building teams can choose.
struct Place {
    std::string id, district, plot, title, titleEn;
};

// Finds the repository root (the folder with city/layout.json): the --root
// option, then the current folder and its parents, then the executable's
// folder and its parents, then the source folder the program was built from.
std::filesystem::path findRoot(const std::string& option, const char* argv0);

// The city program: opens the window, builds the city, runs the zones, and
// handles the camera, the clock and the on-screen display.
class Application {
public:
    Application(Options options, std::filesystem::path root);
    ~Application();

    int run();    // the interactive city (or a scripted screenshot)
    int list();   // --list: print plots and teams
    int check();  // --check: the automatic plot checker (Checker.cpp)

private:
    void loadCity();
    void startZones();
    void placeCamera();
    void handleKeys();
    void handleMouse();
    void render();
    void reportBench(std::vector<double> frameMilliseconds,
                     const std::vector<std::vector<std::pair<double, double>>>& zoneMilliseconds);
    void setUpLights(const Environment& env) const;
    bool visible(const PlotLayout& plot) const;

    // Checker (Checker.cpp)
    struct Capture;
    Capture capture(ZoneHost& zone, ZoneHost::Probe* probe);
    void runFrames(int count, float dt);
    bool shoot(const std::filesystem::path& file);

    // Navigation
    void goToPlot(size_t index);
    void goToStart();
    void jumpTo(float x, float z);  // a click on the map
    std::vector<size_t> placesToVisit() const;
    ZoneHost* zoneOnPlot(const std::string& plotId) const;
    const PlotLayout* focusedPlot() const;

    // On-screen display (Overlay.cpp)
    void drawOverlay();
    void drawLocation(float width);
    void drawClock(float width);
    void drawMinimap(float width, float height);
    void drawHelp(float width, float height);
    void drawStats();
    void drawMessages(float width);
    std::string locationName() const;
    std::string placeTitle(const TeamInfo& team) const;
    void toast(const std::string& message);

    Options options_;
    std::filesystem::path root_;
    Layout layout_;
    Registry registry_;
    std::map<std::string, Place> places_;

    std::unique_ptr<PlatformWindow> window_;
    CollisionWorld collisions_;
    CityScene city_;
    CityLife life_;
    Sky sky_;
    Hud hud_;
    Camera camera_;
    FirstPersonHands hands_;
    Settings settings_;
    CityClock clock_;
    Tour tour_;
    bool touring_ = false;
    std::vector<std::unique_ptr<ZoneHost>> zones_;

    glm::mat4 projection_{1.0f}, view_{1.0f};
    Environment env_;
    double time_ = 0.0;  // seconds since start (fixed-dt aware)
    float dt_ = 0.0f;
    float fps_ = 0.0f;
    long frame_ = 0;
    int visiblePlots_ = 0;

    bool showHelp_ = false, showBorders_ = false, showStats_ = false, showHud_ = true, showMap_ = true;
    // Where the F4 map was last drawn (window pixels) and the world area it shows.
    struct MapFrame {
        float x = 0, y = 0, w = 0, h = 0, minX = 0, maxX = 0, minZ = 0, maxZ = 0;
    } minimap_;
    bool screenshotRequested_ = false;
    bool lookHintShown_ = false;
    int placeCursor_ = -1;
    std::string toast_;
    double toastUntil_ = 0.0;
};

}  // namespace citylab
