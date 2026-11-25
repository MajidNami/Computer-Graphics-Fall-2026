#pragma once

#include "Geometry.h"

namespace team12 {

struct StadiumShell {
    Mesh plinth;
    Mesh walls;
    Mesh portalFrames;
    Mesh redPanels;
    Mesh whitePanels;
    Mesh lightPanels;
};

StadiumShell makeStadiumShell(const StadiumLayout& layout, citylab::Context& ctx);
void drawArchitecture(const StadiumShell& shell, float nightLight);

}  // namespace team12
