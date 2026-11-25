#include "Geometry.h"

#include <cmath>
#include <citylab/CityLab.h>

namespace team12 {

ScopedMatrix::ScopedMatrix() { glPushMatrix(); }
ScopedMatrix::~ScopedMatrix() { glPopMatrix(); }

glm::vec2 ovalPoint(const glm::vec2& radii, float angle) {
    return radii * glm::vec2{std::cos(angle), std::sin(angle)};
}

void addTriangle(Mesh& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c) {
    mesh.push_back({a, b, c, glm::normalize(glm::cross(b - a, c - a))});
}

void addQuad(Mesh& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
    addTriangle(mesh, a, b, c);
    addTriangle(mesh, a, c, d);
}

void addWedge(Mesh& mesh, const std::array<glm::vec2, 4>& outline, float bottom, float top) {
    // The outline is counterclockwise viewed from +Y.
    std::array<glm::vec3, 4> low{}, high{};
    for (int i = 0; i < 4; ++i) {
        low[i] = {outline[i].x, bottom, outline[i].y};
        high[i] = {outline[i].x, top, outline[i].y};
    }
    addQuad(mesh, high[0], high[1], high[2], high[3]);
    addQuad(mesh, low[3], low[2], low[1], low[0]);
    for (int i = 0; i < 4; ++i) {
        const int next = (i + 1) % 4;
        addQuad(mesh, low[i], low[next], high[next], high[i]);
    }
}

void drawMesh(const Mesh& mesh, const glm::vec3& color) {
    glColor3f(color.r, color.g, color.b);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_TRIANGLES);
    for (const auto& face : mesh) {
        glNormal3f(face.normal.x, face.normal.y, face.normal.z);
        glVertex3f(face.a.x, face.a.y, face.a.z);
        glVertex3f(face.b.x, face.b.y, face.b.z);
        glVertex3f(face.c.x, face.c.y, face.c.z);
    }
    glEnd();
}

void drawBox(glm::vec3 center, glm::vec3 size, glm::vec3 color) {
    const ScopedMatrix matrix;
    glTranslatef(center.x, center.y, center.z);
    glColor3f(color.r, color.g, color.b);
    citylab::shapes::box(size.x, size.y, size.z);
}

void drawPad(const Footprint& footprint, float height, const glm::vec3& color, float yawDegrees) {
    const ScopedMatrix matrix;
    glTranslatef(footprint.center.x, height, footprint.center.y);
    glRotatef(yawDegrees, 0.0f, 1.0f, 0.0f);
    glColor3f(color.r, color.g, color.b);
    citylab::shapes::plane(footprint.size.x, footprint.size.y);
}

}  // namespace team12
