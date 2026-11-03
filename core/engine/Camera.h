#pragma once

#include <glm/glm.hpp>

#include "engine/Options.h"
#include "engine/Settings.h"

class PlatformWindow;

namespace citylab {

class CollisionWorld;

// The viewer. Three modes, switched with F9:
//   Walk  - first person at eye height 1.7 m, collides with the city; Space jumps
//   Fly   - free flight, no collisions (good for looking at roofs)
//   Orbit - turns around a target point (good for building one plot)
//
// Walking and flying look around with the mouse (captured, or dragged with
// the left button) and speed up and slow down quickly but without a jerk:
// Shift runs, Ctrl creeps. Looking is never smoothed: the view turns by
// exactly what the mouse did, in the frame it did it. Orbiting follows the
// mouse and the wheel a little behind, so that a notch of the wheel is a glide.
//
// Yaw is measured from north (-z) towards east (+x), in radians.
class Camera {
public:
    static constexpr float kEyeHeight = 1.7f;

    CameraMode mode() const { return mode_; }
    void setMode(CameraMode mode);
    const char* modeName() const;

    // Walk/fly: put the eye (walk: the feet) at `position` looking along yaw/pitch.
    void place(const glm::vec3& position, float yaw, float pitch = 0.0f);
    // Orbit around `target` from `distance` metres.
    void orbit(const glm::vec3& target, float yaw, float pitch, float distance);

    void update(const PlatformWindow& window, float dt, const CollisionWorld& world);
    // Speeds and the mouse, from the viewer's settings.json.
    void configure(const Settings& settings) {
        settings_ = settings;
        walkThrough_ = settings.walkThroughObjects;
    }

    // The mouse wheel changes how fast the walker and the flyer go, by a share
    // per notch; settings.json holds the speeds the program starts with.
    float speedScale() const { return speedScale_; }
    float walkSpeed() const { return settings_.walkSpeed * speedScale_; }
    float flySpeed() const { return settings_.flySpeed * speedScale_; }
    // Walking through walls and buildings (F11), or being stopped by them.
    bool walkThroughObjects() const { return walkThrough_; }
    void setWalkThroughObjects(bool on) { walkThrough_ = on; }

    glm::vec3 eye() const;
    glm::vec3 forward() const;
    glm::mat4 view() const;
    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }
    glm::vec3 feet() const { return feet_; }
    // The point the viewer is "at": the feet when walking, the eye when
    // flying, the orbit centre when orbiting. Decides which plot has focus.
    glm::vec3 focusPoint() const {
        return mode_ == CameraMode::Orbit ? target_ : mode_ == CameraMode::Fly ? flyEye_ : feet_;
    }

    // Used by the automatic tour, which drives the walker itself.
    void setWalkPose(const glm::vec3& feet, float yaw, float pitch);

private:
    CameraMode mode_ = CameraMode::Walk;
    glm::vec3 feet_{0.0f};       // walk mode: feet on the ground
    glm::vec3 flyEye_{0.0f};     // fly mode: eye position
    glm::vec3 target_{0.0f};     // orbit mode
    float distance_ = 80.0f;
    float yaw_ = 0.0f, pitch_ = 0.0f;
    float bob_ = 0.0f;           // walking head bob phase
    float bobAmount_ = 0.0f;
    // The height of the eye above the ground it has been walking on. The feet
    // jump by a step at a time; this follows them in a short glide, so a stair
    // is climbed rather than snapped onto.
    float groundY_ = 0.0f;
    float verticalSpeed_ = 0.0f;
    bool airborne_ = false;
    // Walk and fly: the speed along the view's own axes (x to the right, y up,
    // z ahead), which eases towards what the keys ask for. Kept in the view's
    // frame, not the world's, so turning the view while holding a key turns the
    // movement with it at once instead of sliding on in the old direction.
    glm::vec3 drive_{0.0f};
    float speedScale_ = 1.0f;
    bool walkThrough_ = false;
    // Orbit: what the mouse and keys ask for; the camera follows it.
    glm::vec3 targetGoal_{0.0f};
    float yawGoal_ = 0.0f, pitchGoal_ = 0.0f, distanceGoal_ = 80.0f;
    Settings settings_;
};

}  // namespace citylab
