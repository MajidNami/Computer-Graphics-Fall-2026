#pragma once

#include "StadiumLayout.h"

namespace team12 {

inline constexpr float previewMarkingHeight = 0.23f;

class ScopedMatrix {
public:
    ScopedMatrix();
    ~ScopedMatrix();
    ScopedMatrix(const ScopedMatrix&) = delete;
    ScopedMatrix& operator=(const ScopedMatrix&) = delete;
};

struct Triangle {
    glm::vec3 a;
    glm::vec3 b;
    glm::vec3 c;
    glm::vec3 normal;
};

using Mesh = std::vector<Triangle>;

glm::vec2 ovalPoint(const glm::vec2& radii, float angle);
void addTriangle(Mesh& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c);
void addQuad(Mesh& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d);
void addWedge(Mesh& mesh, const std::array<glm::vec2, 4>& outline, float bottom, float top);
void drawMesh(const Mesh& mesh, const glm::vec3& color);
void drawBox(glm::vec3 center, glm::vec3 size, glm::vec3 color);
void drawPad(const Footprint& footprint, float height, const glm::vec3& color, float yawDegrees = 0.0f);

}  // namespace team12
