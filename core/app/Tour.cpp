#include "app/Tour.h"

#include <algorithm>
#include <cmath>

namespace citylab {
namespace {

constexpr float kSpeed = 6.0f;      // m/s, a brisk walk
constexpr float kLookAhead = 7.0f;  // the walker looks at the path this far ahead
constexpr float kTurnRate = 2.2f;   // rad/s

float wrapAngle(float a) {
    const float pi = 3.14159265f;
    while (a > pi) a -= 2.0f * pi;
    while (a < -pi) a += 2.0f * pi;
    return a;
}

}  // namespace

void Tour::plan(const Layout& L) {
    points_.clear();
    glm::vec2 at{L.spawnPosition.x, L.spawnPosition.z};
    auto go = [&](glm::vec2 p) {
        points_.push_back(p);
        at = p;
    };
    // Around a rectangle along its sidewalk, starting at the corner nearest
    // to where the walker is, so every plot gate on it is passed once.
    auto around = [&](const Rect& r) {
        const glm::vec2 corners[4] = {{r.x0, r.z0}, {r.x1, r.z0}, {r.x1, r.z1}, {r.x0, r.z1}};
        int first = 0;
        for (int k = 1; k < 4; ++k)
            if (glm::length(corners[k] - at) < glm::length(corners[first] - at)) first = k;
        for (int k = 0; k <= 4; ++k) go(corners[(first + k) % 4]);
    };
    const float lane = L.sidewalk * 0.5f;  // the middle of the sidewalk

    go(at);
    // The three northern blocks.
    for (const char* id : {"commercial", "sports", "culture"}) {
        if (const DistrictLayout* d = L.findDistrict(id)) around(d->rect.grown(lane));
    }
    // Into the park along the western path, round the lake, to the tower
    // and out along the eastern side.
    const Rect& pk = L.park;
    // The paths that continue the inner avenues are in the middle of the gap between two blocks.
    const DistrictLayout* west = L.findDistrict("culture");
    const DistrictLayout* middle = L.findDistrict("commercial");
    const DistrictLayout* east = L.findDistrict("sports");
    const float pathWest = west && middle ? (west->rect.x1 + middle->rect.x0) * 0.5f : pk.x0 * 0.35f;
    const float pathEast = middle && east ? (middle->rect.x1 + east->rect.x0) * 0.5f : pk.x1 * 0.35f;
    go({pathWest, pk.z0 + lane});
    go({pathWest, L.lake.y});
    go({L.tower.x - 16.0f, L.tower.y});
    go({L.tower.x, L.tower.y - 16.0f});
    go({L.tower.x + 16.0f, L.tower.y});
    go({L.fountain.x - L.fountainRadius - 6.0f, L.fountain.y});
    go({L.fountain.x, L.fountain.y + L.fountainRadius + 6.0f});
    go({pathEast, pk.z1 - lane});
    for (const char* id : {"energy", "services", "transport"}) {
        if (const DistrictLayout* d = L.findDistrict(id)) around(d->rect.grown(lane));
    }
    go({L.tower.x, pk.z1 - lane});
    go({L.tower.x, L.tower.y + 16.0f});

    lengths_.assign(points_.size(), 0.0f);
    for (size_t i = 1; i < points_.size(); ++i)
        lengths_[i] = lengths_[i - 1] + glm::length(points_[i] - points_[i - 1]);
    travelled_ = 0.0f;
    position_ = points_.empty() ? glm::vec2(0.0f) : points_.front();
    started_ = false;
}

glm::vec2 Tour::pointAt(float distance) const {
    if (points_.empty()) return glm::vec2(0.0f);
    if (distance <= 0.0f) return points_.front();
    for (size_t i = 1; i < points_.size(); ++i) {
        if (distance <= lengths_[i]) {
            const float segment = lengths_[i] - lengths_[i - 1];
            const float t = segment > 0.0f ? (distance - lengths_[i - 1]) / segment : 1.0f;
            return glm::mix(points_[i - 1], points_[i], t);
        }
    }
    return points_.back();
}

float Tour::progress() const { return lengths_.empty() ? 0.0f : travelled_ / std::max(1.0f, lengths_.back()); }

void Tour::advance(float dt) {
    if (points_.size() < 2) return;
    travelled_ += kSpeed * dt;
    if (travelled_ > lengths_.back()) {  // start the round again
        travelled_ = 0.0f;
        started_ = false;
    }
    position_ = pointAt(travelled_);
    const glm::vec2 ahead = pointAt(travelled_ + kLookAhead) - position_;
    if (glm::length(ahead) < 1e-3f) return;
    const float target = std::atan2(ahead.x, -ahead.y);  // yaw 0 looks along -z
    if (!started_) {
        yaw_ = target;
        started_ = true;
        return;
    }
    const float delta = wrapAngle(target - yaw_);
    yaw_ = wrapAngle(yaw_ + std::clamp(delta, -kTurnRate * dt, kTurnRate * dt));
}

}  // namespace citylab
