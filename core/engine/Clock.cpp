#include "engine/Clock.h"

#include <algorithm>
#include <cmath>

namespace citylab {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMaxSunElevation = 65.0f * kPi / 180.0f;  // Tehran is near 35.7 N

float smooth(float edge0, float edge1, float x) {
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

glm::vec3 mix3(const glm::vec3& a, const glm::vec3& b, float t) { return a + (b - a) * std::clamp(t, 0.0f, 1.0f); }

// The sun is up from 05:00 to 19:00, as in early summer.
constexpr float kSunrise = 5.0f, kSunset = 19.0f;

// Angle of the sun along its daily half circle: 0 at sunrise, pi at sunset.
float sunAngle(float hours) { return (hours - kSunrise) / (kSunset - kSunrise) * kPi; }

}  // namespace

CityClock::CityClock(float startHours, float timeScale) : hours_(0.0f), timeScale_(timeScale) {
    setHours(startHours);
}

void CityClock::advance(float realSeconds) {
    if (paused_) return;
    // Nights pass kNightSpeedup times faster than days, so visitors spend
    // most of their time in daylight but still see every evening. The speed
    // changes smoothly around dusk and dawn.
    const float elevation = std::sin(sunAngle(hours_));
    const float night = 1.0f - smooth(-0.12f, 0.05f, hours_ > kSunrise && hours_ < kSunset ? elevation : -1.0f);
    addHours(realSeconds * timeScale_ * (1.0f + (kNightSpeedup - 1.0f) * night) / 3600.0f);
}

void CityClock::setHours(float hours) {
    hours_ = std::fmod(hours, 24.0f);
    if (hours_ < 0.0f) hours_ += 24.0f;
}

void CityClock::addHours(float hours) {
    float h = hours_ + hours;
    while (h >= 24.0f) { h -= 24.0f; ++day_; }
    while (h < 0.0f) { h += 24.0f; --day_; }
    hours_ = h;
}

void CityClock::cycleSpeed() {
    if (timeScale_ < kDefaultScale - 1.0f) timeScale_ = kDefaultScale;
    else if (timeScale_ < 599.0f) timeScale_ = 600.0f;
    else timeScale_ = 1.0f;
}

Environment CityClock::environment() const {
    Environment env;

    // The sun rises in the east at 05:00, culminates in the south at 12:00
    // and sets in the west at 19:00: a half circle tilted towards the south.
    const float h = sunAngle(hours_);
    env.sunDirection = glm::normalize(glm::vec3(std::cos(h), std::sin(h) * std::sin(kMaxSunElevation),
                                                std::sin(h) * std::cos(kMaxSunElevation)));
    const float elevation = env.sunDirection.y;

    env.daylight = smooth(-0.10f, 0.28f, elevation);
    env.starAlpha = 1.0f - smooth(-0.22f, -0.02f, elevation);
    env.lampLevel = 1.0f - smooth(0.00f, 0.14f, elevation);

    // Key colours for night, golden hour and noon, blended by elevation.
    const glm::vec3 nightZenith{0.012f, 0.018f, 0.055f}, nightHorizon{0.035f, 0.055f, 0.12f};
    const glm::vec3 duskZenith{0.16f, 0.21f, 0.42f}, duskHorizon{0.96f, 0.56f, 0.33f};
    const glm::vec3 dayZenith{0.16f, 0.45f, 0.92f}, dayHorizon{0.70f, 0.85f, 0.99f};
    const float toDusk = smooth(-0.20f, 0.02f, elevation);
    const float toDay = smooth(0.04f, 0.42f, elevation);
    env.zenithColor = mix3(mix3(nightZenith, duskZenith, toDusk), dayZenith, toDay);
    env.horizonColor = mix3(mix3(nightHorizon, duskHorizon, toDusk), dayHorizon, toDay);
    env.glowColor = mix3(glm::vec3(1.0f, 0.45f, 0.20f), glm::vec3(1.0f, 0.85f, 0.65f), toDay);

    // GL_LIGHT0 follows the sun by day and the moon by night. Both are dim
    // around the switch-over, so the change of direction is not noticeable.
    const float sunStrength = smooth(-0.08f, 0.10f, elevation);
    const float moonStrength = 1.0f - smooth(-0.16f, -0.04f, elevation);
    env.ambientColor = mix3(glm::vec3(0.21f, 0.22f, 0.31f), glm::vec3(0.50f, 0.51f, 0.54f), env.daylight);
    if (sunStrength >= 0.5f * moonStrength) {
        const glm::vec3 low{1.00f, 0.60f, 0.34f}, high{1.00f, 0.97f, 0.90f};
        env.lightDirection = env.sunDirection;
        env.lightColor = mix3(low, high, smooth(0.0f, 0.45f, elevation)) * sunStrength;
    } else {
        glm::vec3 moon = -env.sunDirection;
        moon.y = std::max(moon.y, 0.25f);
        env.lightDirection = glm::normalize(moon);
        env.lightColor = glm::vec3(0.30f, 0.34f, 0.48f) * moonStrength;
    }
    return env;
}

}  // namespace citylab
