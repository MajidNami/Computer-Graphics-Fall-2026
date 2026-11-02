#pragma once

// How much there is in the city: every "how many" that decides how crowded it
// looks and how much it costs to draw, in one place. Tune the look here.
// (docs/city.md, "Life in the city", quotes these numbers.) The city has no
// trees, cars, boats or buildings of its own: the teams model those.

namespace citylab::density {

// ---- Lamps ----------------------------------------------------------------

constexpr float kStreetLampSpacing = 36.0f;      // one street lamp every 36 m along the sidewalks

}  // namespace citylab::density
