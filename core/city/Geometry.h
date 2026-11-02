#pragma once

#include <functional>
#include <vector>

#include <glm/glm.hpp>

#include "city/Layout.h"

namespace citylab::geom {

// Building blocks for the city fabric. Unlike the SDK shapes, these use
// world-space texture coordinates (u, v = position * uvPerMeter), so walls
// and floors of any size show bricks and tiles at a constant real size.

// A colour for every point of the ground, from the x and z it is given.
using Tint = std::function<glm::vec3(float x, float z)>;

// Horizontal quad at height y, facing up. With a tint, every corner of its
// tiles of 25 m gets the colour the tint gives for that point, so the colour
// can change slowly over a field however small the texture is.
void ground(const Rect& r, float y, float uvPerMeter, const Tint& tint = nullptr);

// Axis-aligned solid box between two corners, all six faces.
void block(const glm::vec3& min, const glm::vec3& max, float uvPerMeter, bool bottom = false);

// Regular prism (n sides) standing on y = 0.
void prism(float radius, float height, int sides, float uvPerMeter);

// Tapered cylinder standing on y = 0: radius r0 at the bottom, r1 at the top.
void frustum(float r0, float r1, float height, int sides, float uvPerMeter, bool top);

// Horizontal ellipse (centre c, radii r along x and z) at height y, facing up.
void ellipse(const glm::vec2& c, const glm::vec2& r, float y, int segments, float uvPerMeter);
// The band between two ellipses with the same centre, facing up.
void ellipseRing(const glm::vec2& c, const glm::vec2& inner, const glm::vec2& outer, float y, int segments,
                 float uvPerMeter);
// A vertical wall along an ellipse from y0 to y1, facing out (or in).
void ellipseWall(const glm::vec2& c, const glm::vec2& r, float y0, float y1, int segments, float uvPerMeter,
                 bool inward);

}  // namespace citylab::geom
