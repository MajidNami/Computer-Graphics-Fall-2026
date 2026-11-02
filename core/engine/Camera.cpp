#include "engine/Camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "engine/Collision.h"
#include "framework/PlatformWindow.h"

namespace citylab {
namespace {

constexpr float kMaxPitch = 1.45f;

// How fast the speed follows the keys, per second: a step towards the goal
// of 1 - e^(-rate * dt) of what is left, so it does not depend on the frame
// rate. A rate of 24 gets to nine tenths in 0.1 s: a start you feel at once
// and still without a jerk. Stopping is a little quicker than starting.
constexpr float kWalkStart = 24.0f, kWalkStop = 32.0f;
constexpr float kFlyStart = 11.0f, kFlyStop = 14.0f;
constexpr float kStepGlide = 16.0f;   // the eye follows the ground this fast
constexpr float kOrbitFollow = 18.0f; // the orbit follows the mouse and the wheel this fast
constexpr float kBobHeight = 0.03f;   // metres up and down at full walking speed

float follow(float rate, float dt) { return 1.0f - std::exp(-rate * dt); }

glm::vec3 direction(float yaw, float pitch) {
    return {std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
}

}  // namespace

const char* Camera::modeName() const {
    switch (mode_) {
    case CameraMode::Walk: return "walk";
    case CameraMode::Fly: return "fly";
    default: return "orbit";
    }
}

void Camera::setMode(CameraMode mode) {
    if (mode == mode_) return;
    const glm::vec3 e = eye();
    if (mode == CameraMode::Walk) {
        feet_ = {e.x, 0.0f, e.z};
        pitch_ = std::clamp(pitch_, -0.6f, 0.6f);
    } else if (mode == CameraMode::Fly) {
        flyEye_ = e;
    } else {
        target_ = e + forward() * 40.0f;
        target_.y = 0.0f;
        distance_ = glm::length(e - target_);
        targetGoal_ = target_;
        yawGoal_ = yaw_;
        pitchGoal_ = pitch_;
        distanceGoal_ = distance_;
    }
    mode_ = mode;
    drive_ = glm::vec3(0.0f);
    groundY_ = feet_.y;
}

void Camera::place(const glm::vec3& position, float yaw, float pitch) {
    yaw_ = yaw;
    pitch_ = std::clamp(pitch, -kMaxPitch, kMaxPitch);
    feet_ = {position.x, 0.0f, position.z};
    flyEye_ = position;
    drive_ = glm::vec3(0.0f);
    groundY_ = feet_.y;
    if (mode_ == CameraMode::Orbit) {
        target_ = position + direction(yaw_, pitch_) * distance_;
        targetGoal_ = target_;
        yawGoal_ = yaw_;
        pitchGoal_ = pitch_;
        distanceGoal_ = distance_;
    }
}

void Camera::orbit(const glm::vec3& target, float yaw, float pitch, float distance) {
    mode_ = CameraMode::Orbit;
    target_ = target;
    yaw_ = yaw;
    pitch_ = std::clamp(pitch, -kMaxPitch, -0.05f);
    distance_ = distance;
    targetGoal_ = target_;
    yawGoal_ = yaw_;
    pitchGoal_ = pitch_;
    distanceGoal_ = distance_;
}

void Camera::setWalkPose(const glm::vec3& feet, float yaw, float pitch) {
    mode_ = CameraMode::Walk;
    feet_ = feet;
    groundY_ = feet.y;
    yaw_ = yaw;
    pitch_ = pitch;
}

glm::vec3 Camera::eye() const {
    switch (mode_) {
    case CameraMode::Walk:
        return {feet_.x, groundY_ + kEyeHeight + std::sin(bob_) * kBobHeight * bobAmount_, feet_.z};
    case CameraMode::Fly: return flyEye_;
    default: return target_ - direction(yaw_, pitch_) * distance_;
    }
}

glm::vec3 Camera::forward() const { return direction(yaw_, pitch_); }

glm::mat4 Camera::view() const {
    const glm::vec3 e = eye();
    return glm::lookAt(e, e + forward(), glm::vec3(0, 1, 0));
}

void Camera::update(const PlatformWindow& window, float dt, const CollisionWorld& world) {
    const auto& in = window.input();
    const bool run = window.keyDown(Key::Shift), creep = window.keyDown(Key::Control);
    auto held = [&](int a, int b) { return window.keyDown(a) || window.keyDown(b); };

    if (mode_ == CameraMode::Orbit) {
        // The mouse and keys move the goal; the camera glides after it.
        if (in.leftMouseDown) {
            yawGoal_ += in.mouseDeltaX * 0.006f;
            pitchGoal_ = std::clamp(pitchGoal_ - in.mouseDeltaY * 0.006f, -kMaxPitch, -0.03f);
        }
        if (held(Key::Left, 'a')) yawGoal_ -= 1.2f * dt;
        if (held(Key::Right, 'd')) yawGoal_ += 1.2f * dt;
        if (held(Key::Up, 'w')) pitchGoal_ = std::max(pitchGoal_ - 0.8f * dt, -kMaxPitch);
        if (held(Key::Down, 's')) pitchGoal_ = std::min(pitchGoal_ + 0.8f * dt, -0.03f);
        distanceGoal_ *= std::pow(0.88f, in.scrollDelta);
        if (held(Key::PageUp, 'e')) distanceGoal_ *= std::exp(-1.2f * dt);
        if (held(Key::PageDown, 'q')) distanceGoal_ *= std::exp(1.2f * dt);
        distanceGoal_ = std::clamp(distanceGoal_, 3.0f, 700.0f);
        if (in.rightMouseDown || in.middleMouseDown) {
            // Pan in the ground plane, scaled so the point under the mouse follows it.
            const glm::vec3 right{std::cos(yaw_), 0.0f, std::sin(yaw_)};
            const glm::vec3 ahead{std::sin(yaw_), 0.0f, -std::cos(yaw_)};
            const float k = distance_ * 0.0015f;
            targetGoal_ -= right * in.mouseDeltaX * k;
            targetGoal_ += ahead * in.mouseDeltaY * k;
        }
        const float k = follow(kOrbitFollow, dt);
        yaw_ += (yawGoal_ - yaw_) * k;
        pitch_ += (pitchGoal_ - pitch_) * k;
        target_ += (targetGoal_ - target_) * k;
        // Zoom in the logarithm: the same share of the distance per step, so the
        // view neither races at the end of a long zoom out nor crawls on the way in.
        distance_ = std::exp(std::log(distance_) + (std::log(distanceGoal_) - std::log(distance_)) * k);
        return;
    }

    // Look: move the mouse while it is captured, or drag with the left
    // button; arrow keys turn.
    if (window.mouseCaptured() || in.leftMouseDown) {
        const float k = (window.mouseCaptured() ? 0.0025f : 0.0045f) * settings_.mouseSensitivity;
        yaw_ += in.mouseDeltaX * k;
        pitch_ = std::clamp(pitch_ - in.mouseDeltaY * k * (settings_.invertMouse ? -1.0f : 1.0f), -kMaxPitch,
                            kMaxPitch);
    }
    if (window.keyDown(Key::Left)) yaw_ -= 1.6f * dt;
    if (window.keyDown(Key::Right)) yaw_ += 1.6f * dt;

    // The wheel sets the pace: a share more or less per notch.
    if (in.scrollDelta != 0.0f) speedScale_ = std::clamp(speedScale_ * std::pow(1.12f, in.scrollDelta), 0.15f, 8.0f);

    float forwardAmount = 0.0f, strafe = 0.0f;
    if (held(Key::Up, 'w')) forwardAmount += 1.0f;
    if (held(Key::Down, 's')) forwardAmount -= 1.0f;
    if (window.keyDown('d')) strafe += 1.0f;
    if (window.keyDown('a')) strafe -= 1.0f;

    const glm::vec3 right{std::cos(yaw_), 0.0f, std::sin(yaw_)};

    if (mode_ == CameraMode::Walk) {
        const glm::vec3 ahead{std::sin(yaw_), 0.0f, -std::cos(yaw_)};
        const float speed = (creep ? 2.0f : run ? settings_.runSpeed : settings_.walkSpeed) * speedScale_;
        glm::vec2 asked{strafe, forwardAmount};
        if (glm::length(asked) > 1.0f) asked = glm::normalize(asked);
        // Speed follows the keys: quick, but not a jerk (and stopping a little quicker).
        const glm::vec3 goal{asked.x * speed, 0.0f, asked.y * speed};
        const float rate = glm::length(goal) > glm::length(drive_) ? kWalkStart : kWalkStop;
        drive_ += (goal - drive_) * follow(rate, dt);
        if (glm::length(goal) == 0.0f && glm::length(drive_) < 0.05f) drive_ = glm::vec3(0.0f);
        // The view's own axes, turned to where the view looks now.
        const glm::vec3 velocity = ahead * drive_.z + right * drive_.x;
        const glm::vec2 delta{velocity.x * dt, velocity.z * dt};
        feet_ = walkThrough_ ? world.moveThrough(feet_, delta) : world.move(feet_, delta);
        // The eye follows the ground in a short glide, so a stair is climbed, not snapped onto.
        groundY_ += (feet_.y - groundY_) * follow(kStepGlide, dt);
        if (std::fabs(feet_.y - groundY_) < 0.001f) groundY_ = feet_.y;
        // A little bob, in step with the walking and only as strong as the pace.
        const float pace = glm::length(drive_);
        const bool moving = pace > 0.3f;
        const float strength = std::min(1.0f, pace / (settings_.walkSpeed * speedScale_));
        bobAmount_ += ((moving && settings_.headBob ? strength : 0.0f) - bobAmount_) * follow(6.0f, dt);
        if (moving) bob_ = std::fmod(bob_ + dt * (5.0f + 0.5f * pace), 2.0f * 3.14159265f);
        return;
    }

    // Fly
    const float speed = settings_.flySpeed * speedScale_ * (creep ? 0.2f : run ? 4.0f : 1.0f);
    float rise = 0.0f;
    if (held(Key::PageUp, 'e')) rise += 1.0f;
    if (held(Key::PageDown, 'q')) rise -= 1.0f;
    glm::vec3 asked{strafe, rise, forwardAmount};
    if (glm::length(asked) > 1.0f) asked = glm::normalize(asked);
    const glm::vec3 goal = asked * speed;
    const float rate = glm::length(goal) > glm::length(drive_) ? kFlyStart : kFlyStop;
    drive_ += (goal - drive_) * follow(rate, dt);
    if (glm::length(goal) == 0.0f && glm::length(drive_) < 0.05f) drive_ = glm::vec3(0.0f);
    const glm::vec3 velocity = direction(yaw_, pitch_) * drive_.z + right * drive_.x + glm::vec3(0.0f, drive_.y, 0.0f);
    flyEye_ += velocity * dt;
    flyEye_.y = std::clamp(flyEye_.y, 0.5f, 600.0f);
}

}  // namespace citylab
