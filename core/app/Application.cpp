#include "app/Application.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <set>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "engine/FrameTimer.h"
#include "engine/Json.h"
#include "engine/Log.h"
#include "engine/Screenshot.h"
#include "engine/VertexCount.h"
#include "framework/PlatformWindow.h"

namespace citylab {
namespace {

constexpr float kNear = 0.15f, kFar = 3200.0f;

float fogDensity(const Environment& env) { return 0.00055f + 0.0005f * (1.0f - env.daylight); }

// GL_EXP2 fog leaves exp(-(density * distance)^2) of a colour. Beyond the
// distance where that is 1 %, whatever is there is the colour of the horizon.
float fogVisibleDistance(const Environment& env) { return 2.15f / fogDensity(env); }

bool isRoot(const std::filesystem::path& p) {
    std::error_code ec;
    return std::filesystem::exists(p / "city" / "layout.json", ec);
}

std::filesystem::path searchUpwards(std::filesystem::path p) {
    std::error_code ec;
    p = std::filesystem::absolute(p, ec);
    for (int i = 0; i < 8 && !p.empty(); ++i) {
        if (isRoot(p)) return p;
        if (p == p.parent_path()) break;
        p = p.parent_path();
    }
    return {};
}

bool reservedForCamera(int key) {
    return key == 'w' || key == 'a' || key == 's' || key == 'd' || key == 'q' || key == 'e' || key == ' ';
}

}  // namespace

std::filesystem::path findRoot(const std::string& option, const char* argv0) {
    if (!option.empty()) {
        if (isRoot(option)) return std::filesystem::absolute(option);
        throw std::runtime_error("--root " + option + " does not contain city/layout.json");
    }
    if (auto found = searchUpwards(std::filesystem::current_path()); !found.empty()) return found;
    if (argv0 && *argv0) {
        std::error_code ec;
        const auto exe = std::filesystem::weakly_canonical(std::filesystem::absolute(argv0, ec), ec);
        if (auto found = searchUpwards(exe.parent_path()); !found.empty()) return found;
    }
#ifdef CITYLAB_SOURCE_DIR
    if (isRoot(CITYLAB_SOURCE_DIR)) return CITYLAB_SOURCE_DIR;
#endif
    throw std::runtime_error(
        "cannot find the CityLab folder (the one with city/layout.json); run citylab from inside the "
        "repository or pass --root DIR");
}

// --------------------------------------------------------------------------
// Set-up
// --------------------------------------------------------------------------

Application::Application(Options options, std::filesystem::path root)
    : options_(std::move(options)),
      root_(std::move(root)),
      layout_(Layout::load(root_ / "city" / "layout.json")),
      registry_(Registry::load(root_, layout_)),
      clock_(options_.startTime.value_or(CityClock::kDefaultStart), options_.timeScale) {
    for (const auto& id : options_.teams)
        if (!registry_.ensurePreview(id, root_, layout_)) log::error(id, "there is no folder teams/" + id);

    const auto placesFile = root_ / "city" / "places.json";
    if (std::filesystem::exists(placesFile)) {
        try {
            const Json json = Json::parseFile(placesFile);
            for (const auto& p : json["places"].items())
                places_[p["id"].str()] = {p["id"].str(), p["district"].str(), p["plot"].str(), p["title"].str(),
                                          p["title_en"].str()};
        } catch (const std::exception& e) {
            log::warn("city", e.what());
        }
    }
}

Application::~Application() = default;

int Application::list() {
    std::printf("%-7s  %-8s  %-20s  %-24s  %s\n", "plot", "size", "district", "topic", "team");
    for (const PlotLayout& p : layout_.plots) {
        const DistrictLayout* d = layout_.findDistrict(p.district);
        const TeamInfo* t = registry_.teamOnPlot(p.id);
        const auto place = places_.find(p.place);
        const std::string topic = t ? placeTitle(*t) : place != places_.end() ? place->second.titleEn : "free";
        const std::string size = std::to_string(static_cast<int>(p.width)) + "x" + std::to_string(static_cast<int>(p.depth));
        std::printf("%-7s  %-8s  %-20s  %-24s  %s\n", p.id.c_str(), size.c_str(),
                    d ? d->titleEn.c_str() : p.district.c_str(), topic.c_str(), t ? t->id.c_str() : "-");
    }
    std::printf("\n%zu plots, %zu teams. Places to choose from: city/places.json (%zu).\n", layout_.plots.size(),
                registry_.teams().size(), places_.size());
    return 0;
}

void Application::loadCity() {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_FOG_HINT, GL_NICEST);
    glEnable(GL_MULTISAMPLE);
    glGetError();  // GL_MULTISAMPLE is unknown to some 1.1 drivers

    const auto start = std::chrono::steady_clock::now();
    city_.build(layout_, registry_, root_, collisions_);
    life_.build(layout_);
    sky_.init(city_.materials().glow);
    if (!hud_.init()) log::warn("hud", "the HUD font could not be created");
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    log::info("city", "built in " + std::to_string(static_cast<int>(ms)) + " ms");
}

void Application::startZones() {
    if (options_.noZones) return;
    const std::set<std::string> only(options_.teams.begin(), options_.teams.end());
    for (const TeamInfo& team : registry_.teams()) {
        if (!only.empty() && !only.count(team.id)) continue;
        if (team.folder.empty()) continue;  // no valid team.json: the plot stays empty
        const PlotLayout* plot = layout_.findPlot(team.plotId);
        if (!plot) continue;
        ZoneMaker maker = [&team](std::string& error) -> std::unique_ptr<Zone> {
            const auto& linked = linkedZones();
            const auto found = linked.find(team.id);
            if (found != linked.end()) return found->second();
            error = "the C++ code of " + team.id +
                    " is not built into this program; build the city again and look for compile errors";
            return nullptr;
        };
        zones_.push_back(std::make_unique<ZoneHost>(team, *plot, layout_, collisions_, std::move(maker)));
        zones_.back()->start();
    }
}

void Application::placeCamera() {
    if (options_.camera) {
        const auto& c = *options_.camera;
        camera_.setMode(options_.cameraMode.value_or(CameraMode::Fly));
        camera_.place({c[0], c[1], c[2]}, glm::radians(c[3]), glm::radians(c[4]));
    } else if (!options_.teams.empty()) {
        if (const TeamInfo* t = registry_.findTeam(options_.teams.front())) {
            camera_.setMode(options_.cameraMode.value_or(CameraMode::Orbit));
            for (size_t i = 0; i < layout_.plots.size(); ++i)
                if (layout_.plots[i].id == t->plotId) goToPlot(i);
        }
    } else {
        goToStart();
        if (options_.cameraMode) camera_.setMode(*options_.cameraMode);
    }
    if (options_.tour) {
        tour_.plan(layout_);
        touring_ = true;
    }
}

// --------------------------------------------------------------------------
// Navigation
// --------------------------------------------------------------------------

std::vector<size_t> Application::placesToVisit() const {
    std::vector<size_t> out;
    for (size_t i = 0; i < layout_.plots.size(); ++i)
        if (zoneOnPlot(layout_.plots[i].id)) out.push_back(i);
    if (out.empty())
        for (size_t i = 0; i < layout_.plots.size(); ++i) out.push_back(i);
    return out;
}

ZoneHost* Application::zoneOnPlot(const std::string& plotId) const {
    for (const auto& z : zones_)
        if (z->plot().id == plotId) return z.get();
    return nullptr;
}

void Application::goToPlot(size_t index) {
    const PlotLayout& p = layout_.plots[index];
    const glm::vec2 front = p.front();  // from the plot towards its street
    const glm::vec2 gate = p.gate();
    const float yaw = -glm::radians(p.yawDegrees);  // look back at the plot
    switch (camera_.mode()) {
    case CameraMode::Orbit:
        camera_.orbit({p.center.x, 8.0f, p.center.y}, yaw, -0.45f, 70.0f + p.width * 0.4f);
        break;
    case CameraMode::Fly: {
        const glm::vec2 e = gate + front * 26.0f;
        camera_.place({e.x, 16.0f, e.y}, yaw, -0.32f);
        break;
    }
    default: {
        const glm::vec2 e = gate + front * 7.0f;
        camera_.place({e.x, 0.0f, e.y}, yaw, 0.05f);
        break;
    }
    }
    placeCursor_ = static_cast<int>(index);
    touring_ = false;
    const TeamInfo* t = registry_.teamOnPlot(p.id);
    const auto place = places_.find(p.place);
    toast(p.id + (t ? "  " + placeTitle(*t) + "  (" + t->id + ")"
                    : place != places_.end() ? "  " + place->second.titleEn + "  (topic free)"
                                             : std::string("  free plot")));
}

void Application::goToStart() {
    camera_.setMode(CameraMode::Walk);
    camera_.place(layout_.spawnPosition, glm::radians(layout_.spawnYaw), 0.04f);
    placeCursor_ = -1;
}

const PlotLayout* Application::focusedPlot() const {
    const glm::vec3 f = camera_.focusPoint();
    for (const PlotLayout& p : layout_.plots) {
        const float hw = p.width * 0.5f, hd = p.depth * 0.5f;
        const glm::vec3 local = p.worldToLocal({f.x, 0.0f, f.z});
        // On the plot, or in the half of the street in front of it.
        if (std::fabs(local.x) <= hw + 3.0f && local.z >= -hd - 3.0f && local.z <= hd + 12.0f) return &p;
    }
    return nullptr;
}

// --------------------------------------------------------------------------
// Input
// --------------------------------------------------------------------------

void Application::handleKeys() {
    const auto& in = window_->input();
    const PlotLayout* focus = focusedPlot();
    ZoneHost* focusedZone = focus ? zoneOnPlot(focus->id) : nullptr;
    for (const int key : in.pressed) {
        switch (key) {
        case Key::Escape:
            if (window_->mouseCaptured()) window_->setMouseCaptured(false);
            else if (showHelp_) showHelp_ = false;
            else window_->requestClose();
            break;
        case Key::F1: showHelp_ = !showHelp_; break;
        case Key::F2: showBorders_ = !showBorders_; break;
        case Key::F3: showStats_ = !showStats_; break;
        case Key::F4: showMap_ = !showMap_; break;
        case Key::F5: {
            clock_.cycleSpeed();
            char text[48];
            std::snprintf(text, sizeof text, "clock speed \xC3\x97%.0f", clock_.timeScale());
            toast(text);
            break;
        }
        case Key::F6:
            clock_.togglePause();
            toast(clock_.paused() ? "clock paused" : "clock running");
            break;
        case Key::F7: clock_.addHours(1.0f); break;
        case Key::F8:
            if (focusedZone) focusedZone->reset();
            else
                for (auto& z : zones_) z->reset();
            toast(focusedZone ? "reset " + focusedZone->team().id : "reset all zones");
            break;
        case Key::F9: {
            const CameraMode next = camera_.mode() == CameraMode::Walk  ? CameraMode::Fly
                                  : camera_.mode() == CameraMode::Fly ? CameraMode::Orbit
                                                                      : CameraMode::Walk;
            camera_.setMode(next);
            if (next == CameraMode::Orbit) window_->setMouseCaptured(false);  // orbit drags with the mouse
            touring_ = false;
            toast(std::string("camera: ") + camera_.modeName());
            break;
        }
        case Key::F10: showHud_ = !showHud_; break;
        case Key::F11:
            camera_.setWalkThroughObjects(!camera_.walkThroughObjects());
            toast(camera_.walkThroughObjects() ? "walking through objects: on" : "walking through objects: off (walls stop you)");
            break;
        case Key::F12: screenshotRequested_ = true; break;
        case Key::Tab: {
            const auto stops = placesToVisit();
            if (stops.empty()) break;
            const bool back = window_->keyDown(Key::Shift);
            auto current = std::find(stops.begin(), stops.end(), static_cast<size_t>(std::max(placeCursor_, 0)));
            size_t k = current == stops.end() ? 0 : static_cast<size_t>(current - stops.begin());
            if (placeCursor_ >= 0 || back) k = back ? (k + stops.size() - 1) % stops.size() : (k + 1) % stops.size();
            goToPlot(stops[k]);
            break;
        }
        case Key::Home:
            goToStart();
            touring_ = false;
            break;
        default:
            if (key < 256 && !reservedForCamera(key) && focusedZone) focusedZone->key(key);
            break;
        }
    }
    if (touring_) {
        for (const int k : {int('w'), int('a'), int('s'), int('d'), int(Key::Up), int(Key::Down), int(Key::Left),
                            int(Key::Right), int(' ')})
            if (window_->keyDown(k)) {
                touring_ = false;
                camera_.setMode(CameraMode::Walk);
                toast("tour stopped");
                break;
            }
    }
}

// A click on the map jumps there; any other click while walking or flying
// captures the mouse for looking around, until Esc.
void Application::handleMouse() {
    const auto& in = window_->input();
    if (!in.leftClicked || window_->mouseCaptured()) return;
    const MapFrame& m = minimap_;
    const float x = static_cast<float>(in.mouseX), y = static_cast<float>(in.mouseY);
    if (showHud_ && showMap_ && m.w > 0 && x >= m.x && x <= m.x + m.w && y >= m.y && y <= m.y + m.h) {
        jumpTo(m.minX + (x - m.x) / m.w * (m.maxX - m.minX), m.minZ + (y - m.y) / m.h * (m.maxZ - m.minZ));
        return;
    }
    if (camera_.mode() == CameraMode::Orbit || showHelp_) return;
    window_->setMouseCaptured(true);
    if (!lookHintShown_) {
        toast("move the mouse to look around  \xC2\xB7  Esc frees the mouse");
        lookHintShown_ = true;
    }
}

void Application::jumpTo(float x, float z) {
    touring_ = false;
    switch (camera_.mode()) {
    case CameraMode::Orbit: camera_.orbit({x, 8.0f, z}, camera_.yaw(), camera_.pitch(), 90.0f); break;
    case CameraMode::Fly: camera_.place({x, std::max(camera_.eye().y, 30.0f), z}, camera_.yaw(), camera_.pitch()); break;
    default: {
        // Stand there; inside a building the walker is pushed out of it.
        const glm::vec3 feet = collisions_.move({x, 0.0f, z}, {0.0f, 0.0f});
        camera_.place(feet, camera_.yaw(), 0.0f);
        break;
    }
    }
    toast(locationName());
}

// --------------------------------------------------------------------------
// Rendering
// --------------------------------------------------------------------------

bool Application::visible(const PlotLayout& plot) const {
    // Frustum test of the plot's box in clip space: invisible when all eight
    // corners are outside the same clip plane.
    const Rect r = plot.bounds();
    const glm::mat4 pv = projection_ * view_;
    int outside[6] = {0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 8; ++i) {
        const glm::vec4 c = pv * glm::vec4(i & 1 ? r.x1 : r.x0, i & 2 ? layout_.plotHeight : -layout_.basement, i & 4 ? r.z1 : r.z0, 1);
        outside[0] += c.x < -c.w;
        outside[1] += c.x > c.w;
        outside[2] += c.y < -c.w;
        outside[3] += c.y > c.w;
        outside[4] += c.z < -c.w;
        outside[5] += c.z > c.w;
    }
    return std::none_of(outside, outside + 6, [](int n) { return n == 8; });
}

void Application::setUpLights(const Environment& env) const {
    // Light positions are given while the modelview holds only the camera,
    // so they are fixed in the world.
    glEnable(GL_LIGHTING);
    const GLfloat ambient[4] = {env.ambientColor.r, env.ambientColor.g, env.ambientColor.b, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

    const glm::vec3& d = env.lightDirection;
    const GLfloat sunPosition[4] = {d.x, d.y, d.z, 0.0f};
    const GLfloat sunColor[4] = {env.lightColor.r, env.lightColor.g, env.lightColor.b, 1.0f};
    const GLfloat sunSpecular[4] = {env.lightColor.r * 0.5f, env.lightColor.g * 0.5f, env.lightColor.b * 0.5f, 1.0f};
    const GLfloat none[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, sunPosition);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, sunColor);
    glLightfv(GL_LIGHT0, GL_SPECULAR, sunSpecular);
    glLightfv(GL_LIGHT0, GL_AMBIENT, none);
    glEnable(GL_LIGHT0);

    // Sky fill: soft light from the side opposite the sun, so shadowed
    // faces keep their shape instead of turning flat.
    const glm::vec3 fill = glm::normalize(glm::vec3(-d.x, 0.9f, -d.z));
    const GLfloat fillPosition[4] = {fill.x, fill.y, fill.z, 0.0f};
    const GLfloat fillColor[4] = {env.ambientColor.r * 0.45f, env.ambientColor.g * 0.5f, env.ambientColor.b * 0.6f, 1.0f};
    glLightfv(GL_LIGHT7, GL_POSITION, fillPosition);
    glLightfv(GL_LIGHT7, GL_DIFFUSE, fillColor);
    glLightfv(GL_LIGHT7, GL_SPECULAR, none);
    glLightfv(GL_LIGHT7, GL_AMBIENT, none);
    glEnable(GL_LIGHT7);

    // Aerial perspective: distant things fade into the colour of the horizon.
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP2);
    const GLfloat fogColor[4] = {env.horizonColor.r, env.horizonColor.g, env.horizonColor.b, 1.0f};
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_DENSITY, fogDensity(env));
}

void Application::render() {
    const auto& in = window_->input();
    const int w = std::max(1, in.framebufferWidth), h = std::max(1, in.framebufferHeight);
    glViewport(0, 0, w, h);
    glClearColor(env_.horizonColor.r, env_.horizonColor.g, env_.horizonColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    projection_ = glm::perspective(glm::radians(settings_.fieldOfView), static_cast<float>(w) / h, kNear, kFar);
    view_ = camera_.view();
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projection_));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view_));

    sky_.draw(camera_.eye(), env_, time_);
    setUpLights(env_);
    // What can be seen: inside the view volume, and not so far that the fog has it.
    Viewer viewer;
    viewer.eye = camera_.eye();
    viewer.frustum.set(projection_ * view_);
    viewer.distance = std::min(kFar, fogVisibleDistance(env_));
    city_.beginFrame(viewer);
    city_.drawOpaque(env_, time_);
    life_.draw(env_, viewer);

    visiblePlots_ = 0;
    for (const PlotLayout& p : layout_.plots) {
        if (!visible(p)) continue;
        ++visiblePlots_;
        ZoneHost* zone = zoneOnPlot(p.id);
        if (zone && zone->running()) zone->draw();
        else city_.drawEmptyPlot(p, registry_.teamOnPlot(p.id) != nullptr, time_);
    }

    city_.drawTransparent(view_, env_, time_);
    if (showBorders_) {
        const PlotLayout* f = focusedPlot();
        city_.drawPlotBorders({0.3f, 1.0f, 0.5f}, f ? f->id : std::string());
    }
    if (showHud_) drawOverlay();
}

// --------------------------------------------------------------------------
// Main loop
// --------------------------------------------------------------------------

int Application::run() {
    // The viewer's settings come first: the window needs the multisampling the
    // viewer wants. A scripted run keeps the defaults, so its pictures do not
    // depend on anyone's settings.
    const bool benching = options_.bench > 0;
    const bool scripted = options_.frames > 0 || benching;
    settings_ = scripted ? Settings{} : Settings::load(root_ / "settings.json", true);
    if (options_.msaa) settings_.msaa = *options_.msaa;
    window_ = std::make_unique<PlatformWindow>(options_.width, options_.height, "CityLab", settings_.msaa);
    log::info("gl", std::string(reinterpret_cast<const char*>(glGetString(GL_VERSION))) + " on " +
                        reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    loadCity();
    startZones();
    placeCamera();

    int running = 0;
    for (const auto& z : zones_) running += z->running();
    const std::string title = "CityLab  \xC2\xB7  " + std::to_string(running) + " zones  \xC2\xB7  F1 help";
    window_->setTitle(title.c_str());

    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();
    double fpsTime = 0.0;
    int fpsFrames = 0;
    if (scripted && !options_.screenshotPath.empty()) showHud_ = false;
    if (benching) showHud_ = false;  // the measurement is about the city, not the overlay
    camera_.configure(settings_);
    // Wait for the screen's refresh (no tearing, steady frames) unless a script
    // or --no-vsync wants the frames as fast as possible.
    const bool wantVSync = settings_.vsync && !options_.noVsync && !scripted;
    const bool vsync = window_->setVSync(wantVSync) && wantVSync;
    if (wantVSync && !vsync) log::warn("gl", "this system does not let the program wait for the screen (vsync)");
    log::info("gl", vsync ? "vsync on" : "vsync off");
    FrameTimer timer;
    std::vector<double> frameMs;  // --bench: the wall time of every measured frame
    std::vector<std::vector<std::pair<double, double>>> zoneMs;  // --bench: draw and update time of every zone
    constexpr int kBenchWarmup = 30;

    while (!window_->shouldClose()) {
        window_->pollEvents();
        const auto now = Clock::now();
        const float real = std::chrono::duration<float>(now - last).count();
        last = now;
        // The world moves by a steady step, not by every wobble of the frame time;
        // zones and the city clock get the same step as the camera.
        dt_ = options_.fixedDt ? *options_.fixedDt : benching ? 1.0f / 60.0f : timer.advance(real);
        if (benching && frame_ > kBenchWarmup) {
            frameMs.push_back(real * 1000.0);
            zoneMs.resize(zones_.size());
            for (size_t i = 0; i < zones_.size(); ++i)  // what the zones took in the frame before
                zoneMs[i].push_back({zones_[i]->drawMilliseconds(), zones_[i]->updateMilliseconds()});
        }
        time_ += dt_;
        log::setClock(time_);
        fpsTime += real;
        if (++fpsFrames >= 20) {
            fps_ = static_cast<float>(fpsFrames / std::max(1e-6, fpsTime));
            fpsTime = 0.0;
            fpsFrames = 0;
        }

        handleKeys();
        handleMouse();
        if (touring_) {
            tour_.advance(dt_);
            camera_.setWalkPose(tour_.feet(), tour_.yaw(), -0.03f);
        } else {
            const float paceBefore = camera_.speedScale();
            camera_.update(*window_, dt_, collisions_);
            if (camera_.speedScale() != paceBefore) {  // the wheel changed how fast the viewer goes
                char text[64];
                if (camera_.mode() == CameraMode::Fly) std::snprintf(text, sizeof text, "flying speed %.0f m/s", camera_.flySpeed());
                else std::snprintf(text, sizeof text, "walking speed %.1f m/s", camera_.walkSpeed());
                toast(text);
            }
        }
        clock_.advance(dt_);
        env_ = clock_.environment();
        life_.update(dt_, camera_.feet(), camera_.mode() == CameraMode::Walk);

        const CityFrame city{dt_, clock_.hours(), env_.daylight, clock_.day(), env_.sunDirection, camera_.eye()};
        const PlotLayout* focus = focusedPlot();
        for (auto& z : zones_) z->update(city, focus && focus->id == z->plot().id);

        render();
        ++frame_;
        if (screenshotRequested_) {  // read the back buffer before it is swapped away
            screenshotRequested_ = false;
            const auto file = timestampedScreenshotPath(root_, "citylab");
            const auto& in = window_->input();
            std::string error;
            if (saveScreenshot(file, in.framebufferWidth, in.framebufferHeight, &error))
                toast("saved " + std::filesystem::relative(file, root_).generic_string());
            else
                log::error("screenshot", error);
        }

        if (options_.frames > 0 && frame_ >= options_.frames) {
            if (!options_.screenshotPath.empty()) {
                std::string error;
                const auto& in = window_->input();
                if (!saveScreenshot(options_.screenshotPath, in.framebufferWidth, in.framebufferHeight, &error)) {
                    log::error("screenshot", error);
                    return 1;
                }
                log::info("screenshot", "saved " + options_.screenshotPath);
            }
            break;
        }
        if (benching && frame_ >= kBenchWarmup + options_.bench) {
            reportBench(frameMs, zoneMs);
            break;
        }
        if (benching) glFinish();  // count the time the graphics card needs too
        window_->swapBuffers();
    }
    return 0;
}

// --bench: what a staff member needs to compare two builds at the same view.
void Application::reportBench(std::vector<double> ms, const std::vector<std::vector<std::pair<double, double>>>& zoneMs) {
    if (ms.empty()) return;
    std::sort(ms.begin(), ms.end());
    double sum = 0.0;
    for (const double v : ms) sum += v;
    const auto at = [&](double q) { return ms[std::min(ms.size() - 1, static_cast<size_t>(q * static_cast<double>(ms.size())))]; };
    const double avg = sum / static_cast<double>(ms.size());
    // The slowest 1 % of the frames: what a player feels as stutter.
    const size_t tail = std::max<size_t>(1, ms.size() / 100);
    double tailSum = 0.0;
    for (size_t i = ms.size() - tail; i < ms.size(); ++i) tailSum += ms[i];
    const double low1 = 1000.0 / (tailSum / static_cast<double>(tail));
    std::printf("bench: %zu frames at %dx%d\n", ms.size(), window_->input().framebufferWidth, window_->input().framebufferHeight);
    std::printf("  frame time ms   avg %.2f  min %.2f  p50 %.2f  p95 %.2f  p99 %.2f  max %.2f\n", avg, ms.front(),
                at(0.50), at(0.95), at(0.99), ms.back());
    std::printf("  frames/second   avg %.1f  1%%-low %.1f  worst %.1f\n", 1000.0 / avg, low1, 1000.0 / ms.back());

    // What the city submits for the last frame, counted by OpenGL's feedback
    // mode so culling in the city is included and zones are not.
    const long sky = measureDrawing([&] { sky_.draw(camera_.eye(), env_, time_); }).vertices;
    city_.setFaceCulling(false);  // submitted, not only what faces the camera
    const long opaque = measureDrawing([&] { city_.drawOpaque(env_, time_); }).vertices;
    city_.setFaceCulling(true);
    Viewer viewer;
    viewer.eye = camera_.eye();
    viewer.frustum.set(projection_ * view_);
    viewer.distance = std::min(kFar, fogVisibleDistance(env_));
    const long traffic = measureDrawing([&] { life_.draw(env_, viewer); }).vertices;
    const long effects = measureDrawing([&] { city_.drawTransparent(view_, env_, time_); }).vertices;
    std::printf("  city vertices   %ld  (scenery %ld, sky %ld, trams %ld, water and lights %ld)\n",
                sky + opaque + traffic + effects, opaque, sky, traffic, effects);
    for (size_t i = 0; i < zoneMs.size() && i < zones_.size(); ++i) {
        double draw = 0.0, update = 0.0, drawMax = 0.0;
        for (const auto& [d, u] : zoneMs[i]) {
            draw += d;
            update += u;
            drawMax = std::max(drawMax, d);
        }
        const double n = static_cast<double>(std::max<size_t>(1, zoneMs[i].size()));
        std::printf("  zone %-7s draw() avg %.2f ms (max %.2f), update() avg %.2f ms\n", zones_[i]->team().id.c_str(), draw / n,
                    drawMax, update / n);
    }
    for (const auto& line : city_.statsLines()) std::printf("  %s\n", line.c_str());
    for (const auto& line : life_.statsLines()) std::printf("  %s\n", line.c_str());
    std::fflush(stdout);
}

}  // namespace citylab
