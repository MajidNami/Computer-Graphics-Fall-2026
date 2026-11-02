#pragma once

#include <memory>

#include <glm/glm.hpp>

namespace citylab {

class Context;

// Everything about "now" that a zone may react to.
//
// All vectors are in PLOT-LOCAL coordinates: the origin is the centre of your
// plot on the ground, x runs across the frontage (-40..+40, or -80..+80 on a
// double plot), z runs from the back (-50) to the street gate (+50), and +y is
// up (0..40, and down to -5 for a basement or a pool).
struct Frame {
    float dt = 0.0f;           // seconds since the previous update; drive ALL motion with it
    double time = 0.0;         // seconds since the zone started or was last reset (F8)
    float timeOfDay = 12.0f;   // shared city clock in hours, 0 <= timeOfDay < 24
    float daylight = 1.0f;     // 0 = night, 1 = full day; use it to switch lights on at dusk
    int day = 0;               // how many midnights the city clock has passed
    glm::vec3 sunDirection{0.0f, 1.0f, 0.0f};  // unit vector towards the sun (y < 0 at night)
    glm::vec3 cameraPosition{0.0f};            // where the viewer's eye is
    bool focused = false;      // the viewer is on or near your plot (your onKey() is live)
};

// The one class each team writes.
//
// The host calls these methods; you never call them yourself. When draw()
// runs, the modelview matrix already places your plot in the city, so you
// draw in plot-local coordinates with ordinary classic OpenGL calls.
class Zone {
public:
    virtual ~Zone() = default;

    // Called once after the OpenGL context exists. Load textures, build
    // display lists and register colliders here.
    virtual void init(Context& ctx) { (void)ctx; }

    // Called once per frame before draw(). Advance animations here.
    virtual void update(const Frame& frame) { (void)frame; }

    // Called once per frame. Draw your plot; do not change the projection,
    // the viewport, fog, or GL_LIGHT0 (the sun belongs to the city).
    virtual void draw(const Context& ctx) const = 0;

    // F8: put every animation back to its starting state.
    virtual void reset() {}

    // A key was pressed while your plot is focused. Letters arrive lowercase
    // ('a'..'z', except w a s d q e which move the camera), digits '0'..'9'
    // and ' ' for space.
    virtual void onKey(int key) { (void)key; }
};

namespace detail {
using ZoneFactory = std::unique_ptr<Zone> (*)();
bool registerZone(const char* teamId, ZoneFactory factory);
}  // namespace detail

}  // namespace citylab

// Put this once in one .cpp file of your team, after your class:
//
//     CITYLAB_REGISTER_ZONE(team07::AirportZone);
//
// CITYLAB_TEAM_ID is set by the build system from your folder name.
#define CITYLAB_REGISTER_ZONE(ZoneClass)                                             \
    static const bool citylabZoneRegistered_ = ::citylab::detail::registerZone(       \
        CITYLAB_TEAM_ID, []() -> std::unique_ptr<::citylab::Zone> {                   \
            return std::make_unique<ZoneClass>();                                     \
        })
