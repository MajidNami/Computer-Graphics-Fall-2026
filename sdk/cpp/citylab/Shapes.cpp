#include "citylab/Shapes.h"

#include <cmath>

#include "citylab/OpenGL.h"

namespace citylab::shapes {
namespace {

constexpr float kPi = 3.14159265358979323846f;

// One axis-aligned face of a box: four corners in counter-clockwise order
// seen from outside, with the shared normal.
void face(const float n[3], const float c[4][3], float uMax, float vMax) {
    glNormal3fv(n);
    glTexCoord2f(0.0f, 0.0f); glVertex3fv(c[0]);
    glTexCoord2f(uMax, 0.0f); glVertex3fv(c[1]);
    glTexCoord2f(uMax, vMax); glVertex3fv(c[2]);
    glTexCoord2f(0.0f, vMax); glVertex3fv(c[3]);
}

float uv(float size, float uvPerMeter) { return uvPerMeter > 0.0f ? size * uvPerMeter : 1.0f; }

}  // namespace

void box(float width, float height, float depth, float uvPerMeter) {
    const float x = width * 0.5f, y = height * 0.5f, z = depth * 0.5f;
    const float uw = uv(width, uvPerMeter), uh = uv(height, uvPerMeter), ud = uv(depth, uvPerMeter);

    glBegin(GL_QUADS);
    {   const float n[3] = {0, 0, 1};
        const float c[4][3] = {{-x, -y, z}, {x, -y, z}, {x, y, z}, {-x, y, z}};
        face(n, c, uw, uh); }
    {   const float n[3] = {0, 0, -1};
        const float c[4][3] = {{x, -y, -z}, {-x, -y, -z}, {-x, y, -z}, {x, y, -z}};
        face(n, c, uw, uh); }
    {   const float n[3] = {1, 0, 0};
        const float c[4][3] = {{x, -y, z}, {x, -y, -z}, {x, y, -z}, {x, y, z}};
        face(n, c, ud, uh); }
    {   const float n[3] = {-1, 0, 0};
        const float c[4][3] = {{-x, -y, -z}, {-x, -y, z}, {-x, y, z}, {-x, y, -z}};
        face(n, c, ud, uh); }
    {   const float n[3] = {0, 1, 0};
        const float c[4][3] = {{-x, y, z}, {x, y, z}, {x, y, -z}, {-x, y, -z}};
        face(n, c, uw, ud); }
    {   const float n[3] = {0, -1, 0};
        const float c[4][3] = {{-x, -y, -z}, {x, -y, -z}, {x, -y, z}, {-x, -y, z}};
        face(n, c, uw, ud); }
    glEnd();
}

void plane(float width, float depth, float uvPerMeter) {
    const float x = width * 0.5f, z = depth * 0.5f;
    const float uw = uv(width, uvPerMeter), ud = uv(depth, uvPerMeter);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0);   glVertex3f(-x, 0, z);
    glTexCoord2f(uw, 0);  glVertex3f(x, 0, z);
    glTexCoord2f(uw, ud); glVertex3f(x, 0, -z);
    glTexCoord2f(0, ud);  glVertex3f(-x, 0, -z);
    glEnd();
}

void disk(float radius, int slices) {
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0.5f, 0.5f);
    glVertex3f(0, 0, 0);
    for (int i = 0; i <= slices; ++i) {
        const float a = -2.0f * kPi * static_cast<float>(i) / static_cast<float>(slices);
        const float c = std::cos(a), s = std::sin(a);
        glTexCoord2f(0.5f + 0.5f * c, 0.5f + 0.5f * s);
        glVertex3f(radius * c, 0, radius * s);
    }
    glEnd();
}

void cylinder(float radius, float height, int slices, bool capped) {
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= slices; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(slices);
        const float a = 2.0f * kPi * t;
        const float c = std::cos(a), s = std::sin(a);
        glNormal3f(c, 0, -s);
        glTexCoord2f(t, 1); glVertex3f(radius * c, height, -radius * s);
        glTexCoord2f(t, 0); glVertex3f(radius * c, 0, -radius * s);
    }
    glEnd();
    if (!capped) return;
    glPushMatrix();
    glTranslatef(0, height, 0);
    disk(radius, slices);
    glPopMatrix();
    glPushMatrix();
    glRotatef(180, 1, 0, 0);  // the bottom cap faces -y
    disk(radius, slices);
    glPopMatrix();
}

void cone(float radius, float height, int slices, bool capped) {
    // The side normal leans outwards by the cone's slope.
    const float slant = std::sqrt(radius * radius + height * height);
    const float ny = radius / slant, nr = height / slant;
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < slices; ++i) {
        const float t0 = static_cast<float>(i) / static_cast<float>(slices);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(slices);
        const float a0 = 2.0f * kPi * t0, a1 = 2.0f * kPi * t1, am = 0.5f * (a0 + a1);
        glNormal3f(nr * std::cos(am), ny, -nr * std::sin(am));
        glTexCoord2f(0.5f * (t0 + t1), 1); glVertex3f(0, height, 0);
        glNormal3f(nr * std::cos(a0), ny, -nr * std::sin(a0));
        glTexCoord2f(t0, 0); glVertex3f(radius * std::cos(a0), 0, -radius * std::sin(a0));
        glNormal3f(nr * std::cos(a1), ny, -nr * std::sin(a1));
        glTexCoord2f(t1, 0); glVertex3f(radius * std::cos(a1), 0, -radius * std::sin(a1));
    }
    glEnd();
    if (!capped) return;
    glPushMatrix();
    glRotatef(180, 1, 0, 0);
    disk(radius, slices);
    glPopMatrix();
}

void sphere(float radius, int slices, int stacks) {
    for (int j = 0; j < stacks; ++j) {
        const float v0 = static_cast<float>(j) / static_cast<float>(stacks);
        const float v1 = static_cast<float>(j + 1) / static_cast<float>(stacks);
        const float p0 = kPi * (v0 - 0.5f), p1 = kPi * (v1 - 0.5f);
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= slices; ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(slices);
            const float a = 2.0f * kPi * u;
            const float x1 = std::cos(p1) * std::cos(a), z1 = -std::cos(p1) * std::sin(a), y1 = std::sin(p1);
            const float x0 = std::cos(p0) * std::cos(a), z0 = -std::cos(p0) * std::sin(a), y0 = std::sin(p0);
            glNormal3f(x1, y1, z1); glTexCoord2f(u, v1); glVertex3f(radius * x1, radius * y1, radius * z1);
            glNormal3f(x0, y0, z0); glTexCoord2f(u, v0); glVertex3f(radius * x0, radius * y0, radius * z0);
        }
        glEnd();
    }
}

void torus(float ringRadius, float tubeRadius, int rings, int sides) {
    for (int i = 0; i < rings; ++i) {
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= sides; ++j) {
            const float v = static_cast<float>(j) / static_cast<float>(sides);
            const float b = 2.0f * kPi * v;
            for (int k = 0; k <= 1; ++k) {
                const float u = static_cast<float>(i + k) / static_cast<float>(rings);
                const float a = 2.0f * kPi * u;
                const float cx = std::cos(a), cz = -std::sin(a);
                const float nx = std::cos(b) * cx, ny = std::sin(b), nz = std::cos(b) * cz;
                glNormal3f(nx, ny, nz);
                glTexCoord2f(u, v);
                glVertex3f(ringRadius * cx + tubeRadius * nx, tubeRadius * ny,
                           ringRadius * cz + tubeRadius * nz);
            }
        }
        glEnd();
    }
}

void gableRoof(float width, float height, float depth) {
    const float x = width * 0.5f, z = depth * 0.5f;
    const float slope = std::sqrt(x * x + height * height);
    const float nx = height / slope, ny = x / slope;
    glBegin(GL_QUADS);
    glNormal3f(-nx, ny, 0);  // left slope
    glTexCoord2f(0, 0); glVertex3f(-x, 0, z);
    glTexCoord2f(1, 0); glVertex3f(0, height, z);
    glTexCoord2f(1, 1); glVertex3f(0, height, -z);
    glTexCoord2f(0, 1); glVertex3f(-x, 0, -z);
    glNormal3f(nx, ny, 0);   // right slope
    glTexCoord2f(0, 0); glVertex3f(0, height, z);
    glTexCoord2f(1, 0); glVertex3f(x, 0, z);
    glTexCoord2f(1, 1); glVertex3f(x, 0, -z);
    glTexCoord2f(0, 1); glVertex3f(0, height, -z);
    glNormal3f(0, -1, 0);    // underside
    glTexCoord2f(0, 0); glVertex3f(-x, 0, -z);
    glTexCoord2f(1, 0); glVertex3f(x, 0, -z);
    glTexCoord2f(1, 1); glVertex3f(x, 0, z);
    glTexCoord2f(0, 1); glVertex3f(-x, 0, z);
    glEnd();
    glBegin(GL_TRIANGLES);   // the two gable ends
    glNormal3f(0, 0, 1);
    glTexCoord2f(0, 0); glVertex3f(-x, 0, z);
    glTexCoord2f(1, 0); glVertex3f(x, 0, z);
    glTexCoord2f(0.5f, 1); glVertex3f(0, height, z);
    glNormal3f(0, 0, -1);
    glTexCoord2f(0, 0); glVertex3f(x, 0, -z);
    glTexCoord2f(1, 0); glVertex3f(-x, 0, -z);
    glTexCoord2f(0.5f, 1); glVertex3f(0, height, -z);
    glEnd();
}

}  // namespace citylab::shapes
