#include "engine/FirstPersonHands.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "citylab/OpenGL.h"
#include "citylab/Shapes.h"
#include "engine/Camera.h"
#include "engine/Clock.h"

namespace citylab {
namespace {

constexpr float kPi = 3.14159265f;
const glm::vec3 kLeather{0.43f, 0.30f, 0.17f};
const glm::vec3 kPanel{0.57f, 0.42f, 0.25f};
const glm::vec3 kRubber{0.12f, 0.13f, 0.12f};
const glm::vec3 kSleeve{0.18f, 0.23f, 0.20f};
const glm::vec3 kThread{0.71f, 0.58f, 0.38f};

void oval(const glm::vec3& center, const glm::vec3& size, const glm::vec3& color) {
    glColor3fv(glm::value_ptr(color));
    glPushMatrix();
    glTranslatef(center.x, center.y, center.z);
    glScalef(size.x, size.y, size.z);
    shapes::sphere(1.0f, 12, 8);
    glPopMatrix();
}

void segment(const glm::vec3& from, const glm::vec3& to, float radius, float thickness, const glm::vec3& color) {
    const glm::vec3 z = glm::normalize(to - from);
    const glm::vec3 up = std::fabs(z.y) > 0.95f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    const glm::vec3 x = glm::normalize(glm::cross(up, z));
    const glm::vec3 y = glm::cross(z, x);
    const glm::mat4 basis{glm::vec4(x, 0), glm::vec4(y, 0), glm::vec4(z, 0), glm::vec4((from + to) * 0.5f, 1)};
    glPushMatrix();
    glMultMatrixf(glm::value_ptr(basis));
    oval({0, 0, 0}, {radius, thickness, glm::length(to - from) * 0.5f + radius * 0.65f}, color);
    glPopMatrix();
}

// Right hand, with its thumb towards -x. The other hand mirrors this geometry.
void glove() {
    // Sleeve and a snug elastic cuff, with a leather fastening across its back.
    segment({0, -0.025f, 0.04f}, {0.065f, -0.20f, 0.34f}, 0.055f, 0.047f, kSleeve);
    oval({0, -0.005f, 0.034f}, {0.043f, 0.034f, 0.043f}, kRubber);
    oval({0, 0, 0.006f}, {0.039f, 0.030f, 0.032f}, kLeather);
    oval({0, 0.030f, 0.031f}, {0.040f, 0.009f, 0.020f}, kPanel);
    oval({0.028f, 0.033f, 0.030f}, {0.009f, 0.006f, 0.016f}, kRubber);

    // Broad palm, then a raised leather panel and separate knuckle protection.
    oval({0, 0, -0.064f}, {0.057f, 0.032f, 0.080f}, kLeather);
    oval({0, 0.025f, -0.064f}, {0.048f, 0.012f, 0.062f}, kPanel);
    oval({0.018f, -0.022f, -0.065f}, {0.040f, 0.013f, 0.064f}, kRubber);

    const float lengths[] = {1.0f, 1.08f, 1.0f, 0.80f};
    for (int finger = 0; finger < 4; ++finger) {
        const float x = -0.039f + static_cast<float>(finger) * 0.026f;
        const float tip = -0.112f - 0.064f * lengths[finger];
        const float radius = finger == 3 ? 0.012f : 0.014f;
        // Loosely curled fingers: top, bend and tucked fingertip are individually modelled.
        segment({x, 0, -0.108f}, {x, 0, tip + 0.018f}, radius, 0.025f, kLeather);
        segment({x, 0, tip + 0.018f}, {x, -0.025f, tip}, radius, 0.017f, kLeather);
        segment({x, -0.025f, tip}, {x, -0.049f, tip + 0.028f}, radius, 0.015f, kRubber);
        oval({x, 0.023f, -0.125f}, {radius * 0.84f, 0.011f, 0.019f}, kRubber);
        oval({x, 0.015f, tip + 0.006f}, {radius * 0.8f, 0.007f, 0.010f}, kPanel);
    }

    // The thumb folds around the index finger, leaving the four fingers readable.
    segment({-0.041f, -0.006f, -0.036f}, {-0.070f, -0.010f, -0.087f}, 0.024f, 0.025f, kLeather);
    segment({-0.070f, -0.010f, -0.087f}, {-0.065f, -0.035f, -0.135f}, 0.019f, 0.020f, kPanel);
    segment({-0.065f, -0.035f, -0.135f}, {-0.041f, -0.047f, -0.145f}, 0.017f, 0.016f, kLeather);

    // Short stitches follow the curved dorsal panel (geometry, no texture files).
    for (int stitch = 0; stitch < 7; ++stitch) {
        const float z = -0.020f - static_cast<float>(stitch) * 0.012f;
        const float nz = (z + 0.064f) / 0.062f;
        const float y = 0.026f + 0.012f * std::sqrt(std::max(0.0f, 1.0f - 0.625f * 0.625f - nz * nz));
        for (float side : {-1.0f, 1.0f}) {
            oval({side * 0.030f, y, z}, {0.0015f, 0.0014f, 0.003f}, kThread);
        }
    }
}

}  // namespace

FirstPersonHands::~FirstPersonHands() {
    if (glove_) glDeleteLists(glove_, 1);
}

void FirstPersonHands::init() {
    if (glove_) return;
    glove_ = glGenLists(1);
    if (!glove_) return;  // drawing directly remains a fallback on old drivers
    glNewList(glove_, GL_COMPILE);
    glove();
    glEndList();
}

void FirstPersonHands::update(const Camera& camera, float dt) {
    if (camera.mode() != CameraMode::Walk) {
        walking_ = false;
        movement_ = turn_ = tilt_ = 0.0f;
        return;
    }
    const float follow = 1.0f - std::exp(-12.0f * dt);
    const float speed = camera.movementSpeed();
    movement_ += (std::clamp(speed / 10.0f, 0.0f, 1.0f) - movement_) * follow;
    phase_ += dt * (5.0f + speed * 0.5f) * std::min(1.0f, speed);
    phase_ = std::fmod(phase_, 2.0f * kPi);
    // Mouse sway uses angular change, so it works for captured and dragged mouse look.
    const float yawChange = walking_ ? std::remainder(camera.yaw() - lastYaw_, 2.0f * kPi) : 0.0f;
    const float pitchChange = walking_ ? camera.pitch() - lastPitch_ : 0.0f;
    const float inverseDt = 1.0f / std::max(dt, 0.001f);
    turn_ += (std::clamp(yawChange * inverseDt * -0.008f, -0.025f, 0.025f) - turn_) * follow;
    tilt_ += (std::clamp(pitchChange * inverseDt * -0.006f, -0.020f, 0.020f) - tilt_) * follow;
    lastYaw_ = camera.yaw();
    lastPitch_ = camera.pitch();
    walking_ = true;
}

void FirstPersonHands::draw(float aspect, const Environment& environment) const {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_FOG);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);  // the left hand mirrors the right-hand mesh
    glDisable(GL_SCISSOR_TEST);
    for (int i = 0; i < 6; ++i) glDisable(GL_CLIP_PLANE0 + i);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    // The world colour is finished. Give the hands their own depth so walls
    // never cut them off, while fingers and sleeves still occlude one another.
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    // A fixed viewmodel FOV keeps hands comfortably framed as the world FOV changes.
    const glm::mat4 projection = glm::perspective(glm::radians(65.0f), aspect, 0.02f, 2.0f);
    glLoadMatrixf(glm::value_ptr(projection));
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const float brightness = 0.48f + 0.52f * environment.daylight;
    const GLfloat ambient[] = {0.30f * brightness, 0.33f * brightness, 0.37f * brightness, 1};
    const GLfloat key[] = {0.85f * brightness, 0.80f * brightness, 0.70f * brightness, 1};
    const GLfloat direction[] = {-0.4f, 0.8f, 0.7f, 0};
    const GLfloat specular[] = {0.15f, 0.15f, 0.15f, 1};
    const GLfloat none[] = {0, 0, 0, 1};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
    for (int i = 0; i < 8; ++i) glDisable(GL_LIGHT0 + i);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, direction);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, key);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_AMBIENT, none);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, none);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 18.0f);

    for (float side : {-1.0f, 1.0f}) {
        const float swing = std::sin(phase_ + (side > 0 ? kPi : 0.0f)) * movement_;
        glPushMatrix();
        glTranslatef(side * 0.20f * aspect + turn_ + swing * 0.008f, -0.285f + tilt_ + std::fabs(swing) * 0.012f,
                     -0.52f + swing * 0.015f);
        glRotatef(15.0f + swing * 4.0f, 1, 0, 0);
        glRotatef(side * -12.0f, 0, 1, 0);
        glRotatef(side * -10.0f, 0, 0, 1);
        glScalef(side, 1, 1);
        if (glove_)
            glCallList(glove_);
        else
            glove();
        glPopMatrix();
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
}

}  // namespace citylab
