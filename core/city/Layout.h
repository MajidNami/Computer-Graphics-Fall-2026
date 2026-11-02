#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace citylab {

// An axis-aligned rectangle on the ground: x in [x0, x1], z in [z0, z1].
struct Rect {
    float x0 = 0, x1 = 0, z0 = 0, z1 = 0;
    float width() const { return x1 - x0; }
    float depth() const { return z1 - z0; }
    glm::vec2 center() const { return {(x0 + x1) * 0.5f, (z0 + z1) * 0.5f}; }
    bool contains(float x, float z, float margin = 0.0f) const {
        return x >= x0 - margin && x <= x1 + margin && z >= z0 - margin && z <= z1 + margin;
    }
    Rect grown(float by) const { return {x0 - by, x1 + by, z0 - by, z1 + by}; }
    bool overlaps(const Rect& o) const { return x0 < o.x1 && o.x0 < x1 && z0 < o.z1 && o.z0 < z1; }
};

// One plot: one lot (80 x 100 m) or two neighbouring lots merged (160 x 100 m).
struct PlotLayout {
    std::string id;        // "CU-3", the first lot of the plot
    std::string district;  // "culture", "commercial", ...
    std::string place;     // topic id from city/places.json, empty for a free plot
    glm::vec2 center{0};   // world x, z of the plot origin
    float yawDegrees = 0;  // rotation about +y that turns local +z (the gate) towards the street
    float width = 60;      // frontage, along local x
    float depth = 80;      // along local z, from the back (-depth/2) to the gate (+depth/2)
    std::vector<std::string> lots;

    Rect bounds() const;          // world rectangle covered by the plot
    glm::vec2 front() const;      // unit vector from the plot towards its street
    glm::vec2 gate() const;       // world x, z of the middle of the gate
    glm::mat4 toWorld() const;    // T(center) * Ry(yaw)
    glm::vec3 localToWorld(const glm::vec3& p) const;
    glm::vec3 worldToLocal(const glm::vec3& p) const;
    glm::vec3 directionToLocal(const glm::vec3& d) const;
};

struct DistrictLayout {
    std::string id, code, title, titleEn;
    Rect rect;              // the block, kerb lines excluded
    glm::vec3 color{1.0f};  // signs and markings of the district
    std::vector<std::string> plots;
};

struct StreetLayout {
    std::string id, kind;  // kind: "avenue" or "tram"
    Rect rect;             // the whole band, sidewalks included
    bool alongX() const { return rect.width() > rect.depth(); }
};

// city/layout.json in memory. Everything that places geometry in the city
// reads these numbers, so the plan can change without touching code.
struct Layout {
    static constexpr float kExitLength = 900.0f;  // how far the avenues go on out of the city

    float plotHeight = 40, basement = 5, gateWidth = 10;
    float lotWidth = 60, lotDepth = 80;
    glm::vec3 spawnPosition{0, 0, -345};
    float spawnYaw = 180;
    Rect city;
    float sidewalk = 5;
    std::vector<StreetLayout> streets;
    Rect park;
    glm::vec2 tower{0, -40};
    float towerHeight = 120;
    glm::vec2 lake{-195, -40}, lakeRadius{75, 55};
    glm::vec2 fountain{170, -40};
    float fountainRadius = 12;
    float tramZ = 75, tramWidth = 6;
    std::vector<DistrictLayout> districts;
    std::vector<PlotLayout> plots;

    const PlotLayout* findPlot(const std::string& id) const;
    const PlotLayout* plotOfPlace(const std::string& place) const;
    const DistrictLayout* findDistrict(const std::string& id) const;
    // The plot or district a point is in; nullptr outside.
    const PlotLayout* plotAt(float x, float z, float margin = 0.0f) const;
    const DistrictLayout* districtAt(float x, float z) const;
    // Inside the lake's ellipse, grown by `margin` metres.
    bool inLake(float x, float z, float margin = 0.0f) const;
    // The carriageways where an avenue along x crosses one along z.
    std::vector<Rect> junctions() const;
    // The tram stops on the tram boulevard: x, z of the shelter and its yaw
    // in degrees (0: the shelter opens towards +z).
    std::vector<glm::vec3> tramStops() const;

    // Throws JsonError / std::runtime_error with a readable message.
    static Layout load(const std::filesystem::path& file);
};

}  // namespace citylab
