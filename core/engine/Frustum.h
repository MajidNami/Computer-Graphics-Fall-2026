#pragma once

#include <glm/glm.hpp>

namespace citylab {

// The six planes of what a camera sees. A test says "certainly outside" or
// "maybe inside": what it keeps may still be hidden behind something.
class Frustum {
public:
    Frustum() = default;
    explicit Frustum(const glm::mat4& projectionTimesView) { set(projectionTimesView); }

    // Gribb and Hartmann: the planes are sums and differences of the rows of
    // projection * view (GLM stores matrices by column).
    void set(const glm::mat4& m) {
        const auto row = [&](int i) { return glm::vec4(m[0][i], m[1][i], m[2][i], m[3][i]); };
        planes_[0] = row(3) + row(0);  // left
        planes_[1] = row(3) - row(0);  // right
        planes_[2] = row(3) + row(1);  // bottom
        planes_[3] = row(3) - row(1);  // top
        planes_[4] = row(3) + row(2);  // the near end
        planes_[5] = row(3) - row(2);  // the far end
        for (glm::vec4& p : planes_) p /= glm::length(glm::vec3(p));
    }

    bool sphere(const glm::vec3& centre, float radius) const {
        for (const glm::vec4& p : planes_)
            if (glm::dot(glm::vec3(p), centre) + p.w < -radius) return false;
        return true;
    }

    bool box(const glm::vec3& min, const glm::vec3& max) const {
        for (const glm::vec4& p : planes_) {
            // The corner furthest along the plane's normal: if even that one is behind it, the box is.
            const glm::vec3 corner{p.x >= 0 ? max.x : min.x, p.y >= 0 ? max.y : min.y, p.z >= 0 ? max.z : min.z};
            if (glm::dot(glm::vec3(p), corner) + p.w < 0.0f) return false;
        }
        return true;
    }

private:
    glm::vec4 planes_[6]{};
};

// Distance from a point to the closest point of a box (0 inside it).
inline float distanceToBox(const glm::vec3& p, const glm::vec3& min, const glm::vec3& max) {
    const glm::vec3 d = glm::max(glm::max(min - p, p - max), glm::vec3(0.0f));
    return glm::length(d);
}

// Everything a draw call needs to leave out what cannot be seen: the camera,
// its view volume, and how far things are still visible through the fog.
struct Viewer {
    glm::vec3 eye{0.0f};
    Frustum frustum;
    float distance = 1e9f;
};

}  // namespace citylab
