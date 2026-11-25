#include "Architecture.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

namespace team12 {
namespace {

constexpr int facadeBays = 48;
constexpr int facadeRows = 4;
constexpr float fullTurn = glm::two_pi<float>();
constexpr float lowerWallHeight = 4.4f;
constexpr float facadeInset = 0.72f;

float wrap(float angle) {
    return angle - fullTurn * std::floor(angle / fullTurn);
}

void addBand(Mesh& mesh, glm::vec2 outer, glm::vec2 inner, float a, float b, float low, float high) {
    addWedge(mesh, {ovalPoint(outer, a), ovalPoint(inner, a),
                    ovalPoint(inner, b), ovalPoint(outer, b)}, low, high);
}

void addCushion(Mesh& mesh, glm::vec2 center, glm::vec2 tangent, glm::vec2 outward,
                float y, float halfWidth, float halfHeight) {
    const auto point = [&](float x, float h, float depth) {
        const auto p = center + tangent * x + outward * depth;
        return glm::vec3{p.x, y + h, p.y};
    };
    const std::array<glm::vec3, 4> front{point(halfWidth, 0.0f, 0.04f),
        point(0.0f, halfHeight, 0.04f), point(-halfWidth, 0.0f, 0.04f),
        point(0.0f, -halfHeight, 0.04f)};
    const std::array<glm::vec3, 4> back{point(halfWidth, 0.0f, -0.10f),
        point(0.0f, halfHeight, -0.10f), point(-halfWidth, 0.0f, -0.10f),
        point(0.0f, -halfHeight, -0.10f)};
    const auto bulge = point(0.0f, 0.0f, 0.54f);
    const auto rear = point(0.0f, 0.0f, -0.20f);
    for (int i = 0; i < 4; ++i) {
        const int next = (i + 1) % 4;
        addTriangle(mesh, front[i], front[next], bulge);
        addQuad(mesh, back[i], back[next], front[next], front[i]);
        addTriangle(mesh, back[next], back[i], rear);
    }
}

}  // namespace

StadiumShell makeStadiumShell(const StadiumLayout& layout, citylab::Context& ctx) {
    StadiumShell shell;
    const auto radii = layout.outerSize * 0.5f;
    const auto facade = radii - glm::vec2{facadeInset};
    const float bay = fullTurn / facadeBays;
    std::vector<float> breaks{0.0f, fullTurn};
    for (int i = 1; i < facadeBays; ++i) {
        breaks.push_back(i * bay);
    }
    for (const auto& opening : layout.shellOpenings) {
        breaks.push_back(wrap(opening.angle - opening.halfAngle));
        breaks.push_back(wrap(opening.angle + opening.halfAngle));
        const auto trimOuter = radii;
        const auto trimInner = radii - glm::vec2{1.08f};
        constexpr float jambAngle = 0.012f;
        const float left = opening.angle - opening.halfAngle;
        const float right = opening.angle + opening.halfAngle;
        addBand(shell.portalFrames, trimOuter, trimInner, left - jambAngle, left, 0.0f, opening.clearHeight);
        addBand(shell.portalFrames, trimOuter, trimInner, right, right + jambAngle, 0.0f, opening.clearHeight);
    }
    std::sort(breaks.begin(), breaks.end());
    for (std::size_t i = 1; i < breaks.size(); ++i) {
        const float a = breaks[i - 1], b = breaks[i];
        const float middle = (a + b) * 0.5f;
        float clearHeight = 0.0f;
        for (const auto& opening : layout.shellOpenings) {
            if (std::abs(std::remainder(middle - opening.angle, fullTurn)) < opening.halfAngle) {
                clearHeight = opening.clearHeight;
            }
        }
        addBand(shell.plinth, radii - glm::vec2{0.08f}, radii - glm::vec2{1.0f},
                a, b, clearHeight, lowerWallHeight);
        if (clearHeight == 0.0f) {
            const auto p = ovalPoint(radii - glm::vec2{0.55f}, a);
            const auto q = ovalPoint(radii - glm::vec2{0.55f}, b);
            const glm::vec2 origin{layout.stadiumCenter.x, layout.stadiumCenter.z};
            const auto lo = origin + glm::min(p, q) - glm::vec2{0.20f};
            const auto hi = origin + glm::max(p, q) + glm::vec2{0.20f};
            ctx.addCollider({lo.x, 0.0f, lo.y}, {hi.x, lowerWallHeight, hi.y});
        }
        addBand(shell.walls, facade - glm::vec2{0.23f}, facade - glm::vec2{0.42f},
                a, b, lowerWallHeight, layout.shellSpringHeight);
    }

    const float rowHeight = (layout.shellSpringHeight - lowerWallHeight) / facadeRows;
    for (int row = 0; row < facadeRows; ++row) {
        for (int section = 0; section < facadeBays; ++section) {
            const float angle = (section + (row % 2) * 0.5f) * bay;
            const glm::vec2 tangent = glm::normalize(glm::vec2{-facade.x * std::sin(angle), facade.y * std::cos(angle)});
            const glm::vec2 outward{tangent.y, -tangent.x};
            // Positive local X crossed with up points outward.
            const float halfWidth = glm::length(ovalPoint(facade, angle - bay * 0.5f) -
                                                ovalPoint(facade, angle + bay * 0.5f)) * 0.46f;
            const float height = lowerWallHeight + (row + 0.5f) * rowHeight;
            auto& panels = row == 0 ? shell.lightPanels :
                ((section + row / 2) % 7 < 5 ? shell.redPanels : shell.whitePanels);
            addCushion(panels, ovalPoint(facade, angle), -tangent, outward, height,
                       halfWidth, rowHeight * (row == 0 || row == facadeRows - 1 ? 0.47f : 0.63f));
        }
    }
    return shell;
}

void drawArchitecture(const StadiumShell& shell, float nightLight) {
    drawMesh(shell.walls, palette::steel);
    drawMesh(shell.plinth, palette::concrete);
    drawMesh(shell.portalFrames, {0.74f, 0.74f, 0.75f});
    glPushAttrib(GL_LIGHTING_BIT);
    const auto drawCushions = [nightLight](const Mesh& mesh, glm::vec3 paint, glm::vec3 glow) {
        const GLfloat emission[] = {glow.r * nightLight, glow.g * nightLight, glow.b * nightLight, 1.0f};
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emission);
        drawMesh(mesh, paint);
    };
    drawCushions(shell.redPanels, palette::facadeRed, {0.76f, 0.012f, 0.035f});
    drawCushions(shell.whitePanels, palette::facadeWhite, {0.72f, 0.75f, 0.81f});
    drawCushions(shell.lightPanels, {0.62f, 0.64f, 0.66f}, {0.27f, 0.29f, 0.32f});
    glPopAttrib();
}

}  // namespace team12
