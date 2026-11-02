#include "city/Layout.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

#include "engine/Json.h"

namespace citylab {
namespace {

Rect readRect(const Json& j, const std::string& what) {
    if (j.size() != 4) throw std::runtime_error("layout.json: '" + what + "' needs [xMin, xMax, zMin, zMax]");
    Rect r{j[0].f(), j[1].f(), j[2].f(), j[3].f()};
    if (r.x1 < r.x0 || r.z1 < r.z0) throw std::runtime_error("layout.json: '" + what + "' has min > max");
    return r;
}

glm::vec2 readPoint(const Json& j, glm::vec2 fallback = glm::vec2(0.0f)) {
    if (j.size() != 2) return fallback;
    return {j[0].f(), j[1].f()};
}

}  // namespace

Rect PlotLayout::bounds() const {
    // Plots are rotated in steps of 90 degrees, so the footprint stays axis aligned.
    const float a = glm::radians(yawDegrees);
    const float ex = std::fabs(std::cos(a)) * width * 0.5f + std::fabs(std::sin(a)) * depth * 0.5f;
    const float ez = std::fabs(std::sin(a)) * width * 0.5f + std::fabs(std::cos(a)) * depth * 0.5f;
    return {center.x - ex, center.x + ex, center.y - ez, center.y + ez};
}

glm::vec2 PlotLayout::front() const {
    const float a = glm::radians(yawDegrees);
    return {std::round(std::sin(a)), std::round(std::cos(a))};
}

glm::vec2 PlotLayout::gate() const { return center + front() * (depth * 0.5f); }

glm::mat4 PlotLayout::toWorld() const {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(center.x, 0.0f, center.y));
    return glm::rotate(m, glm::radians(yawDegrees), glm::vec3(0, 1, 0));
}

glm::vec3 PlotLayout::localToWorld(const glm::vec3& p) const { return glm::vec3(toWorld() * glm::vec4(p, 1.0f)); }

glm::vec3 PlotLayout::worldToLocal(const glm::vec3& p) const {
    return glm::vec3(glm::inverse(toWorld()) * glm::vec4(p, 1.0f));
}

glm::vec3 PlotLayout::directionToLocal(const glm::vec3& d) const {
    return glm::vec3(glm::inverse(toWorld()) * glm::vec4(d, 0.0f));
}

const PlotLayout* Layout::findPlot(const std::string& id) const {
    for (const auto& p : plots)
        if (p.id == id) return &p;
    return nullptr;
}

const PlotLayout* Layout::plotOfPlace(const std::string& place) const {
    if (place.empty()) return nullptr;
    for (const auto& p : plots)
        if (p.place == place) return &p;
    return nullptr;
}

const DistrictLayout* Layout::findDistrict(const std::string& id) const {
    for (const auto& d : districts)
        if (d.id == id) return &d;
    return nullptr;
}

const PlotLayout* Layout::plotAt(float x, float z, float margin) const {
    for (const auto& p : plots)
        if (p.bounds().contains(x, z, margin)) return &p;
    return nullptr;
}

const DistrictLayout* Layout::districtAt(float x, float z) const {
    for (const auto& d : districts)
        if (d.rect.contains(x, z, sidewalk)) return &d;
    return nullptr;
}

bool Layout::inLake(float x, float z, float margin) const {
    const float dx = (x - lake.x) / (lakeRadius.x + margin), dz = (z - lake.y) / (lakeRadius.y + margin);
    return dx * dx + dz * dz <= 1.0f;
}

std::vector<Rect> Layout::junctions() const {
    std::vector<Rect> alongX, alongZ, out;
    for (const StreetLayout& s : streets) {
        const Rect c = s.rect.grown(-sidewalk);
        (s.alongX() ? alongX : alongZ).push_back(c);
    }
    for (const Rect& h : alongX)
        for (const Rect& v : alongZ)
            if (h.overlaps(v))
                out.push_back({std::max(h.x0, v.x0), std::min(h.x1, v.x1), std::max(h.z0, v.z0), std::min(h.z1, v.z1)});
    return out;
}

std::vector<glm::vec3> Layout::tramStops() const {
    const float north = park.z1 + 2.3f, south = park.z1 + 27.7f;
    // Where a shelter stands along the boulevard is a share of the park's half width, so the
    // shelters keep their places between the avenues when the city grows or shrinks.
    const float middle = (park.x0 + park.x1) * 0.5f, half = (park.x1 - park.x0) * 0.5f;
    const auto at = [&](float share) { return std::round((middle + share * half) / 5.0f) * 5.0f; };
    return {{at(-0.667f), north, 0.0f}, {at(0.133f), north, 0.0f}, {at(0.667f), north, 0.0f},
            {at(-0.65f), south, 180.0f}, {at(0.6f), south, 180.0f}};
}

Layout Layout::load(const std::filesystem::path& file) {
    const Json j = Json::parseFile(file);
    if (j["version"].f(1) < 2.0f)
        throw std::runtime_error("layout.json: version 1 plan; run tools/staff/gen_layout.py again");
    Layout L;
    L.lotWidth = j["lot"]["width"].f(60);
    L.lotDepth = j["lot"]["depth"].f(80);
    L.plotHeight = j["lot"]["height"].f(40);
    L.basement = j["lot"]["basement"].f(5);
    L.gateWidth = j["lot"]["gate"].f(10);

    const Json& spawn = j["spawn"];
    L.spawnPosition = {spawn["position"][0].f(), spawn["position"][1].f(), spawn["position"][2].f()};
    L.spawnYaw = spawn["yaw"].f(180);
    L.city = readRect(j["city"], "city");
    L.sidewalk = j["sidewalk"].f(5);
    for (const auto& s : j["streets"].items())
        L.streets.push_back({s["id"].str(), s["kind"].str("avenue"), readRect(s["rect"], "streets.rect")});

    const Json& park = j["park"];
    L.park = readRect(park["rect"], "park.rect");
    L.tower = readPoint(park["tower"]["center"], L.tower);
    L.towerHeight = park["tower"]["height"].f(120);
    L.lake = readPoint(park["lake"]["center"], L.lake);
    L.lakeRadius = readPoint(park["lake"]["radius"], L.lakeRadius);
    L.fountain = readPoint(park["fountain"]["center"], L.fountain);
    L.fountainRadius = park["fountain"]["radius"].f(12);
    L.tramZ = j["tram"]["z"].f(75);
    L.tramWidth = j["tram"]["width"].f(6);

    for (const auto& d : j["districts"].items()) {
        DistrictLayout D;
        D.id = d["id"].str();
        D.code = d["code"].str();
        D.title = d["title"].str();
        D.titleEn = d["title_en"].str();
        D.rect = readRect(d["rect"], "districts." + D.id + ".rect");
        D.color = {d["color"][0].f(1), d["color"][1].f(1), d["color"][2].f(1)};
        for (const auto& p : d["plots"].items()) D.plots.push_back(p.str());
        L.districts.push_back(std::move(D));
    }
    for (const auto& p : j["plots"].items()) {
        PlotLayout P;
        P.id = p["id"].str();
        P.district = p["district"].str();
        P.place = p["place"].str();
        P.center = readPoint(p["center"]);
        P.yawDegrees = p["yaw"].f();
        P.width = p["width"].f(L.lotWidth);
        P.depth = p["depth"].f(L.lotDepth);
        for (const auto& lot : p["lots"].items()) P.lots.push_back(lot.str());
        if (P.id.empty()) throw std::runtime_error("layout.json: a plot has no id");
        if (std::fmod(std::fabs(P.yawDegrees), 90.0f) > 1e-3f)
            throw std::runtime_error("layout.json: plot " + P.id + " yaw must be a multiple of 90");
        if (P.width < 10.0f || P.depth < 10.0f) throw std::runtime_error("layout.json: plot " + P.id + " is too small");
        L.plots.push_back(std::move(P));
    }
    if (L.plots.empty()) throw std::runtime_error("layout.json: no plots");
    return L;
}

}  // namespace citylab
