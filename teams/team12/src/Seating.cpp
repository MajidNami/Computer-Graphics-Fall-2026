#include "Seating.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

namespace team12 {

std::vector<SeatPlacement> makeSeatPlacements(const StadiumLayout& layout) {
    constexpr int slotsPerSector = nominalSeatsPerRow / seatingSectors;
    constexpr float seatClearance = 0.35f;
    std::vector<SeatPlacement> seats;
    int id = 0;
    for (int tierIndex = 0; tierIndex < static_cast<int>(layout.tiers.size()); ++tierIndex) {
        const auto& tier = layout.tiers[tierIndex];
        for (int row = 0; row < tier.rows; ++row) {
            const glm::vec2 radii = tier.firstRowRadii + glm::vec2{row * tier.rowDepth};
            const float height = tier.firstRowHeight + row * tier.risePerRow;
            for (int slot = 0; slot < nominalSeatsPerRow; ++slot, ++id) {
                const float angle = glm::two_pi<float>() * slot / nominalSeatsPerRow;
                const glm::vec2 position{radii.x * std::cos(angle), radii.y * std::sin(angle)};
                const auto overlapsSeat = [position, seatClearance](const Footprint& corridor) {
                    const glm::vec2 offset = glm::abs(position - corridor.center);
                    const glm::vec2 limits = corridor.size * 0.5f + glm::vec2{seatClearance};
                    return offset.x <= limits.x && offset.y <= limits.y;
                };
                if (overlapsSeat(layout.playerTunnel) ||
                    std::any_of(layout.spectatorCorridors.begin(), layout.spectatorCorridors.end(), overlapsSeat)) {
                    continue;
                }
                const glm::vec2 toPitch = layout.pitch.center - position;
                const float yaw = glm::degrees(std::atan2(toPitch.x, toPitch.y));
                seats.push_back({id, {position.x, height, position.y}, yaw, tierIndex, row,
                                 slot / slotsPerSector, true});
            }
        }
    }
    return seats;
}

}  // namespace team12
