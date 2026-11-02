#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "citylab/OpenGL.h"
#include "engine/Clock.h"

namespace citylab {

// Sky dome with a sunset glow, sun and moon discs, stars, drifting clouds and
// a ring of distant mountains. Everything is drawn around the eye, before the city,
// without writing depth, so it always stays "infinitely far away".
class Sky {
public:
    void init(GLuint glowTexture);
    void draw(const glm::vec3& eye, const Environment& env, double time) const;

private:
    void drawDome(const Environment& env) const;
    void drawStars(const Environment& env) const;
    void drawDiscs(const glm::mat4& view, const Environment& env) const;
    void drawClouds(const glm::mat4& view, const Environment& env, double time) const;
    void drawMountains(const Environment& env) const;

    struct Puff {
        float azimuth, elevation, size, shade;  // radians, radians, metres on the dome, 0..1
    };

    GLuint glow_ = 0;
    std::vector<glm::vec4> stars_;      // xyz direction, w brightness
    std::vector<glm::vec2> mountains_;  // (angle, height) samples
    std::vector<Puff> puffs_;           // clouds: clusters of soft discs
};

}  // namespace citylab
