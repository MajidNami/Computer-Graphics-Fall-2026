#pragma once

#include "Geometry.h"

namespace team12 {

struct RoofSupport {
    glm::vec3 columnTop;
    glm::vec3 beamCenter;
    float beamLength;
    float yaw;
    float pitch;
};

struct StadiumRoof {
    Mesh membrane;
    Mesh fascia;
    Mesh liner;
    Mesh banners;
    Mesh lettering;
    Mesh scoreboard;
    Mesh scoreText;
    Mesh clockText;
    std::array<glm::vec3, 6> bannerFixtures;
    std::vector<RoofSupport> supports;
    glm::vec3 canopyHinge;
};

StadiumRoof makeStadiumRoof(const StadiumLayout& layout);
void drawStadiumRoof(const StadiumRoof& roof, float nightLight);

}  // namespace team12
