#pragma once

#include "Geometry.h"

namespace team12 {

struct LandscapeObject {
    glm::vec2 position;
    float yaw;
    float scale;
};

struct StadiumGrounds {
    Footprint plazaApron;
    std::vector<Footprint> gardens;
    std::vector<LandscapeObject> trees;
    std::vector<LandscapeObject> lamps;
    std::vector<LandscapeObject> benches;
    std::vector<LandscapeObject> bollards;
    Mesh whiteBallPanels;
    Mesh blackBallPanels;
};

StadiumGrounds makeGrounds(const StadiumLayout& layout, citylab::Context& ctx);
void drawLandscape(const StadiumLayout& layout, const StadiumGrounds& grounds);

}  // namespace team12
