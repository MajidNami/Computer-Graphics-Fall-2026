#pragma once

// How much there is in the city: every "how many" that decides how crowded it
// looks and how much it costs to draw, in one place. Tune the look here.
// (docs/city.md, "Life in the city", quotes these numbers.)

namespace citylab::density {

// ---- Lamps ----------------------------------------------------------------

constexpr float kStreetLampSpacing = 36.0f;      // one street lamp every 36 m along the sidewalks

// ---- The skyline ----------------------------------------------------------

constexpr int kSkylineBuildings = 80;            // towers outside the city
constexpr float kSkylineInner = 230.0f;          // clear ring around the city
constexpr float kSkylineDepth = 420.0f;          // how far out the skyline reaches beyond that
constexpr float kSkylineGap = 14.0f;             // space kept between two towers

// ---- Life -----------------------------------------------------------------

constexpr int kBoats = 5;                        // on the lake

}  // namespace citylab::density
