#pragma once

#include "citylab/OpenGL.h"

namespace citylab {

class Camera;
struct Environment;

// Camera-local geometry: full-finger leather gloves, padded knuckles and jacket cuffs.
// Draw after the world and before the HUD, only in the walking camera.
class FirstPersonHands {
public:
    FirstPersonHands() = default;
    ~FirstPersonHands();
    FirstPersonHands(const FirstPersonHands&) = delete;
    FirstPersonHands& operator=(const FirstPersonHands&) = delete;

    void init();
    void update(const Camera& camera, float dt);
    void draw(float aspect, const Environment& environment) const;

private:
    GLuint glove_ = 0;
    float phase_ = 0.0f;
    float movement_ = 0.0f;
    float turn_ = 0.0f, tilt_ = 0.0f;
    float lastYaw_ = 0.0f, lastPitch_ = 0.0f;
    bool walking_ = false;
};

}  // namespace citylab
