#pragma once

#include <glm/glm.hpp>

#include "citylab/OpenGL.h"
#include "citylab/Props.h"
#include "city/Materials.h"

namespace citylab {

// The SDK's props (sdk/cpp/citylab/Props.h) with the city's textures,
// compiled once into display lists and placed many times with
// glTranslatef/glRotatef/glScalef. Each stands on y = 0 at the origin.
struct Props {
    GLuint flowerBed = 0;   // 3 x 1.2 m raised bed with flowers
    GLuint streetLamp = 0;  // 8 m pole whose arm reaches over local +z; its LED head is separate
    GLuint lampHead = 0;    // the glowing part of the street lamp, in the same frame
    GLuint parkLamp = 0;    // 4 m post; its globe is separate
    GLuint parkGlobe = 0;
    GLuint bench = 0;       // 2 m, sitting side towards +z
    GLuint busShelter = 0;  // 6 x 2 m glass shelter open towards +z
    GLuint craneMast = 0;   // tower crane mast, 34 m, with the slewing ring on top
    GLuint craneJib = 0;    // jib and counter-jib, turned about the mast axis

    // Where the light of each lamp is, in the lamp's frame.
    static constexpr float kStreetLampHeight = props::kStreetLampHeight, kStreetLampReach = props::kStreetLampReach;
    static constexpr float kParkLampHeight = props::kParkLampHeight;

    void build(const Materials& materials);
};

}  // namespace citylab
