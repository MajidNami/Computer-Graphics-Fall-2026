// citylab --check: measures one zone the way the graders will, so a team
// knows before the deadline whether its plot keeps the rules.

#include "app/Checker.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>

#include <glm/gtc/type_ptr.hpp>

#include "app/Application.h"
#include "city/Frame.h"
#include "engine/Json.h"
#include "engine/Log.h"
#include "engine/Screenshot.h"
#include "framework/PlatformWindow.h"

namespace citylab {
namespace {

constexpr long kVertexBudget = 100000;           // per frame, after display lists are expanded (triangle corners)
constexpr std::uintmax_t kFolderBudget = 15u << 20;  // bytes in the team folder
constexpr double kSlowDrawMs = 6.0;               // average draw() time that earns a warning
constexpr float kCheckDt = 1.0f / 30.0f;

enum class Status { Pass, Warn, Fail, Info };

struct Check {
    std::string id, title;
    Status status;
    std::string detail;
};

const char* statusName(Status s) {
    switch (s) {
    case Status::Pass: return "PASS";
    case Status::Warn: return "WARN";
    case Status::Fail: return "FAIL";
    default: return "INFO";
    }
}

std::string number(long n) {  // 12480 -> "12 480"
    std::string s = std::to_string(n), out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i > 0 && (s.size() - i) % 3 == 0) out += ' ';
        out += s[i];
    }
    return out;
}

std::string fixed(double v, int digits = 1) {
    char text[32];
    std::snprintf(text, sizeof text, "%.*f", digits, v);
    return text;
}

}  // namespace

struct Application::Capture {
    long vertices = 0, primitives = 0, outside = 0;
    bool overflow = false;
    glm::vec3 min{1e9f}, max{-1e9f};
    std::vector<glm::vec3> points;
};

Application::Capture Application::capture(ZoneHost& zone, ZoneHost::Probe* probe) {
    // Feedback mode: OpenGL returns the transformed vertices instead of
    // drawing them. With an orthographic box of +-1000 m and an identity
    // modelview, they convert straight back to plot-local metres.
    static std::vector<GLfloat> buffer(6u << 20);
    const auto& in = window_->input();
    const float w = static_cast<float>(std::max(1, in.framebufferWidth));
    const float h = static_cast<float>(std::max(1, in.framebufferHeight));
    const float range = 1000.0f;
    glViewport(0, 0, in.framebufferWidth, in.framebufferHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-range, range, -range, range, -range, range);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glFeedbackBuffer(static_cast<GLsizei>(buffer.size()), GL_3D, buffer.data());
    glRenderMode(GL_FEEDBACK);
    zone.draw(true, probe);
    const GLint count = glRenderMode(GL_RENDER);

    Capture c;
    if (count < 0) {
        c.overflow = true;
        return c;
    }
    const float hw = zone.plot().width * 0.5f + 0.05f, hd = zone.plot().depth * 0.5f + 0.05f;
    const float top = layout_.plotHeight + 0.05f, bottom = -layout_.basement - 0.05f;
    auto vertex = [&](GLint& i) {
        const glm::vec3 p{(2.0f * buffer[i] / w - 1.0f) * range, (2.0f * buffer[i + 1] / h - 1.0f) * range,
                          -(2.0f * buffer[i + 2] - 1.0f) * range};
        i += 3;
        ++c.vertices;
        c.min = glm::min(c.min, p);
        c.max = glm::max(c.max, p);
        if (std::fabs(p.x) > hw || std::fabs(p.z) > hd || p.y > top || p.y < bottom) ++c.outside;
        c.points.push_back(p);
    };
    for (GLint i = 0; i < count;) {
        const GLint token = static_cast<GLint>(buffer[i++]);
        ++c.primitives;
        switch (token) {
        case GL_POINT_TOKEN:
        case GL_BITMAP_TOKEN:
        case GL_DRAW_PIXEL_TOKEN:
        case GL_COPY_PIXEL_TOKEN: vertex(i); break;
        case GL_LINE_TOKEN:
        case GL_LINE_RESET_TOKEN:
            vertex(i);
            vertex(i);
            break;
        case GL_POLYGON_TOKEN: {
            const GLint n = static_cast<GLint>(buffer[i++]);
            for (GLint k = 0; k < n; ++k) vertex(i);
            break;
        }
        case GL_PASS_THROUGH_TOKEN: ++i; break;
        default: i = count; break;  // unknown token: stop rather than misread
        }
    }
    return c;
}

void Application::runFrames(int count, float dt) {
    for (int k = 0; k < count; ++k) {
        window_->pollEvents();
        time_ += dt;
        log::setClock(time_);
        clock_.advance(dt);
        env_ = clock_.environment();
        const CityFrame city{dt, clock_.hours(), env_.daylight, clock_.day(), env_.sunDirection, camera_.eye()};
        for (auto& z : zones_) z->update(city, true);
        render();
        window_->swapBuffers();
    }
}

bool Application::shoot(const std::filesystem::path& file) {
    render();
    const auto& in = window_->input();
    std::string error;
    const bool ok = saveScreenshot(file, in.framebufferWidth, in.framebufferHeight, &error);
    if (!ok) log::error("check", error);
    window_->swapBuffers();
    return ok;
}

int Application::check() {
    const std::string id = options_.checkTeam;
    std::vector<Check> checks;
    auto add = [&](const std::string& key, const std::string& title, Status s, const std::string& detail) {
        checks.push_back({key, title, s, detail});
    };
    Json metrics = Json::object();
    Json shots = Json::array();
    const auto outDir = root_ / "build" / "check";
    std::filesystem::create_directories(outDir / id);

    const TeamInfo* team = registry_.findTeam(id);
    if (!team) add("folder", "team folder", Status::Fail, "teams/" + id + " was not found or team.json is invalid");

    if (team) {
        // ---- Files ----------------------------------------------------------
        if (!team->registered)
            add("registry", "registered", Status::Warn,
                "not in city/registry.json yet; checked on free plot " + team->plotId);
        std::uintmax_t bytes = 0;
        int files = 0;
        std::vector<std::string> unwanted;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(team->folder)) {
            if (!entry.is_regular_file()) continue;
            ++files;
            bytes += entry.file_size();
            const std::string ext = entry.path().extension().string();
            for (const char* bad : {".exe", ".dll", ".so", ".dylib", ".o", ".obj", ".class", ".pyc", ".zip", ".rar",
                                    ".7z", ".psd", ".blend"})
                if (ext == bad) unwanted.push_back(std::filesystem::relative(entry.path(), team->folder).generic_string());
        }
        metrics.set("folder_bytes", static_cast<double>(bytes));
        metrics.set("files", files);
        const std::string size = fixed(bytes / 1048576.0) + " MB in " + std::to_string(files) + " files";
        add("folder", "folder size", bytes > kFolderBudget ? Status::Fail : Status::Pass,
            size + " (limit " + std::to_string(kFolderBudget >> 20) + " MB)");
        if (!unwanted.empty())
            add("files", "no build products", Status::Warn,
                "remove " + unwanted.front() + (unwanted.size() > 1 ? " and " + std::to_string(unwanted.size() - 1) + " more" : "") +
                    " (build outputs, archives and editor files do not belong in the repository)");
        if (team->title.empty() || team->titleEn.empty())
            add("title", "names on the sign", Status::Warn, "fill in \"title\" (Persian) and \"title_en\" in team.json");
    }

    // ---- Start ----------------------------------------------------------------
    ZoneHost* zone = nullptr;
    if (team) {
        window_ = std::make_unique<PlatformWindow>(options_.width, options_.height, ("CityLab check " + id).c_str());
        window_->setVSync(false);  // the check runs as fast as it can
        loadCity();
        startZones();
        zone = zoneOnPlot(team->plotId);
        if (!zone || !zone->running()) {
            add("start", "starts", Status::Fail, zone ? zone->failure() : "the zone did not start");
            zone = nullptr;
        }
    }

    if (zone) {
        const PlotLayout& plot = zone->plot();
        clock_.setHours(12.0f);
        camera_.setMode(CameraMode::Orbit);
        goToPlot(static_cast<size_t>(&plot - layout_.plots.data()));
        showHud_ = false;

        // Run for a while and time it.
        double drawSum = 0.0, drawMax = 0.0, updateSum = 0.0;
        const int frames = 90;
        for (int k = 0; k < frames && zone->running(); ++k) {
            runFrames(1, kCheckDt);
            drawSum += zone->drawMilliseconds();
            drawMax = std::max(drawMax, zone->drawMilliseconds());
            updateSum += zone->updateMilliseconds();
        }
        if (!zone->running()) {
            add("start", "runs", Status::Fail, zone->failure());
        } else {
            add("start", "starts and runs", Status::Pass, std::to_string(frames) + " frames without exceptions");
            const double drawAvg = drawSum / frames, updateAvg = updateSum / frames;
            metrics.set("draw_ms_avg", drawAvg);
            metrics.set("draw_ms_max", drawMax);
            metrics.set("update_ms_avg", updateAvg);
            add("speed", "fast enough", drawAvg > kSlowDrawMs ? Status::Warn : Status::Pass,
                "draw() " + fixed(drawAvg, 2) + " ms on average, " + fixed(drawMax, 2) + " ms at most" +
                    (drawAvg > kSlowDrawMs ? " (use display lists for static parts)" : ""));

            // Geometry: budget, bounds, animation.
            ZoneHost::Probe dayProbe;
            const Capture first = capture(*zone, &dayProbe);
            runFrames(30, kCheckDt);
            const Capture second = capture(*zone, nullptr);
            metrics.set("vertices", static_cast<double>(first.vertices));
            metrics.set("primitives", static_cast<double>(first.primitives));
            if (first.overflow || first.vertices > kVertexBudget) {
                add("budget", "vertex budget", Status::Fail,
                    (first.overflow ? std::string("far more than ") : number(first.vertices) + " vertices, more than ") +
                        number(kVertexBudget) + " per frame; simplify shapes or use fewer slices");
            } else {
                add("budget", "vertex budget", Status::Pass,
                    number(first.vertices) + " of " + number(kVertexBudget) + " vertices per frame");
            }
            if (first.vertices == 0) {
                add("draws", "draws something", Status::Fail, "draw() produced no geometry");
            } else if (first.outside > 0) {
                char box[160];
                std::snprintf(box, sizeof box, "x %.1f..%.1f, y %.1f..%.1f, z %.1f..%.1f", first.min.x, first.max.x,
                              first.min.y, first.max.y, first.min.z, first.max.z);
                const PlotLayout& pl = zone->plot();
                add("bounds", "stays inside the plot", Status::Warn,
                    number(first.outside) + " vertices outside the plot volume (geometry reaches " + box +
                        "; the city cuts it off at x +-" + fixed(pl.width * 0.5f, 0) + ", z +-" +
                        fixed(pl.depth * 0.5f, 0) + ", y " + fixed(-layout_.basement, 0) + ".." +
                        fixed(layout_.plotHeight, 0) + ")");
            } else {
                const PlotLayout& pl = zone->plot();
                add("bounds", "stays inside the plot", Status::Pass,
                    "every vertex is inside " + fixed(pl.width, 0) + " x " + fixed(pl.depth, 0) + " m, up to " +
                        fixed(layout_.plotHeight, 0) + " m");
            }
            bool moving = first.vertices != second.vertices;
            for (size_t i = 0; !moving && i < first.points.size(); ++i)
                moving = glm::length(first.points[i] - second.points[i]) > 0.01f;
            metrics.set("moving_geometry", moving);
            add("animation", "moving geometry", Status::Info,
                moving ? "geometry moves over time" : "no moving geometry found in one second (colour or texture "
                                                      "animation is not detected)");

            // Lights, by day and at night.
            clock_.setHours(21.5f);
            runFrames(3, kCheckDt);
            ZoneHost::Probe nightProbe;
            capture(*zone, &nightProbe);
            metrics.set("lights_day", dayProbe.extraLights);
            metrics.set("lights_night", nightProbe.extraLights);
            const int highest = std::max(dayProbe.highestLight, nightProbe.highestLight);
            if (dayProbe.sunChanged || nightProbe.sunChanged)
                add("lights", "lights", Status::Warn, "draw() changes GL_LIGHT0, which is the sun of the whole city");
            else if (highest > ZoneBudget::kLights)
                add("lights", "lights", Status::Warn,
                    "uses GL_LIGHT" + std::to_string(highest) + "; zones may use GL_LIGHT1 to GL_LIGHT4");
            else
                add("lights", "lights", Status::Pass,
                    std::to_string(dayProbe.extraLights) + " own lights by day, " +
                        std::to_string(nightProbe.extraLights) + " at night");

            add("textures", "textures", Status::Pass,
                std::to_string(zone->textureCount()) + " of " + std::to_string(ZoneBudget::kTextures) + " textures, " +
                    std::to_string(zone->colliderCount()) + " colliders");

            // The gate: a visitor walks in from the street along three lines
            // and has to get 6 m past the frontage.
            const GroundFrame ground{plot.center, plot.yawDegrees};
            const float frontage = plot.depth * 0.5f;
            std::string blocked;
            for (const float x : {-2.5f, 0.0f, 2.5f}) {
                glm::vec3 feet = ground.toWorld(glm::vec3(x, 0.0f, frontage + 4.0f));
                const glm::vec3 goal = ground.toWorld(glm::vec3(x, 0.0f, frontage - 6.0f));
                for (int step = 0; step < 300; ++step) {
                    const glm::vec2 left{goal.x - feet.x, goal.z - feet.z};
                    const float distance = glm::length(left);
                    if (distance < 0.2f) break;
                    feet = collisions_.move(feet, left * (std::min(0.15f, distance) / distance));
                }
                if (glm::length(glm::vec2(goal.x - feet.x, goal.z - feet.z)) > 0.5f)
                    blocked += (blocked.empty() ? "x = " : ", ") + fixed(x, 1);
            }
            if (blocked.empty())
                add("gate", "the gate is open", Status::Pass, "a visitor can walk in through the " + fixed(layout_.gateWidth, 0) + " m gate");
            else
                add("gate", "the gate is open", Status::Fail,
                    "a visitor walking in from the street is stopped (" + blocked +
                        "); keep the gate clear of colliders up to 3 m");

            // Screenshots for the report.
            const float a = glm::radians(plot.yawDegrees);
            const glm::vec2 front = plot.front();
            const glm::vec3 target{plot.center.x, 8.0f, plot.center.y};
            const float wide = plot.width / 60.0f;  // double plots need a wider view
            struct Shot {
                const char* name;
                float hours;
                int mode;  // 0 orbit, 1 walk
                float yaw, pitch, distance;
            };
            const Shot list[] = {{"front", 11.0f, 0, -a, -0.36f, 95.0f * wide},
                                 {"aerial", 13.0f, 0, -a + 0.75f, -0.95f, 125.0f * wide},
                                 {"street", 16.5f, 1, -a, 0.08f, 0.0f},
                                 {"night", 21.5f, 0, -a - 0.5f, -0.30f, 90.0f * wide}};
            for (const Shot& s : list) {
                clock_.setHours(s.hours);
                if (s.mode == 0) {
                    camera_.orbit(target, s.yaw, s.pitch, s.distance);
                } else {
                    const glm::vec2 e = plot.gate() + front * 7.0f;
                    camera_.setWalkPose({e.x, 0.0f, e.y}, s.yaw, s.pitch);
                }
                runFrames(4, kCheckDt);
                const auto file = outDir / id / (std::string(s.name) + ".png");
                if (shoot(file)) shots.push(std::filesystem::relative(file, root_).generic_string());
            }
        }

        // Every mistake the host repaired during the run.
        for (const std::string& w : zone->warnings()) add("host", "OpenGL usage", Status::Warn, w);
        if (zone->warnings().empty())
            add("host", "OpenGL usage", Status::Pass, "balanced glBegin/glEnd, push/pop and display lists");
    }

    // ---- Report ---------------------------------------------------------------
    int failures = 0, warnings = 0;
    Json list = Json::array();
    for (const Check& c : checks) {
        failures += c.status == Status::Fail;
        warnings += c.status == Status::Warn;
        Json item = Json::object();
        item.set("id", c.id);
        item.set("title", c.title);
        item.set("status", std::string(statusName(c.status)));
        item.set("detail", c.detail);
        list.push(item);
    }
    Json report = Json::object();
    report.set("team", id);
    report.set("plot", team ? team->plotId : std::string());
    report.set("result", failures ? "fail" : "pass");
    report.set("failures", failures);
    report.set("warnings", warnings);
    report.set("checks", list);
    report.set("metrics", metrics);
    report.set("screenshots", shots);
    const auto reportFile = outDir / (id + ".json");
    std::ofstream(reportFile) << report.dump(2) << "\n";

    std::printf("\nCityLab check  %s  plot %s\n\n", id.c_str(), team ? team->plotId.c_str() : "-");
    for (const Check& c : checks) std::printf("  %s  %-22s %s\n", statusName(c.status), c.title.c_str(), c.detail.c_str());
    std::printf("\nresult: %s (%d failed, %d warnings)\nreport: %s\n", failures ? "FAIL" : "PASS", failures, warnings,
                std::filesystem::relative(reportFile, root_).generic_string().c_str());
    if (shots.size()) std::printf("screenshots: build/check/%s/\n", id.c_str());
    return failures ? 1 : 0;
}

int runChecker(const Options& options, const std::filesystem::path& root) {
    Options o = options;
    o.teams = {o.checkTeam};
    o.timeScale = CityClock::kDefaultScale;
    o.fixedDt = kCheckDt;
    Application app(o, root);
    return app.check();
}

}  // namespace citylab
