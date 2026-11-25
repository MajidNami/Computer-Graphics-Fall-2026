#pragma once

#include "Geometry.h"

namespace team12 {

inline constexpr int seatingSectors = 4;
inline constexpr int nominalSeatsPerRow = 96;

// Build once in init(). Seating and Crowd both consume layout.seats.
std::vector<SeatPlacement> makeSeatPlacements(const StadiumLayout& layout);

struct SeatingRow {
    glm::vec3 origin;
    Mesh tread;
    std::vector<SeatPlacement> seats;
};

struct SeatingSection {
    glm::vec3 origin;
    int tier;
    std::vector<SeatingRow> rows;
};

struct SeatingBowl {
    std::vector<SeatingSection> sections;
    Mesh stairs;
    Mesh rails;
    Mesh seat;
    Mesh seatMount;
};

SeatingBowl makeSeatingBowl(const StadiumLayout& layout, citylab::Context& ctx);
void drawSeatingBowl(const SeatingBowl& bowl);

}  // namespace team12
