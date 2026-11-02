#pragma once

#include <cmath>

#include <glm/glm.hpp>

#include "citylab/OpenGL.h"
#include "engine/Collision.h"

namespace citylab {

// A local frame on the ground: origin (x, z) plus a rotation about +y in
// steps of 90 degrees. Lets one piece of architecture code (an arcade side,
// a portal) be reused in all four directions, and turns its local boxes into
// world-space colliders.
struct GroundFrame {
    glm::vec2 origin{0.0f};
    float yawDegrees = 0.0f;

    void apply() const {  // multiply the current matrix: T(origin) * Ry(yaw)
        glTranslatef(origin.x, 0.0f, origin.y);
        glRotatef(yawDegrees, 0.0f, 1.0f, 0.0f);
    }

    glm::vec3 toWorld(const glm::vec3& p) const {
        const float a = glm::radians(yawDegrees);
        const float c = std::cos(a), s = std::sin(a);
        // Ry(a) * p, matching glRotatef(a, 0, 1, 0).
        return {origin.x + c * p.x + s * p.z, p.y, origin.y - s * p.x + c * p.z};
    }

    Box toWorld(const glm::vec3& localMin, const glm::vec3& localMax) const {
        const glm::vec3 a = toWorld(localMin), b = toWorld(localMax);
        return {glm::min(a, b), glm::max(a, b)};
    }
};

}  // namespace citylab
