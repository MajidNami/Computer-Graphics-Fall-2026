#include "city/TramModel.h"

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "city/Geometry.h"
#include "citylab/OpenGL.h"
#include "citylab/Shapes.h"

namespace citylab::tram {
namespace {

constexpr float kLength = 27.0f;
constexpr float kSection = 8.7f;
constexpr float kGap = 0.45f;
constexpr float kWheelRadius = 0.32f;
constexpr float kPi = 3.14159265f;

void paint(float r, float g, float b) {
    glColor3f(r, g, b);
}
void box(float x0, float y0, float z0, float x1, float y1, float z1) {
    geom::block({x0, y0, z0}, {x1, y1, z1}, 0.0f, true);
}
void glow(float r, float g, float b) {
    const GLfloat value[] = {r, g, b, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, value);
}
void quad(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, const glm::vec3 &d) {
    const glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
    glBegin(GL_QUADS);
    glNormal3f(n.x, n.y, n.z);
    for (const auto &p : {a, b, c, d})
        glVertex3f(p.x, p.y, p.z);
    glEnd();
}
void rod(const glm::vec3 &a, const glm::vec3 &b, float radius) {
    const glm::vec3 delta = b - a;
    const float length = glm::length(delta);
    const glm::vec3 axis = glm::cross(glm::vec3(0, 1, 0), delta / length);
    glPushMatrix();
    glTranslatef(a.x, a.y, a.z);
    glRotatef(glm::degrees(std::acos(delta.y / length)), axis.x, axis.y, axis.z);
    shapes::cylinder(radius, length, 8);
    glPopMatrix();
}

// Bevelled shoulders and skirts catch sunlight instead of reading as a box.
void shell(float z0, float z1) {
    const glm::vec2 profile[] = {{-1.08f, .46f}, {1.08f, .46f},   {1.25f, .70f},   {1.25f, 2.82f},
                                 {1.02f, 3.24f}, {-1.02f, 3.24f}, {-1.25f, 2.82f}, {-1.25f, .70f}};
    for (int i = 0; i < 8; ++i) {
        const auto &a = profile[i];
        const auto &b = profile[(i + 1) % 8];
        quad({a.x, a.y, z1}, {a.x, a.y, z0}, {b.x, b.y, z0}, {b.x, b.y, z1});
    }
}

// Both cab ends use the same local geometry, facing +z.
void cab(bool rear) {
    paint(.92f, .94f, .91f);
    const float y[] = {.46f, .78f, 1.42f, 2.82f, 3.24f};
    const float w[] = {.90f, 1.10f, 1.13f, 1.03f, .86f};
    const float z[] = {-.18f, 0.0f, -.10f, -.78f, -1.24f};
    for (int i = 0; i < 4; ++i) {
        quad({-w[i], y[i], z[i]}, {w[i], y[i], z[i]}, {w[i + 1], y[i + 1], z[i + 1]}, {-w[i + 1], y[i + 1], z[i + 1]});
        for (float side : {-1.0f, 1.0f}) {
            const float x = side * 1.25f;
            if (side > 0)
                quad({w[i], y[i], z[i]}, {x, y[i], -1.6f}, {x, y[i + 1], -1.6f}, {w[i + 1], y[i + 1], z[i + 1]});
            else
                quad({x, y[i], -1.6f}, {-w[i], y[i], z[i]}, {-w[i + 1], y[i + 1], z[i + 1]}, {x, y[i + 1], -1.6f});
        }
    }
    paint(.035f, .075f, .095f);
    quad({-1.04f, 1.50f, -.127f}, {1.04f, 1.50f, -.127f}, {.94f, 2.78f, -.749f}, {-.94f, 2.78f, -.749f});
    paint(.12f, .25f, .30f);
    quad({-.92f, 1.63f, -.184f}, {.92f, 1.63f, -.184f}, {.84f, 2.63f, -.670f}, {-.84f, 2.63f, -.670f});
    paint(.23f, .39f, .44f);
    quad({-.89f, 2.38f, -.546f}, {.86f, 2.38f, -.546f}, {.84f, 2.57f, -.638f}, {-.87f, 2.57f, -.638f});
    paint(.035f, .045f, .05f);
    rod({.04f, 1.55f, -.10f}, {-.64f, 2.12f, -.38f}, .018f); // windscreen wiper
    box(-.82f, 2.85f, -.88f, .82f, 3.08f, -.73f);            // destination display
    glow(.65f, .32f, .04f);
    paint(1.0f, .64f, .13f);
    // LED route number 01, made from small rectangular segments.
    for (float x : {-.66f, -.45f}) {
        box(x, 2.89f, -.724f, x + .025f, 3.04f, -.714f);
        if (x < -.5f) {
            box(x + .10f, 2.89f, -.724f, x + .125f, 3.04f, -.714f);
            box(x, 2.89f, -.724f, x + .125f, 2.915f, -.714f);
            box(x, 3.015f, -.724f, x + .125f, 3.04f, -.714f);
        }
    }
    for (int i = 0; i < 7; ++i) {
        const float x = -.12f + i * .10f;
        box(x, 2.94f, -.724f, x + .065f, 2.985f, -.714f);
    }
    glow(0, 0, 0);
    paint(.94f, .31f, .055f);
    box(-1.03f, .98f, -.067f, 1.03f, 1.16f, -.016f);
    paint(.08f, .11f, .13f);
    box(-.87f, .49f, -.10f, .87f, .65f, .025f); // bumper
    box(-.29f, .28f, -.15f, .29f, .43f, .08f);  // coupler cover
    for (float x : {-.79f, .79f}) {
        paint(.025f, .04f, .055f);
        box(x - .20f, .79f, -.035f, x + .20f, .95f, .018f);
        paint(rear ? .70f : .87f, rear ? .08f : .92f, rear ? .055f : .94f);
        box(x - .15f, .83f, .019f, x + .15f, .89f, .03f);
        paint(.15f, .19f, .21f);
        rod({x * 1.37f, 2.48f, -1.25f}, {x * 1.62f, 2.42f, -.98f}, .028f);
        box(x * 1.62f - .065f, 2.30f, -1.02f, x * 1.62f + .065f, 2.53f, -.89f); // mirror
    }
}

// Paired side windows: opaque blue glass with a restrained sky reflection.
void window(float side, float z0, float z1, bool lit) {
    glPushMatrix();
    glScalef(side, 1, 1);
    if (side < 0)
        glFrontFace(GL_CW);
    if (lit) {
        paint(.65f, .73f, .70f);
        box(1.266f, 1.59f, z0, 1.269f, 2.65f, z1);
        paint(.085f, .14f, .17f);
        // Dark lower silhouettes keep the lit windows from becoming one glowing band.
        box(1.270f, 1.59f, z0 + .10f, 1.273f, 1.91f, z1 - .10f);
    } else {
        paint(.13f, .25f, .30f);
        box(1.257f, 1.59f, z0, 1.260f, 2.65f, z1);
        paint(.22f, .36f, .41f);
        box(1.261f, 2.39f, z0 + .025f, 1.264f, 2.56f, z1 - .025f);
        paint(.10f, .18f, .22f);
        box(1.261f, 1.62f, z0 + .025f, 1.264f, 1.88f, z1 - .025f);
    }
    glFrontFace(GL_CCW);
    glPopMatrix();
}

void door(float side, float centre) {
    glPushMatrix();
    glScalef(side, 1, 1);
    if (side < 0)
        glFrontFace(GL_CW);
    paint(.93f, .36f, .075f);
    box(1.255f, .48f, centre - .78f, 1.275f, 2.88f, centre + .78f);
    paint(.065f, .105f, .12f);
    box(1.276f, .61f, centre - .66f, 1.283f, 2.77f, centre + .66f);
    for (float offset : {-.34f, .34f}) {
        paint(.15f, .28f, .32f);
        box(1.284f, 1.12f, centre + offset - .24f, 1.29f, 2.62f, centre + offset + .24f);
        paint(.39f, .48f, .49f);
        box(1.291f, .66f, centre + offset - .24f, 1.296f, .88f, centre + offset + .24f);
    }
    paint(.63f, .67f, .65f);
    box(1.295f, .62f, centre - .025f, 1.30f, 2.77f, centre + .025f);
    paint(.98f, .78f, .20f);
    box(1.302f, 1.28f, centre - .09f, 1.32f, 1.68f, centre - .055f);
    box(1.302f, 1.28f, centre + .055f, 1.32f, 1.68f, centre + .09f);
    box(1.287f, .48f, centre - .68f, 1.34f, .54f, centre + .68f);  // yellow threshold
    box(1.28f, 1.39f, centre + .83f, 1.31f, 1.49f, centre + .92f); // stop button
    glFrontFace(GL_CCW);
    glPopMatrix();
}

void roof(float z0, float z1) {
    paint(.67f, .73f, .73f);
    box(-.86f, 3.24f, z0 + 1.1f, .86f, 3.46f, z1 - 1.1f);
    paint(.20f, .27f, .29f);
    for (int i = 0; i < 9; ++i) {
        const float z = z0 + 1.35f + i * .23f;
        box(-.70f, 3.462f, z, .70f, 3.48f, z + .065f);
    }
    paint(.80f, .83f, .80f);
    box(-1.01f, 3.20f, z0 + .15f, -.91f, 3.29f, z1 - .15f);
    box(.91f, 3.20f, z0 + .15f, 1.01f, 3.29f, z1 - .15f);
}

}  // namespace

void body() {
    for (int s = 0; s < 3; ++s) {
        const float z1 = -s * (kSection + kGap), z0 = z1 - kSection;
        const float a = z0 + (s == 2 ? 1.6f : 0.0f);
        const float b = z1 - (s == 0 ? 1.6f : 0.0f);
        paint(.91f, .94f, .91f);
        shell(a, b);
        paint(.13f, .18f, .20f);
        box(-1.12f, .43f, a, 1.12f, .61f, b);
        for (float side : {-1.0f, 1.0f}) {
            glPushMatrix();
            glScalef(side, 1, 1);
            if (side < 0)
                glFrontFace(GL_CW);
            paint(.94f, .35f, .065f);
            box(1.251f, .77f, a, 1.255f, 1.26f, b);
            paint(.035f, .075f, .095f);
            box(1.251f, 1.44f, a, 1.255f, 2.80f, b);
            paint(.96f, .63f, .19f);
            box(1.256f, 1.27f, a, 1.259f, 1.32f, b);
            glFrontFace(GL_CCW);
            glPopMatrix();
            const float centre = (z0 + z1) * .5f;
            door(side, centre);
            for (float z = a + .18f; z < b - .30f; z += 1.35f) {
                const float end = std::min(z + 1.14f, b - .12f);
                if (end > centre - .88f && z < centre + .88f)
                    continue;
                window(side, z, end, false);
            }
        }
        roof(z0, z1);
        // Bogie frames and axles, with the wheels drawn separately.
        for (float z : {z0 + 1.65f, z1 - 1.65f}) {
            paint(.075f, .10f, .12f);
            box(-.62f, .22f, z - .66f, .62f, .46f, z + .66f);
            for (float axle : {z - .48f, z + .48f}) {
                paint(.32f, .38f, .39f);
                rod({-.85f, .34f, axle}, {.85f, .34f, axle}, .055f);
            }
            for (float side : {-1.0f, 1.0f}) {
                paint(.22f, .28f, .29f);
                box(side * .85f - .08f, .36f, z - .68f, side * .85f + .08f, .53f, z + .68f);
            }
        }
        if (s < 2) {
            paint(.055f, .085f, .095f);
            box(-1.12f, .55f, z0 - kGap, 1.12f, 3.08f, z0);
            for (int rib = 0; rib < 7; ++rib) {
                const float z = z0 - kGap + rib * .067f;
                paint(rib % 2 ? .22f : .14f, .25f, .26f);
                box(-1.20f, .62f, z, -1.12f, 3.10f, z + .025f);
                box(1.12f, .62f, z, 1.20f, 3.10f, z + .025f);
                box(-1.20f, 3.07f, z, 1.20f, 3.12f, z + .025f);
            }
        }
    }
    cab(false);
    glPushMatrix();
    glTranslatef(0, 0, -kLength);
    glRotatef(180, 0, 1, 0);
    cab(true);
    glPopMatrix();

    // Diamond pantograph: paired arms, ceramic insulators and contact shoe.
    const float z = -13.5f;
    paint(.24f, .28f, .29f);
    box(-.70f, 3.47f, z - 1.0f, .70f, 3.60f, z + 1.0f);
    for (float x : {-.48f, .48f}) {
        paint(.78f, .69f, .51f);
        for (int i = 0; i < 3; ++i)
            box(x - .10f, 3.60f + i * .075f, z - .14f, x + .10f, 3.64f + i * .075f, z + .14f);
        paint(.20f, .25f, .27f);
        rod({x, 3.84f, z}, {x, 4.95f, z - 1.15f}, .04f);
        rod({x, 3.84f, z}, {x, 4.95f, z + 1.15f}, .04f);
        rod({x, 4.95f, z - 1.15f}, {x, 6.16f, z}, .035f);
        rod({x, 4.95f, z + 1.15f}, {x, 6.16f, z}, .035f);
    }
    paint(.12f, .17f, .19f);
    box(-.86f, 6.16f, z - .12f, .86f, 6.20f, z + .12f);
}

void wheel() {
    glPushMatrix();
    paint(.15f, .20f, .22f);
    shapes::cylinder(kWheelRadius, .16f, 16);
    glTranslatef(0, .165f, 0);
    paint(.48f, .55f, .56f);
    shapes::cylinder(.23f, .015f, 16);
    paint(.12f, .18f, .20f);
    for (int i = 0; i < 4; ++i) {
        glPushMatrix();
        glRotatef(i * 90.0f, 0, 1, 0);
        box(.085f, .017f, -.025f, .18f, .023f, .025f);
        glPopMatrix();
    }
    paint(.62f, .66f, .65f);
    shapes::cylinder(.085f, .035f, 12);
    glPopMatrix();
}

void wheels(float distance, unsigned int wheelList) {
    const float angle = std::fmod(distance / kWheelRadius * 180.0f / kPi, 360.0f);
    for (int s = 0; s < 3; ++s) {
        const float z1 = -s * (kSection + kGap), z0 = z1 - kSection;
        for (float z : {z0 + 1.65f, z1 - 1.65f})
            for (float axle : {z - .48f, z + .48f})
                for (float side : {-1.0f, 1.0f}) {
                    glPushMatrix();
                    glTranslatef(side * .72f, .34f, axle);
                    glRotatef(-90 * side, 0, 0, 1); // cylinder's y axis becomes the axle
                    glRotatef(angle * side, 0, 1, 0);
                    glCallList(wheelList);
                    glPopMatrix();
                }
    }
}

void lights(float cabinGlow) {
    glow(.25f * cabinGlow, .31f * cabinGlow, .27f * cabinGlow);
    for (int s = 0; s < 3; ++s) {
        const float z1 = -s * (kSection + kGap), z0 = z1 - kSection;
        const float a = z0 + (s == 2 ? 1.6f : 0.0f), b = z1 - (s == 0 ? 1.6f : 0.0f);
        const float centre = (z0 + z1) * .5f;
        for (float side : {-1.0f, 1.0f}) {
            for (float z = a + .18f; z < b - .30f; z += 1.35f) {
                const float end = std::min(z + 1.14f, b - .12f);
                if (end > centre - .88f && z < centre + .88f)
                    continue;
                window(side, z, end, true);
            }
        }
    }
    for (int end = 0; end < 2; ++end) {
        glPushMatrix();
        if (end) {
            glTranslatef(0, 0, -kLength);
            glRotatef(180, 0, 1, 0);
        }
        glow(end ? .75f : .85f, end ? .025f : .82f, end ? .015f : .66f);
        paint(end ? .95f : 1.0f, end ? .055f : .96f, end ? .035f : .79f);
        for (float x : {-.79f, .79f})
            box(x - .15f, .83f, .031f, x + .15f, .89f, .036f);
        glPopMatrix();
    }
    glow(0, 0, 0);
}

}  // namespace citylab::tram
