#pragma once

#include "Geometry.h"

namespace team12 {

struct PitchScene {
    Mesh grass[2];
    Mesh runoff;
    Mesh markings;
    std::vector<glm::vec3> netLines;
    float halfLength;
    float goalWidth;
    float goalHeight;
    float rearDepth;
};

PitchScene makePitch(const StadiumLayout& layout);
void drawPitch(const StadiumLayout& layout, const PitchScene& scene);

}  // namespace team12
