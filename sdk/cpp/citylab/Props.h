#pragma once

#include "citylab/OpenGL.h"

// Ready-made city objects: the same lamps, benches, flower beds, shelters and
// cranes the city itself uses, for your own plot. Each one stands on y = 0 at the
// origin and faces +z, so you place it like any shape:
//
//     glPushMatrix();
//     glTranslatef(12.0f, 0.0f, -8.0f);
//     glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
//     props::bench();
//     glPopMatrix();
//
// They are drawn with colours only and leave the OpenGL state as they found
// it (they save it with glPushAttrib). You may pass textures of your own.
//
// They are helpers, not part of what is graded: modelling your own objects
// is what each phase asks for. Use them for the surroundings.
namespace citylab::props {

// Optional textures for the parts of the objects; 0 means a plain colour.
struct Textures {
    GLuint leaves = 0, wood = 0, concrete = 0, metal = 0, glass = 0;
};

void shrub(float height = 1.2f, const Textures& textures = {});       // clipped bush
void flowerBed(const Textures& textures = {});                         // 3 x 1.2 m raised bed
void bench(const Textures& textures = {});                             // 2 m, seat facing +z
// An 8 m street light whose arm reaches over +z. glow 0..1 lights it, for
// example with 1 - frame.daylight.
void streetLamp(float glow = 0.0f, const Textures& textures = {});
void parkLamp(float glow = 0.0f, const Textures& textures = {});      // 4 m post with a globe
void busShelter(const Textures& textures = {});                        // 6 x 2.4 m, open towards +z
// A 34 m tower crane; the jib turns about the mast by jibDegrees.
void crane(float jibDegrees = 0.0f, const Textures& textures = {});

// Measurements, for colliders and for placing lights.
constexpr float kStreetLampHeight = 7.6f, kStreetLampReach = 1.9f;  // the light, in the lamp's frame
constexpr float kParkLampHeight = 3.9f;
constexpr float kCraneHeight = 34.0f;

// Parts that the city draws on their own (glowing heads at night, the
// turning jib): the same geometry as inside the objects above.
void streetLampHead();
void parkLampGlobe();
void craneMast(const Textures& textures = {});
void craneJib(const Textures& textures = {});
void streetLampPost(const Textures& textures = {});
void parkLampPost(const Textures& textures = {});

}  // namespace citylab::props
