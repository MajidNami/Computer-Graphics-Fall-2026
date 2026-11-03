#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace citylab {

struct Box {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
};

// Solid boxes for the walking camera: buildings, walls, lamp posts, benches,
// and the colliders that zones register with ctx.addCollider().
//
// The walker is a vertical capsule approximated by a circle (radius 0.3 m)
// at every height between its feet and its head. A box whose top is less
// than kStepHeight above the feet is a step and lifts the walker onto it.
class CollisionWorld {
public:
    static constexpr float kRadius = 0.3f;
    static constexpr float kStepHeight = 0.45f;
    static constexpr float kBodyHeight = 1.8f;

    // `owner` lets a zone's colliders be replaced when the zone restarts.
    void add(const Box& box, const std::string& owner = "city");
    void removeOwner(const std::string& owner);
    size_t size() const { return boxes_.size(); }

    // Height of the floor under (x, z) that a walker whose feet are at
    // `feetY` would stand on (never below 0).
    float groundHeight(float x, float z, float feetY) const;

    // Moves a walker from `from` (feet position) by `delta` in x/z, sliding
    // along walls. Returns the new feet position, including steps up/down.
    // Airborne movement keeps its height and cannot automatically climb steps.
    glm::vec3 move(const glm::vec3& from, const glm::vec2& delta, bool followGround = true) const;

    // Sweep the body vertically, stopping at floors and ceilings.
    float moveVertical(const glm::vec3& from, float deltaY) const;

    // The same walk without the walls: the walker goes through everything, on
    // the ground, and only the edge of the city stops it.
    glm::vec3 moveThrough(const glm::vec3& from, const glm::vec2& delta) const;

    void setBounds(float minX, float maxX, float minZ, float maxZ) {
        boundsMin_ = {minX, minZ};
        boundsMax_ = {maxX, maxZ};
    }

private:
    struct Entry {
        Box box;
        std::string owner;
    };
    static long long cellKey(int cx, int cz) { return (static_cast<long long>(cx) << 32) ^ (cz & 0xffffffffLL); }
    void rebuildGrid() const;
    template <typename F>
    void forNear(float x, float z, float radius, F&& f) const;

    std::vector<Entry> boxes_;
    mutable std::unordered_map<long long, std::vector<int>> grid_;
    mutable bool gridDirty_ = true;
    glm::vec2 boundsMin_{-1e9f}, boundsMax_{1e9f};
    static constexpr float kCell = 16.0f;
};

}  // namespace citylab
