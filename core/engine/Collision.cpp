#include "engine/Collision.h"

#include <algorithm>
#include <cmath>

namespace citylab {

void CollisionWorld::add(const Box& box, const std::string& owner) {
    Box b;
    b.min = glm::min(box.min, box.max);
    b.max = glm::max(box.min, box.max);
    boxes_.push_back({b, owner});
    gridDirty_ = true;
}

void CollisionWorld::removeOwner(const std::string& owner) {
    boxes_.erase(std::remove_if(boxes_.begin(), boxes_.end(),
                                [&](const Entry& e) { return e.owner == owner; }),
                 boxes_.end());
    gridDirty_ = true;
}

void CollisionWorld::rebuildGrid() const {
    grid_.clear();
    for (int i = 0; i < static_cast<int>(boxes_.size()); ++i) {
        const Box& b = boxes_[static_cast<size_t>(i)].box;
        const int x0 = static_cast<int>(std::floor(b.min.x / kCell)), x1 = static_cast<int>(std::floor(b.max.x / kCell));
        const int z0 = static_cast<int>(std::floor(b.min.z / kCell)), z1 = static_cast<int>(std::floor(b.max.z / kCell));
        for (int cx = x0; cx <= x1; ++cx)
            for (int cz = z0; cz <= z1; ++cz) grid_[cellKey(cx, cz)].push_back(i);
    }
    gridDirty_ = false;
}

template <typename F>
void CollisionWorld::forNear(float x, float z, float radius, F&& f) const {
    if (gridDirty_) rebuildGrid();
    const int x0 = static_cast<int>(std::floor((x - radius) / kCell)), x1 = static_cast<int>(std::floor((x + radius) / kCell));
    const int z0 = static_cast<int>(std::floor((z - radius) / kCell)), z1 = static_cast<int>(std::floor((z + radius) / kCell));
    // A box can sit in several cells; remember which ones we already visited.
    thread_local std::vector<int> seen;
    seen.clear();
    for (int cx = x0; cx <= x1; ++cx) {
        for (int cz = z0; cz <= z1; ++cz) {
            const auto it = grid_.find(cellKey(cx, cz));
            if (it == grid_.end()) continue;
            for (const int index : it->second) {
                if (std::find(seen.begin(), seen.end(), index) != seen.end()) continue;
                seen.push_back(index);
                f(boxes_[static_cast<size_t>(index)].box);
            }
        }
    }
}

float CollisionWorld::groundHeight(float x, float z, float feetY) const {
    float ground = 0.0f;
    forNear(x, z, kRadius, [&](const Box& b) {
        if (x + kRadius * 0.5f < b.min.x || x - kRadius * 0.5f > b.max.x) return;
        if (z + kRadius * 0.5f < b.min.z || z - kRadius * 0.5f > b.max.z) return;
        if (b.max.y <= feetY + kStepHeight) ground = std::max(ground, b.max.y);
    });
    return ground;
}

glm::vec3 CollisionWorld::move(const glm::vec3& from, const glm::vec2& delta, bool followGround) const {
    glm::vec3 p = from;
    // Sub-step fast moves so the walker cannot tunnel through thin walls.
    const float length = glm::length(delta);
    const int steps = std::max(1, static_cast<int>(std::ceil(length / (kRadius * 0.8f))));
    const glm::vec2 step = delta / static_cast<float>(steps);

    for (int s = 0; s < steps; ++s) {
        p.x += step.x;
        p.z += step.y;
        // Push out of every box that blocks the body (two passes handle corners).
        for (int pass = 0; pass < 2; ++pass) {
            forNear(p.x, p.z, kRadius + 1.0f, [&](const Box& b) {
                if (b.max.y <= p.y + (followGround ? kStepHeight : 0.0f) || b.min.y >= p.y + kBodyHeight)
                    return;  // step or overhead
                const float cx = std::clamp(p.x, b.min.x, b.max.x);
                const float cz = std::clamp(p.z, b.min.z, b.max.z);
                const float dx = p.x - cx, dz = p.z - cz;
                const float d2 = dx * dx + dz * dz;
                if (d2 >= kRadius * kRadius) return;
                if (d2 > 1e-8f) {
                    const float d = std::sqrt(d2);
                    p.x = cx + dx / d * kRadius;
                    p.z = cz + dz / d * kRadius;
                } else {
                    // The centre is inside the box: leave by the nearest side.
                    const float left = p.x - b.min.x, right = b.max.x - p.x;
                    const float back = p.z - b.min.z, front = b.max.z - p.z;
                    const float m = std::min({left, right, back, front});
                    if (m == left) p.x = b.min.x - kRadius;
                    else if (m == right) p.x = b.max.x + kRadius;
                    else if (m == back) p.z = b.min.z - kRadius;
                    else p.z = b.max.z + kRadius;
                }
            });
        }
        p.x = std::clamp(p.x, boundsMin_.x, boundsMax_.x);
        p.z = std::clamp(p.z, boundsMin_.y, boundsMax_.y);
        if (followGround) p.y = groundHeight(p.x, p.z, p.y);
    }
    return p;
}

float CollisionWorld::moveVertical(const glm::vec3& from, float deltaY) const {
    float y = std::max(0.0f, from.y + deltaY);
    forNear(from.x, from.z, kRadius, [&](const Box& b) {
        const float dx = from.x - std::clamp(from.x, b.min.x, b.max.x);
        const float dz = from.z - std::clamp(from.z, b.min.z, b.max.z);
        if (dx * dx + dz * dz >= kRadius * kRadius) return;
        if (deltaY < 0.0f && b.max.y <= from.y && b.max.y >= y)
            y = b.max.y;
        else if (deltaY > 0.0f && b.min.y >= from.y + kBodyHeight && b.min.y <= y + kBodyHeight)
            y = b.min.y - kBodyHeight;
    });
    return y;
}

glm::vec3 CollisionWorld::moveThrough(const glm::vec3& from, const glm::vec2& delta) const {
    return {std::clamp(from.x + delta.x, boundsMin_.x, boundsMax_.x), 0.0f,
            std::clamp(from.z + delta.y, boundsMin_.y, boundsMax_.y)};
}

}  // namespace citylab
