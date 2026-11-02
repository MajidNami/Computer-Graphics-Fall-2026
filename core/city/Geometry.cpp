#include "city/Geometry.h"

#include <algorithm>
#include <cmath>

#include "citylab/OpenGL.h"

namespace citylab::geom {
namespace {

constexpr float kPi = 3.14159265358979323846f;

void vertex(float x, float y, float z, float u, float v) {
    glTexCoord2f(u, v);
    glVertex3f(x, y, z);
}

}  // namespace

void ground(const Rect& r, float y, float uv, const Tint& tint) {
    // Fog and lighting are computed per vertex, so a long strip of road or
    // water is cut into tiles of at most kTile metres to keep them smooth.
    constexpr float kTile = 25.0f;
    const int nx = std::max(1, static_cast<int>(std::ceil((r.x1 - r.x0) / kTile)));
    const int nz = std::max(1, static_cast<int>(std::ceil((r.z1 - r.z0) / kTile)));
    const float dx = (r.x1 - r.x0) / nx, dz = (r.z1 - r.z0) / nz;
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    for (int i = 0; i < nx; ++i) {
        const float x0 = r.x0 + dx * i, x1 = i + 1 == nx ? r.x1 : x0 + dx;
        for (int j = 0; j < nz; ++j) {
            const float z0 = r.z0 + dz * j, z1 = j + 1 == nz ? r.z1 : z0 + dz;
            const auto corner = [&](float x, float z) {
                if (tint) {
                    const glm::vec3 c = tint(x, z);
                    glColor3f(c.r, c.g, c.b);
                }
                vertex(x, y, z, x * uv, -z * uv);
            };
            corner(x0, z1);
            corner(x1, z1);
            corner(x1, z0);
            corner(x0, z0);
        }
    }
    glEnd();
}

void block(const glm::vec3& a, const glm::vec3& b, float uv, bool bottom) {
    const float x0 = a.x, y0 = a.y, z0 = a.z, x1 = b.x, y1 = b.y, z1 = b.z;
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);  // front (+z)
    vertex(x0, y0, z1, x0 * uv, y0 * uv); vertex(x1, y0, z1, x1 * uv, y0 * uv);
    vertex(x1, y1, z1, x1 * uv, y1 * uv); vertex(x0, y1, z1, x0 * uv, y1 * uv);
    glNormal3f(0, 0, -1);  // back
    vertex(x1, y0, z0, -x1 * uv, y0 * uv); vertex(x0, y0, z0, -x0 * uv, y0 * uv);
    vertex(x0, y1, z0, -x0 * uv, y1 * uv); vertex(x1, y1, z0, -x1 * uv, y1 * uv);
    glNormal3f(1, 0, 0);  // right (+x)
    vertex(x1, y0, z1, -z1 * uv, y0 * uv); vertex(x1, y0, z0, -z0 * uv, y0 * uv);
    vertex(x1, y1, z0, -z0 * uv, y1 * uv); vertex(x1, y1, z1, -z1 * uv, y1 * uv);
    glNormal3f(-1, 0, 0);  // left
    vertex(x0, y0, z0, z0 * uv, y0 * uv); vertex(x0, y0, z1, z1 * uv, y0 * uv);
    vertex(x0, y1, z1, z1 * uv, y1 * uv); vertex(x0, y1, z0, z0 * uv, y1 * uv);
    glNormal3f(0, 1, 0);  // top
    vertex(x0, y1, z1, x0 * uv, -z1 * uv); vertex(x1, y1, z1, x1 * uv, -z1 * uv);
    vertex(x1, y1, z0, x1 * uv, -z0 * uv); vertex(x0, y1, z0, x0 * uv, -z0 * uv);
    if (bottom) {
        glNormal3f(0, -1, 0);
        vertex(x0, y0, z0, x0 * uv, z0 * uv); vertex(x1, y0, z0, x1 * uv, z0 * uv);
        vertex(x1, y0, z1, x1 * uv, z1 * uv); vertex(x0, y0, z1, x0 * uv, z1 * uv);
    }
    glEnd();
}

void prism(float radius, float height, int sides, float uv) {
    const float step = 2.0f * kPi / static_cast<float>(sides);
    const float side = 2.0f * radius * std::sin(step * 0.5f);
    glBegin(GL_QUADS);
    for (int i = 0; i < sides; ++i) {
        const float a0 = step * static_cast<float>(i) - step * 0.5f, a1 = a0 + step, am = a0 + step * 0.5f;
        glNormal3f(std::cos(am), 0, -std::sin(am));
        const float u0 = static_cast<float>(i) * side * uv, u1 = u0 + side * uv;
        vertex(radius * std::cos(a0), 0, -radius * std::sin(a0), u0, 0);
        vertex(radius * std::cos(a1), 0, -radius * std::sin(a1), u1, 0);
        vertex(radius * std::cos(a1), height, -radius * std::sin(a1), u1, height * uv);
        vertex(radius * std::cos(a0), height, -radius * std::sin(a0), u0, height * uv);
    }
    glEnd();
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    vertex(0, height, 0, 0, 0);
    for (int i = 0; i <= sides; ++i) {
        const float a = step * static_cast<float>(i) - step * 0.5f;
        vertex(radius * std::cos(a), height, -radius * std::sin(a), radius * std::cos(a) * uv, radius * std::sin(a) * uv);
    }
    glEnd();
}

void frustum(float r0, float r1, float height, int sides, float uv, bool top) {
    const float step = 2.0f * kPi / static_cast<float>(sides);
    const float slope = (r0 - r1) / height;  // the normal leans up as the side narrows
    const float circumference = 2.0f * kPi * std::max(r0, r1);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= sides; ++i) {
        const float a = step * static_cast<float>(i), c = std::cos(a), s = -std::sin(a);
        const glm::vec3 n = glm::normalize(glm::vec3(c, slope, s));
        glNormal3f(n.x, n.y, n.z);
        const float u = static_cast<float>(i) / static_cast<float>(sides) * circumference * uv;
        vertex(r1 * c, height, r1 * s, u, height * uv);
        vertex(r0 * c, 0.0f, r0 * s, u, 0.0f);
    }
    glEnd();
    if (!top) return;
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    vertex(0, height, 0, 0, 0);
    for (int i = 0; i <= sides; ++i) {
        const float a = step * static_cast<float>(i);
        vertex(r1 * std::cos(a), height, -r1 * std::sin(a), r1 * std::cos(a) * uv, r1 * std::sin(a) * uv);
    }
    glEnd();
}

void ellipse(const glm::vec2& c, const glm::vec2& r, float y, int segments, float uv) {
    const float step = 2.0f * kPi / static_cast<float>(segments);
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    vertex(c.x, y, c.y, c.x * uv, -c.y * uv);
    for (int i = 0; i <= segments; ++i) {
        const float a = -step * static_cast<float>(i);  // counter-clockwise seen from above
        const float x = c.x + r.x * std::cos(a), z = c.y + r.y * std::sin(a);
        vertex(x, y, z, x * uv, -z * uv);
    }
    glEnd();
}

void ellipseRing(const glm::vec2& c, const glm::vec2& inner, const glm::vec2& outer, float y, int segments,
                 float uv) {
    const float step = 2.0f * kPi / static_cast<float>(segments);
    glBegin(GL_QUAD_STRIP);
    glNormal3f(0, 1, 0);
    for (int i = 0; i <= segments; ++i) {
        const float a = -step * static_cast<float>(i), ca = std::cos(a), sa = std::sin(a);
        const float xo = c.x + outer.x * ca, zo = c.y + outer.y * sa;
        const float xi = c.x + inner.x * ca, zi = c.y + inner.y * sa;
        vertex(xi, y, zi, xi * uv, -zi * uv);  // inner first keeps the band facing up
        vertex(xo, y, zo, xo * uv, -zo * uv);
    }
    glEnd();
}

void ellipseWall(const glm::vec2& c, const glm::vec2& r, float y0, float y1, int segments, float uv, bool inward) {
    const float step = 2.0f * kPi / static_cast<float>(segments);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; ++i) {
        const float a = -step * static_cast<float>(i), ca = std::cos(a), sa = std::sin(a);
        // The outward normal of an ellipse is (cos/rx, sin/rz), normalised.
        glm::vec2 n = glm::normalize(glm::vec2(ca / r.x, sa / r.y));
        if (inward) n = -n;
        glNormal3f(n.x, 0.0f, n.y);
        const float x = c.x + r.x * ca, z = c.y + r.y * sa, u = static_cast<float>(i) * step * r.x * uv;
        if (inward) {
            vertex(x, y0, z, u, y0 * uv);
            vertex(x, y1, z, u, y1 * uv);
        } else {
            vertex(x, y1, z, u, y1 * uv);
            vertex(x, y0, z, u, y0 * uv);
        }
    }
    glEnd();
}

}  // namespace citylab::geom

