#pragma once

#include <functional>

#include <glm/glm.hpp>

namespace citylab {

// What a piece of drawing sends to OpenGL.
struct DrawMeasure {
    long vertices = 0;                       // triangle corners (a quad is 6), lines 2, points 1
    glm::vec3 min{1e9f}, max{-1e9f};         // bounding box in the coordinates the drawing uses
    bool overflow = false;                   // the feedback buffer was too small even at its limit
};

// Runs `draw` once in feedback mode, so nothing is drawn, and measures the
// geometry it submits. A huge orthographic box and no face culling make
// every primitive count, whatever the camera is. `draw` must not change the
// projection. Needs a current OpenGL context. Staff tools only: the city
// uses it to size its tiles, and F3 / --bench report the result.
DrawMeasure measureDrawing(const std::function<void()>& draw);

}  // namespace citylab
