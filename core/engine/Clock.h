#pragma once

#include <glm/glm.hpp>

namespace citylab {

// Light and sky for one moment of the day. World axes: +x east, +y up,
// -z north (so +z is south).
struct Environment {
    glm::vec3 sunDirection{0, 1, 0};    // unit vector towards the sun
    glm::vec3 lightDirection{0, 1, 0};  // towards GL_LIGHT0: the sun by day, the moon by night
    glm::vec3 lightColor{1.0f};
    glm::vec3 ambientColor{0.3f};
    glm::vec3 zenithColor{0.2f, 0.45f, 0.85f};
    glm::vec3 horizonColor{0.7f, 0.83f, 0.95f};
    glm::vec3 glowColor{1.0f, 0.6f, 0.3f};  // warm halo around a low sun
    float daylight = 1.0f;                  // 0 night .. 1 day
    float starAlpha = 0.0f;                 // how visible the stars are
    float lampLevel = 0.0f;                 // street lamps: 0 off .. 1 fully on
};

// The shared city clock. Every zone sees the same time, so sunsets, street
// lamps and shop opening hours happen together across the whole city.
class CityClock {
public:
    static constexpr float kDefaultStart = 10.0f;   // the city opens at 10:00
    static constexpr float kDefaultScale = 47.0f;   // a day (05-19) in about 18 minutes
    static constexpr float kNightSpeedup = 3.5f;    // a night in about 4 minutes

    explicit CityClock(float startHours = kDefaultStart, float timeScale = kDefaultScale);

    void advance(float realSeconds);
    void setHours(float hours);
    void addHours(float hours);

    void togglePause() { paused_ = !paused_; }
    bool paused() const { return paused_; }
    void cycleSpeed();  // F5: the default speed, 600x, 1x
    float timeScale() const { return timeScale_; }  // by day; nights run kNightSpeedup times faster

    float hours() const { return hours_; }  // 0 <= hours < 24
    int day() const { return day_; }

    Environment environment() const;

private:
    float hours_;
    float timeScale_;
    int day_ = 0;
    bool paused_ = false;
};

}  // namespace citylab
