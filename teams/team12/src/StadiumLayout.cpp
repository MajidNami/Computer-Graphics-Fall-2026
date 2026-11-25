#include "StadiumLayout.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

#include "Seating.h"

namespace team12 {

StadiumLayout makeStadiumLayout(const citylab::PlotInfo& plot) {
    constexpr float referenceWidth = 160.0f;
    constexpr float referenceDepth = 100.0f;
    constexpr float checkedGateDepth = 6.0f;
    const float scale = std::min(plot.width / referenceWidth, plot.depth / referenceDepth);
    const float front = plot.depth * 0.5f;

    StadiumLayout layout{};
    layout.plot = plot;
    layout.stadiumCenter = {-18.0f * scale, 0.0f, -5.0f * scale};
    layout.stadiumYawDegrees = 0.0f;
    const glm::vec2 outerRadii = glm::vec2{56.0f, 38.0f} * scale;
    const glm::vec2 innerRadii = glm::vec2{48.0f, 30.0f} * scale;
    layout.outerSize = outerRadii * 2.0f;
    layout.innerSize = innerRadii * 2.0f;
    layout.roofHeight = std::min(24.0f, plot.height);
    layout.shellSpringHeight = layout.roofHeight - 4.0f;
    layout.pitch = {{0.0f, 0.0f}, glm::vec2{70.0f, 36.0f} * scale};
    layout.goalClearance = glm::vec2{3.0f, 10.0f} * scale;
    layout.sidelineClearance = 2.0f * scale;
    layout.pitchYawDegrees = 0.0f;
    layout.mainEntrance = {0.0f, outerRadii.y};
    layout.tiers = {{
        {3, 0.95f * scale, 1.0f, 0.45f, innerRadii + glm::vec2{0.65f * scale}},
        {3, 0.95f * scale, 4.0f, 0.48f, innerRadii + glm::vec2{4.25f * scale}},
    }};

    const auto rectangle = [](float left, float right, float back, float frontEdge) -> Footprint {
        return {{(left + right) * 0.5f, (back + frontEdge) * 0.5f},
                {right - left, frontEdge - back}};
    };
    const float aisleHalfWidth = 2.0f * scale;
    layout.spectatorCorridors = {{
        rectangle(-aisleHalfWidth, aisleHalfWidth, innerRadii.y, outerRadii.y),
        rectangle(-aisleHalfWidth, aisleHalfWidth, -outerRadii.y, -innerRadii.y),
        rectangle(innerRadii.x, outerRadii.x, -aisleHalfWidth, aisleHalfWidth),
        rectangle(-outerRadii.x, -innerRadii.x, -aisleHalfWidth, aisleHalfWidth),
    }};

    layout.playerFacilities = {glm::vec2{59.0f, -23.0f} * scale, glm::vec2{24.0f, 18.0f} * scale};
    layout.ticketOffice = {glm::vec2{59.0f, 3.0f} * scale, glm::vec2{22.0f, 12.0f} * scale};
    layout.fanShop = {glm::vec2{59.0f, 25.0f} * scale, glm::vec2{24.0f, 16.0f} * scale};
    layout.monument = {glm::vec2{15.0f, 40.0f} * scale, glm::vec2{5.0f, 5.0f} * scale};
    const float gateHalfWidth = plot.gateWidth * 0.5f;
    layout.gateClearance = rectangle(-gateHalfWidth, gateHalfWidth, front - checkedGateDepth, front);

    const float serviceZ = layout.playerFacilities.center.y;
    const float tunnelZ = serviceZ - layout.stadiumCenter.z;
    const float ellipseZ = tunnelZ / outerRadii.y;
    const float shellX = outerRadii.x * std::sqrt(1.0f - ellipseZ * ellipseZ);
    const float tunnelHalfWidth = 1.5f * scale;
    const float tunnelInnerX = layout.pitch.size.x * 0.5f + scale;
    layout.playerTunnel = rectangle(tunnelInnerX, shellX, tunnelZ - tunnelHalfWidth, tunnelZ + tunnelHalfWidth);
    layout.playerRoute = rectangle(layout.stadiumCenter.x + shellX,
                                   layout.playerFacilities.center.x - layout.playerFacilities.size.x * 0.5f,
                                   serviceZ - tunnelHalfWidth, serviceZ + tunnelHalfWidth);
    const float tunnelStart = std::asin((tunnelZ - tunnelHalfWidth) / outerRadii.y);
    const float tunnelEnd = std::asin((tunnelZ + tunnelHalfWidth) / outerRadii.y);
    const float frontHalfAngle = std::asin(2.4f * scale / outerRadii.x);
    const float sideHalfAngle = std::asin(2.4f * scale / outerRadii.y);
    const float quarterTurn = glm::half_pi<float>();
    layout.shellOpenings = {{
        {quarterTurn, frontHalfAngle, 4.0f},
        {3.0f * quarterTurn, frontHalfAngle, 3.5f},
        {0.0f, sideHalfAngle, 3.5f},
        {2.0f * quarterTurn, sideHalfAngle, 3.5f},
        {(tunnelStart + tunnelEnd) * 0.5f, (tunnelEnd - tunnelStart) * 0.5f + 0.008f, 3.2f},
    }};

    const float entranceX = layout.stadiumCenter.x + layout.mainEntrance.x;
    const float entranceZ = layout.stadiumCenter.z + layout.mainEntrance.y;
    const float plazaBack = 35.0f * scale;
    const float plazaFront = 37.0f * scale;
    const float spineLeft = 41.0f * scale;
    const float spineRight = 45.0f * scale;
    const float pathHalfWidth = 2.0f * scale;
    const auto publicApproach = [&](const Footprint& building) {
        const float facadeZ = building.center.y + building.size.y * 0.5f;
        return rectangle(spineRight, building.center.x + pathHalfWidth, facadeZ, facadeZ + 3.0f * scale);
    };
    const float shopFront = layout.fanShop.center.y + layout.fanShop.size.y * 0.5f;
    layout.paths = {{
        rectangle(-gateHalfWidth, gateHalfWidth, plazaFront, front),
        rectangle(entranceX - pathHalfWidth, layout.fanShop.center.x + pathHalfWidth, plazaBack, plazaFront),
        rectangle(entranceX - pathHalfWidth, entranceX + pathHalfWidth, entranceZ, plazaBack),
        rectangle(spineLeft, spineRight, serviceZ + tunnelHalfWidth, plazaBack),
        publicApproach(layout.playerFacilities),
        publicApproach(layout.ticketOffice),
        rectangle(layout.fanShop.center.x - pathHalfWidth, layout.fanShop.center.x + pathHalfWidth,
                  shopFront, plazaBack),
    }};
    layout.seats = makeSeatPlacements(layout);
    return layout;
}

}  // namespace team12
