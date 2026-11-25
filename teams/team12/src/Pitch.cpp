#include "Pitch.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

namespace team12 {
namespace {

constexpr float lineWidth = 0.13f;
constexpr float markingY = 0.035f;
constexpr float postWidth = 0.12f;
constexpr glm::vec3 white{0.97f, 0.97f, 0.94f};

void rectangle(Mesh& mesh, float xMin, float xMax, float zMin, float zMax, float y) {
    addQuad(mesh, {xMin, y, zMax}, {xMax, y, zMax}, {xMax, y, zMin}, {xMin, y, zMin});
}

void crossField(Mesh& mesh, float x, float zMin, float zMax) {
    rectangle(mesh, x - lineWidth * 0.5f, x + lineWidth * 0.5f, zMin, zMax, markingY);
}

void alongField(Mesh& mesh, float z, float xMin, float xMax) {
    rectangle(mesh, xMin, xMax, z - lineWidth * 0.5f, z + lineWidth * 0.5f, markingY);
}

void arc(Mesh& mesh, glm::vec2 center, float radius, float start, float end, int segments) {
    for (int i = 0; i < segments; ++i) {
        const float a = start + (end - start) * i / segments;
        const float b = start + (end - start) * (i + 1) / segments;
        const auto vertex = [&](float r, float angle) {
            const auto p = center + ovalPoint(glm::vec2{r}, angle);
            return glm::vec3{p.x, markingY, p.y};
        };
        addQuad(mesh, vertex(radius + lineWidth * 0.5f, a), vertex(radius - lineWidth * 0.5f, a),
                vertex(radius - lineWidth * 0.5f, b), vertex(radius + lineWidth * 0.5f, b));
    }
}

void spot(Mesh& mesh, glm::vec2 center) {
    constexpr int segments = 12;
    constexpr float radius = 0.17f;
    for (int i = 0; i < segments; ++i) {
        const auto a = center + ovalPoint(glm::vec2{radius}, glm::two_pi<float>() * i / segments);
        const auto b = center + ovalPoint(glm::vec2{radius}, glm::two_pi<float>() * (i + 1) / segments);
        addTriangle(mesh, {center.x, markingY, center.y}, {b.x, markingY, b.y}, {a.x, markingY, a.y});
    }
}

void markBox(Mesh& mesh, float goalX, float depth, float halfWidth) {
    const float inside = goalX - std::copysign(depth, goalX);
    crossField(mesh, inside, -halfWidth, halfWidth);
    for (const float z : {-halfWidth, halfWidth}) {
        alongField(mesh, z, std::min(inside, goalX), std::max(inside, goalX));
    }
}

void netPanel(std::vector<glm::vec3>& lines, glm::vec3 a, glm::vec3 b,
              glm::vec3 c, glm::vec3 d, int across, int down) {
    for (int i = 0; i <= across; ++i) {
        const float t = static_cast<float>(i) / across;
        lines.push_back(glm::mix(a, b, t));
        lines.push_back(glm::mix(d, c, t));
    }
    // The adjacent panels share the perimeter. Keep only the interior transverse runs.
    for (int i = 1; i < down; ++i) {
        const float t = static_cast<float>(i) / down;
        lines.push_back(glm::mix(a, d, t));
        lines.push_back(glm::mix(b, c, t));
    }
}

void buildNet(PitchScene& scene) {
    const float z = scene.goalWidth * 0.5f;
    const float y = scene.goalHeight;
    const float rear = scene.rearDepth;
    const float upper = rear * 0.43f;

    netPanel(scene.netLines, {0,y,-z}, {0,y,z}, {upper,y,z}, {upper,y,-z}, 22, 5);
    netPanel(scene.netLines, {upper,y,-z}, {upper,y,z}, {rear,0,z}, {rear,0,-z}, 22, 10);
    for (const float side : {-z, z}) {
        netPanel(scene.netLines, {0,y,side}, {upper,y,side}, {rear,0,side}, {0,0,side}, 9, 10);
    }
}

void drawGoal(const PitchScene& scene) {
    const float z = scene.goalWidth * 0.5f;
    const float y = scene.goalHeight;
    const float rear = scene.rearDepth;
    const float upper = rear * 0.43f;
    for (const float side : {-z, z}) {
        drawBox({0.0f,y * 0.5f,side}, {postWidth,y,postWidth}, white);
        drawBox({rear * 0.5f,0.045f,side}, {rear,0.09f,0.08f}, white);
        drawBox({upper * 0.5f,y,side}, {upper,0.065f,0.065f}, white);
        const ScopedMatrix strut;
        glTranslatef((rear + upper) * 0.5f, y * 0.5f, side);
        glRotatef(-glm::degrees(std::atan2(y, rear - upper)), 0.0f, 0.0f, 1.0f);
        drawBox({}, {std::hypot(y, rear - upper),0.065f,0.065f}, white);
    }
    drawBox({0.0f,y,0.0f}, {postWidth,postWidth,scene.goalWidth + postWidth}, white);
    drawBox({rear,0.045f,0.0f}, {0.08f,0.09f,scene.goalWidth}, white);

    glColor3f(0.78f, 0.81f, 0.84f);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_LINES);
    for (const auto& vertex : scene.netLines) {
        glVertex3f(vertex.x, vertex.y, vertex.z);
    }
    glEnd();
}

}  // namespace

PitchScene makePitch(const StadiumLayout& layout) {
    PitchScene scene{};
    const float x = layout.pitch.size.x * 0.5f;
    const float z = layout.pitch.size.y * 0.5f;
    const float scale = std::min(x / 35.0f, z / 18.0f);
    scene.halfLength = x;
    scene.goalWidth = std::min(5.8f * scale, layout.goalClearance.y - postWidth);
    scene.goalHeight = 2.15f * scale;
    scene.rearDepth = layout.goalClearance.x * 0.80f;

    constexpr int stripes = 10;
    for (int i = 0; i < stripes; ++i) {
        rectangle(scene.grass[i % 2], -x + 2.0f * x * i / stripes, -x + 2.0f * x * (i + 1) / stripes,
                  -z, z, 0.0f);
    }
    // Continue the turf under the goal stays, inside the shared clearance envelope.
    const float endRunoff = layout.goalClearance.x * 0.88f;
    rectangle(scene.runoff, -x - endRunoff, -x, -z, z, 0.0f);
    rectangle(scene.runoff, x, x + endRunoff, -z, z, 0.0f);
    rectangle(scene.runoff, -x, x, -z - layout.sidelineClearance, -z, 0.0f);
    rectangle(scene.runoff, -x, x, z, z + layout.sidelineClearance, 0.0f);
    auto& marks = scene.markings;
    for (const float side : {-z, z}) alongField(marks, side, -x, x);
    for (const float line : {-x, 0.0f, x}) crossField(marks, line, -z, z);

    constexpr float quarter = glm::half_pi<float>();
    const float radius = 5.9f * scale;
    arc(marks, {0,0}, radius, 0.0f, glm::two_pi<float>(), 48);
    spot(marks, {0,0});
    const float penaltyDepth = 10.8f * scale;
    const float spotDepth = 7.2f * scale;
    const float spread = std::acos((penaltyDepth - spotDepth) / radius);
    for (const float sign : {-1.0f, 1.0f}) {
        const float goalLine = sign * x;
        const float spotX = sign * (x - spotDepth);
        markBox(marks, goalLine, penaltyDepth, 12.0f * scale);
        markBox(marks, goalLine, 3.6f * scale, 6.0f * scale);
        spot(marks, {spotX,0});
        const float intoField = sign > 0.0f ? glm::pi<float>() : 0.0f;
        arc(marks, {spotX,0}, radius, intoField - spread, intoField + spread, 20);
    }
    arc(marks, {-x,-z}, 0.72f * scale, 0.0f, quarter, 8);
    arc(marks, { x,-z}, 0.72f * scale, quarter, 2.0f * quarter, 8);
    arc(marks, { x, z}, 0.72f * scale, 2.0f * quarter, 3.0f * quarter, 8);
    arc(marks, {-x, z}, 0.72f * scale, 3.0f * quarter, 4.0f * quarter, 8);
    buildNet(scene);
    return scene;
}

void drawPitch(const StadiumLayout& layout, const PitchScene& scene) {
    const ScopedMatrix pitchFrame;
    glTranslatef(layout.pitch.center.x, previewMarkingHeight, layout.pitch.center.y);
    glRotatef(layout.pitchYawDegrees, 0.0f, 1.0f, 0.0f);
    drawMesh(scene.runoff, {0.13f, 0.34f, 0.16f});
    drawMesh(scene.grass[0], {0.15f, 0.44f, 0.19f});
    drawMesh(scene.grass[1], {0.11f, 0.37f, 0.15f});
    drawMesh(scene.markings, white);
    for (const float sign : {-1.0f, 1.0f}) {
        const ScopedMatrix goalFrame;
        glTranslatef(sign * scene.halfLength, 0.0f, 0.0f);
        glRotatef(sign < 0.0f ? 180.0f : 0.0f, 0.0f, 1.0f, 0.0f);
        drawGoal(scene);
    }
}

}  // namespace team12
