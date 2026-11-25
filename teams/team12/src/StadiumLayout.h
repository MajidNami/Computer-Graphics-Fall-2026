#pragma once

#include <array>
#include <vector>

#include <citylab/Context.h>

namespace team12 {

// Rectangles use (x, z) in metres. Axis-aligned in their owning coordinate system.
struct Footprint {
    glm::vec2 center;
    glm::vec2 size;
};

struct SeatPlacement {
    int id;  // Nominal slot ID, independent of excluded corridors.
    glm::vec3 position;  // Stadium-local center of the seat surface, in metres.
    float yawDegrees;  // Local +Z faces the pitch after a rotation about +Y.
    int tier;
    int row;
    int sector;  // Increasing ellipse angle from +X toward +Z.
    bool occupancyEligible;
};

struct SeatingTier {
    int rows;
    float rowDepth;
    float firstRowHeight;
    float risePerRow;
    glm::vec2 firstRowRadii;
};

struct ShellOpening {
    float angle;      // Ellipse parameter, in radians.
    float halfAngle;
    float clearHeight;
};

namespace palette {
inline constexpr glm::vec3 ground{0.30f, 0.38f, 0.26f};
inline constexpr glm::vec3 paving{0.67f, 0.64f, 0.59f};
inline constexpr glm::vec3 facadeRed{0.73f, 0.10f, 0.16f};
inline constexpr glm::vec3 facadeWhite{0.90f, 0.89f, 0.86f};
inline constexpr glm::vec3 turf{0.14f, 0.41f, 0.19f};
inline constexpr glm::vec3 concrete{0.46f, 0.47f, 0.49f};
inline constexpr glm::vec3 steel{0.24f, 0.27f, 0.31f};
}  // namespace palette

namespace vertexBudget {
inline constexpr int shell = 14000;
inline constexpr int roof = 7000;
inline constexpr int seating = 20000;
inline constexpr int landscape = 8000;
inline constexpr int pitch = 6000;
inline constexpr int crowd = 10000;
inline constexpr int playerFacilities = 8000;
inline constexpr int commercialBuildings = 7000;
inline constexpr int target = 80000;
inline constexpr int limit = 100000;
static_assert(shell + roof + seating + landscape + pitch + crowd + playerFacilities + commercialBuildings == target);
static_assert(target < limit);
}  // namespace vertexBudget

struct StadiumLayout {
    citylab::PlotInfo plot;
    glm::vec3 stadiumCenter;
    float stadiumYawDegrees;
    glm::vec2 outerSize;
    glm::vec2 innerSize;
    float roofHeight;
    float shellSpringHeight;
    std::array<ShellOpening, 5> shellOpenings;
    glm::vec2 goalClearance;      // Length behind each goal line, and total width.
    float sidelineClearance;

    // Pitch, tunnel, corridors and seats are stadium-local.
    Footprint pitch;
    float pitchYawDegrees;
    Footprint playerTunnel;
    std::array<SeatingTier, 2> tiers;
    glm::vec2 mainEntrance;  // Stadium-local (x, z), on the street-facing shell.
    std::array<Footprint, 4> spectatorCorridors;
    std::vector<SeatPlacement> seats;

    // Buildings and pedestrian routes are plot-local.
    Footprint playerFacilities;
    Footprint ticketOffice;
    Footprint fanShop;
    Footprint monument;
    Footprint gateClearance;
    std::array<Footprint, 7> paths;
    Footprint playerRoute;
};

StadiumLayout makeStadiumLayout(const citylab::PlotInfo& plot);

}  // namespace team12
