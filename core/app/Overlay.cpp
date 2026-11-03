// The on-screen display: where you are, the city clock, a map, messages,
// help (F1) and statistics (F3). Drawn last, in window pixels.

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "app/Application.h"
#include "engine/Log.h"
#include "framework/PlatformWindow.h"

namespace citylab {
namespace {

const glm::vec4 kText{1.0f, 1.0f, 1.0f, 0.95f};
const glm::vec4 kDim{0.82f, 0.85f, 0.88f, 0.80f};
const glm::vec4 kPanel{0.05f, 0.07f, 0.09f, 0.55f};
const glm::vec4 kGold{1.0f, 0.84f, 0.45f, 1.0f};
const glm::vec4 kWarn{1.0f, 0.80f, 0.35f, 1.0f};
const glm::vec4 kError{1.0f, 0.45f, 0.40f, 1.0f};

glm::vec4 districtColor(const Layout& layout, const std::string& id, float alpha) {
    if (const DistrictLayout* d = layout.findDistrict(id)) return {d->color.r, d->color.g, d->color.b, alpha};
    return {0.92f, 0.92f, 0.90f, alpha};  // a plot outside the districts
}

std::string clockText(float hours) {
    const int minutes = static_cast<int>(hours * 60.0f) % (24 * 60);
    char text[16];
    std::snprintf(text, sizeof text, "%02d:%02d", minutes / 60, minutes % 60);
    return text;
}

}  // namespace

std::string Application::placeTitle(const TeamInfo& team) const {
    if (!team.titleEn.empty()) return team.titleEn;
    const auto found = places_.find(team.place);
    if (found != places_.end() && !found->second.titleEn.empty()) return found->second.titleEn;
    return team.place.empty() ? team.id : team.place;
}

void Application::toast(const std::string& message) {
    toast_ = message;
    toastUntil_ = time_ + 3.0;
}

std::string Application::locationName() const {
    const glm::vec3 f = camera_.focusPoint();
    const float x = f.x, z = f.z;
    const Layout& L = layout_;
    if (const PlotLayout* p = focusedPlot()) {
        const TeamInfo* t = registry_.teamOnPlot(p->id);
        const auto place = places_.find(p->place);
        return p->id + "  \xC2\xB7  " +
               (t ? placeTitle(*t) : place != places_.end() ? place->second.titleEn : std::string("free plot"));
    }
    if (const DistrictLayout* d = L.districtAt(x, z)) return d->titleEn + " district";
    if (L.park.contains(x, z)) {
        if (glm::length(glm::vec2(x, z) - L.tower) < 30.0f) return "City tower";
        if (L.inLake(x, z, 6.0f)) return "The lake";
        if (glm::length(glm::vec2(x, z) - L.fountain) < L.fountainRadius + 12.0f) return "Fountain square";
        return "Central park";
    }
    for (const StreetLayout& s : L.streets) {
        if (!s.rect.contains(x, z)) continue;
        if (s.kind == "tram") return "Tram boulevard";
        return s.alongX() ? (z < L.park.z0 - 100.0f ? "North avenue" : z < L.park.z0 ? "Park avenue" : "South avenue")
                          : (x < -200.0f ? "West avenue" : x > 200.0f ? "East avenue" : "Inner avenue");
    }
    return L.city.contains(x, z) ? "CityLab" : "Outskirts";
}

void Application::drawOverlay() {
    const auto& in = window_->input();
    const float w = static_cast<float>(in.framebufferWidth), h = static_cast<float>(in.framebufferHeight);
    hud_.begin(in.framebufferWidth, in.framebufferHeight);
    drawLocation(w);
    drawClock(w);
    drawMessages(w);
    if (showMap_) drawMinimap(w, h);
    if (showStats_) drawStats();
    if (showHelp_) {
        drawHelp(w, h);
    } else {
        const std::string hint = touring_ ? "touring  \xC2\xB7  press W or an arrow key to walk yourself"
                                          : window_->mouseCaptured()
                                                ? "F1 help  \xC2\xB7  Esc frees the mouse  \xC2\xB7  Shift runs"
                                                : "F1 help  \xC2\xB7  click to look around  \xC2\xB7  Tab next place  \xC2\xB7  F9 walk / fly / orbit";
        hud_.text(14.0f, h - 30.0f, hint, kDim);
    }
    if (time_ < toastUntil_ && !toast_.empty()) {
        const float tw = hud_.textWidth(toast_) + 28.0f;
        const float alpha = static_cast<float>(std::min(1.0, (toastUntil_ - time_) * 2.0));
        hud_.panel((w - tw) * 0.5f, h - 92.0f, tw, 34.0f, {kPanel.r, kPanel.g, kPanel.b, kPanel.a * alpha});
        hud_.text((w - tw) * 0.5f + 14.0f, h - 85.0f, toast_, {1, 1, 1, alpha});
    }
    hud_.end();
}

void Application::drawLocation(float /*width*/) {
    const std::string name = locationName();
    std::string detail;
    glm::vec4 detailColor = kDim;
    if (const PlotLayout* p = focusedPlot()) {
        const DistrictLayout* d = layout_.findDistrict(p->district);
        const TeamInfo* t = registry_.teamOnPlot(p->id);
        detail = (d ? d->titleEn : p->district) + "  \xC2\xB7  " + std::to_string(static_cast<int>(p->width)) +
                 " \xC3\x97 " + std::to_string(static_cast<int>(p->depth)) + " m";
        if (t) detail += "  \xC2\xB7  " + t->id;
        if (const ZoneHost* z = zoneOnPlot(p->id); z && z->state() == ZoneHost::State::Failed) {
            detail = "stopped: " + z->failure() + "   (F8 restarts)";
            detailColor = kError;
        } else if (t && !zoneOnPlot(p->id)) {
            detail += "  \xC2\xB7  not running";
        }
    }
    const float tw = std::max(hud_.textWidth(name, 1.25f), hud_.textWidth(detail)) + 28.0f;
    hud_.panel(12.0f, 12.0f, std::min(tw, 900.0f), detail.empty() ? 40.0f : 62.0f, kPanel);
    hud_.text(26.0f, 19.0f, name, kText, 1.25f);
    if (!detail.empty()) hud_.text(26.0f, 45.0f, detail, detailColor);
}

void Application::drawClock(float width) {
    const std::string time = clockText(clock_.hours());
    char line[64];
    if (clock_.paused()) std::snprintf(line, sizeof line, "day %d  \xC2\xB7  paused", clock_.day() + 1);
    else std::snprintf(line, sizeof line, "day %d  \xC2\xB7  \xC3\x97%.0f", clock_.day() + 1, clock_.timeScale());
    const std::string second = std::string(line) + "  \xC2\xB7  " + camera_.modeName();
    const float tw = std::max(hud_.textWidth(time, 1.6f), hud_.textWidth(second)) + 28.0f;
    hud_.panel(width - tw - 12.0f, 12.0f, tw, 64.0f, kPanel);
    hud_.text(width - tw + 2.0f, 16.0f, time, kGold, 1.6f);
    hud_.text(width - tw + 2.0f, 47.0f, second, kDim);
}

void Application::drawMessages(float width) {
    const auto recent = log::recent(time_, 7.0);
    float y = 90.0f;
    int shown = 0;
    for (auto it = recent.rbegin(); it != recent.rend() && shown < 4; ++it, ++shown) {
        const std::string text = "[" + it->tag + "] " + it->message;
        const float tw = std::min(hud_.textWidth(text) + 24.0f, width - 40.0f);
        const float age = static_cast<float>(time_ - it->time);
        const float alpha = std::clamp(7.0f - age, 0.0f, 1.0f);
        const glm::vec4 color = it->level == log::Level::Error ? kError : kWarn;
        hud_.panel((width - tw) * 0.5f, y, tw, 28.0f, {kPanel.r, kPanel.g, kPanel.b, kPanel.a * alpha});
        hud_.text((width - tw) * 0.5f + 12.0f, y + 4.0f, text, {color.r, color.g, color.b, alpha});
        y += 32.0f;
    }
}

void Application::drawMinimap(float width, float height) {
    // North is up. The frame covers the city.
    const Layout& L = layout_;
    const float minX = L.city.x0 - 10.0f, maxX = L.city.x1 + 10.0f;
    const float minZ = L.city.z0 - 10.0f, maxZ = L.city.z1 + 10.0f;
    const float mapH = std::min(360.0f, height * 0.5f), mapW = mapH * (maxX - minX) / (maxZ - minZ);
    const float x0 = width - mapW - 14.0f, y0 = height - mapH - 14.0f;
    auto mx = [&](float x) { return x0 + (x - minX) / (maxX - minX) * mapW; };
    auto my = [&](float z) { return y0 + (z - minZ) / (maxZ - minZ) * mapH; };
    auto fill = [&](const Rect& r, const glm::vec4& c) {
        const float ax = std::max(mx(r.x0), x0), bx = std::min(mx(r.x1), x0 + mapW);
        const float ay = std::max(my(r.z0), y0), by = std::min(my(r.z1), y0 + mapH);
        if (bx > ax && by > ay) hud_.panel(ax, ay, bx - ax, by - ay, c);
    };
    minimap_ = {x0, y0, mapW, mapH, minX, maxX, minZ, maxZ};

    hud_.panel(x0 - 4.0f, y0 - 4.0f, mapW + 8.0f, mapH + 8.0f, {0.04f, 0.06f, 0.08f, 0.66f});
    fill({minX, maxX, minZ, maxZ}, {0.30f, 0.48f, 0.26f, 0.55f});
    for (const StreetLayout& s : L.streets) fill(s.rect, {0.42f, 0.44f, 0.48f, 0.9f});
    fill(L.park, {0.36f, 0.66f, 0.32f, 0.9f});
    const float rx = L.lakeRadius.x * 0.8f, rz = L.lakeRadius.y * 0.8f;  // the lake, roughly
    fill({L.lake.x - rx, L.lake.x + rx, L.lake.y - rz, L.lake.y + rz}, {0.25f, 0.58f, 0.85f, 0.95f});
    fill({L.lake.x - L.lakeRadius.x, L.lake.x + L.lakeRadius.x, L.lake.y - rz * 0.6f, L.lake.y + rz * 0.6f},
         {0.25f, 0.58f, 0.85f, 0.95f});
    fill({L.city.x0, L.city.x1, L.tramZ - 1.5f, L.tramZ + 1.5f}, {0.95f, 0.85f, 0.35f, 0.9f});
    fill({L.tower.x - 4, L.tower.x + 4, L.tower.y - 4, L.tower.y + 4}, kGold);

    const PlotLayout* focus = focusedPlot();
    for (const PlotLayout& p : L.plots) {
        const Rect r = p.bounds().grown(-1.0f);
        const TeamInfo* t = registry_.teamOnPlot(p.id);
        glm::vec4 c = districtColor(L, p.district, t ? 0.95f : 0.45f);
        if (const ZoneHost* z = zoneOnPlot(p.id); z && !z->running()) c = kError;
        fill(r, c);
        if (focus == &p) {
            const glm::vec4 hi{1, 1, 1, 1};
            hud_.line({mx(r.x0), my(r.z0)}, {mx(r.x1), my(r.z0)}, hi, 2.0f);
            hud_.line({mx(r.x1), my(r.z0)}, {mx(r.x1), my(r.z1)}, hi, 2.0f);
            hud_.line({mx(r.x1), my(r.z1)}, {mx(r.x0), my(r.z1)}, hi, 2.0f);
            hud_.line({mx(r.x0), my(r.z1)}, {mx(r.x0), my(r.z0)}, hi, 2.0f);
        }
    }

    // The viewer: a dot and a short line in the viewing direction.
    const glm::vec3 e = camera_.focusPoint();
    const float px = std::clamp(mx(e.x), x0, x0 + mapW), py = std::clamp(my(e.z), y0, y0 + mapH);
    const glm::vec2 d{std::sin(camera_.yaw()), -std::cos(camera_.yaw())};
    hud_.line({px, py}, {px + d.x * 16.0f, py + d.y * 16.0f}, {1.0f, 0.3f, 0.25f, 1.0f}, 2.5f);
    hud_.panel(px - 3.5f, py - 3.5f, 7.0f, 7.0f, {1.0f, 0.3f, 0.25f, 1.0f});
    hud_.text(x0 + 6.0f, y0 + 2.0f, "N \xE2\x86\x91", kDim, 0.85f);
}

void Application::drawHelp(float width, float height) {
    const char* lines[] = {
        "Moving",
        "  W A S D or arrows      walk  (Shift runs, Ctrl creeps)",
        "  Space                  jump when walking (hold to bunny hop)",
        "  click, then the mouse  look around  (Esc frees the mouse)",
        "  Q / E                  down / up when flying, zoom when orbiting",
        "  mouse wheel            walking and flying speed (zoom when orbiting)",
        "  F9                     walk  \xE2\x86\x92  fly  \xE2\x86\x92  orbit",
        "  F11                    walk through objects on / off",
        "  click on the map       jump there",
        "  Tab / Shift+Tab        next / previous place",
        "  Home                   back to the city gate",
        "  settings.json          mouse speed, field of view, speeds, vsync, multisampling",
        "",
        "The city",
        "  F5 clock speed     F6 pause clock     F7 one hour later",
        "  F2 plot borders    F3 statistics      F4 map",
        "  F8 restart the zone you are at",
        "  F10 hide this display     F12 screenshot     Esc quit (when the mouse is free)",
        "",
        "Your zone",
        "  Other letters and digits go to the zone you are standing at.",
    };
    const int count = static_cast<int>(sizeof lines / sizeof lines[0]);
    float tw = 0.0f;
    for (const char* l : lines) tw = std::max(tw, hud_.textWidth(l));
    const float lh = hud_.lineHeight() + 4.0f;
    const float pw = tw + 60.0f, ph = lh * count + 70.0f;
    const float x = (width - pw) * 0.5f, y = (height - ph) * 0.5f;
    hud_.panel(x, y, pw, ph, {0.04f, 0.06f, 0.08f, 0.82f});
    hud_.text(x + 30.0f, y + 18.0f, "CityLab", kGold, 1.3f);
    for (int i = 0; i < count; ++i) {
        const bool heading = lines[i][0] != ' ' && lines[i][0] != '\0';
        hud_.text(x + 30.0f, y + 56.0f + lh * i, lines[i], heading ? kGold : kText);
    }
}

void Application::drawStats() {
    std::vector<std::string> lines;
    char text[160];
    std::snprintf(text, sizeof text, "%.0f fps  \xC2\xB7  %.1f ms  \xC2\xB7  %d plots in view", fps_,
                  fps_ > 0 ? 1000.0f / fps_ : 0.0f, visiblePlots_);
    lines.push_back(text);
    // What the city sent last frame: the scenery in its lists, not the zones.
    const CityScene::FrameStats& c = city_.frameStats();
    std::snprintf(text, sizeof text,
                  "city: %.0fk vertices  \xC2\xB7  %d of %d chunks  \xC2\xB7  %d trams",
                  static_cast<double>(c.vertices) / 1000.0, c.chunksDrawn, c.chunks, life_.tramsDrawn());
    lines.push_back(text);
    int running = 0, failed = 0;
    for (const auto& z : zones_) {
        running += z->running();
        failed += z->state() == ZoneHost::State::Failed;
    }
    std::snprintf(text, sizeof text, "%d zones running, %d stopped  \xC2\xB7  %zu colliders  \xC2\xB7  %d warnings, %d errors",
                  running, failed, collisions_.size(), log::warningCount(), log::errorCount());
    lines.push_back(text);
    std::vector<const ZoneHost*> sorted;
    for (const auto& z : zones_) sorted.push_back(z.get());
    std::sort(sorted.begin(), sorted.end(),
              [](const ZoneHost* a, const ZoneHost* b) { return a->drawMilliseconds() > b->drawMilliseconds(); });
    for (size_t i = 0; i < sorted.size() && i < 8; ++i) {
        const ZoneHost* z = sorted[i];
        std::snprintf(text, sizeof text, "%s  %-5s  draw %.2f ms  update %.2f ms  textures %d  colliders %d",
                      z->team().id.c_str(), z->plot().id.c_str(), z->drawMilliseconds(), z->updateMilliseconds(),
                      z->textureCount(), z->colliderCount());
        lines.push_back(text);
    }
    float tw = 0.0f;
    for (const auto& l : lines) tw = std::max(tw, hud_.textWidth(l));
    const float lh = hud_.lineHeight() + 2.0f;
    hud_.panel(12.0f, 84.0f, tw + 28.0f, lh * lines.size() + 16.0f, kPanel);
    for (size_t i = 0; i < lines.size(); ++i)
        hud_.text(26.0f, 92.0f + lh * i, lines[i], i < 3 ? kText : kDim);
}

}  // namespace citylab
