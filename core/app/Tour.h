#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "city/Layout.h"

namespace citylab {

// The automatic walk (--tour): around every block along its sidewalks (so it
// passes every plot gate), and through the park to the tower and the fountain.
// Then it starts again.
//
// The route is built from layout.json, so it follows the plan when the
// plan changes. It does not collide; it is laid out along free lanes.
class Tour {
public:
    void plan(const Layout& layout);
    void advance(float dt);

    glm::vec3 feet() const { return {position_.x, 0.0f, position_.y}; }
    float yaw() const { return yaw_; }  // radians, 0 = north
    float progress() const;             // 0..1 along the whole route

private:
    glm::vec2 pointAt(float distance) const;

    std::vector<glm::vec2> points_;
    std::vector<float> lengths_;  // distance from the start to each point
    float travelled_ = 0.0f;
    glm::vec2 position_{0.0f};
    float yaw_ = 0.0f;
    bool started_ = false;
};

}  // namespace citylab
