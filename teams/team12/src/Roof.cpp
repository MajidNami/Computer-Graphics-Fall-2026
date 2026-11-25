#include "Roof.h"

#include <cmath>
#include <string_view>
#include <glm/gtc/constants.hpp>

namespace team12 {
namespace {

glm::vec2 aperturePoint(glm::vec2 radii, float angle) {
    const glm::vec2 direction{std::cos(angle), std::sin(angle)};
    const auto squared = direction * direction;
    return radii * direction / std::sqrt(std::sqrt(glm::dot(squared, squared)));
}

struct BannerGlyph {
    char letter;
    std::array<unsigned, 7> rows;
};

constexpr BannerGlyph glyphs[] = {
    {'A', {14,17,17,31,17,17,17}}, {'B', {30,17,17,30,17,17,30}},
    {'C', {14,17,16,16,16,17,14}}, {'E', {31,16,16,30,16,16,31}},
    {'F', {31,16,16,30,16,16,16}}, {'I', {31,4,4,4,4,4,31}},
    {'M', {17,27,21,21,17,17,17}}, {'N', {17,25,21,19,17,17,17}},
    {'R', {30,17,17,30,20,18,17}}, {'S', {15,16,16,14,1,1,30}},
    {'Y', {17,17,10,4,4,4,4}},
    {'L', {16,16,16,16,16,16,31}}, {'O', {14,17,17,17,17,17,14}},
    {'8', {14,17,17,14,17,17,14}}, {'2', {14,17,1,2,4,8,31}},
    {'9', {14,17,17,15,1,1,14}}, {'0', {14,17,17,17,17,17,14}},
    {':', {0,0,4,0,4,0,0}}, {'-', {0,0,0,31,0,0,0}},
};

void addWallStrip(Mesh& mesh, glm::vec2 radii, float a, float b, float low, float high) {
    const auto p = ovalPoint(radii, a);
    const auto q = ovalPoint(radii, b);
    addQuad(mesh, {p.x, low, p.y}, {q.x, low, q.y}, {q.x, high, q.y}, {p.x, high, p.y});
}

void addWallText(Mesh& mesh, glm::vec2 print, float center, std::string_view words,
                 float topY, float pixelAngle, float pixelHeight) {
    float cursor = center - (words.size() * 6 - 1) * pixelAngle * 0.5f;
    for (const char letter : words) {
        for (const auto& glyph : glyphs) {
            if (glyph.letter != letter) continue;
            for (int row = 0; row < 7; ++row) {
                const float top = topY - row * pixelHeight;
                const auto painted = [&](int column) { return (glyph.rows[row] & (16u >> column)) != 0; };
                for (int column = 0; column < 5;) {
                    if (!painted(column)) {
                        ++column;
                        continue;
                    }
                    const int start = column++;
                    while (column < 5 && painted(column)) ++column;
                    addWallStrip(mesh, print, cursor + start * pixelAngle,
                                 cursor + column * pixelAngle, top - pixelHeight, top);
                }
            }
        }
        cursor += 6.0f * pixelAngle;
    }
}

void addBanner(StadiumRoof& roof, glm::vec2 outer, float center, std::string_view words) {
    constexpr float halfAngle = 0.47f;
    constexpr int segments = 16;
    const auto fabric = outer - glm::vec2{1.54f};
    const auto print = fabric - glm::vec2{0.10f};
    for (int i = 0; i < segments; ++i) {
        const float a = center - halfAngle + 2.0f * halfAngle * i / segments;
        const float b = a + 2.0f * halfAngle / segments;
        addWallStrip(roof.banners, fabric, a, b, 8.0f, 14.1f);
        addWallStrip(roof.lettering, print, a, b, 8.30f, 8.43f);
        addWallStrip(roof.lettering, print, a, b, 13.67f, 13.80f);
    }
    addWallText(roof.lettering, print, center, words, 12.96f, 0.012f, 0.55f);
}

void addScoreboard(StadiumRoof& roof, glm::vec2 outer) {
    const auto panel = outer - glm::vec2{1.95f};
    const auto screen = panel - glm::vec2{0.16f};
    constexpr int segments = 16;
    constexpr float halfAngle = 0.46f;
    for (int i = 0; i < segments; ++i) {
        const float a = -halfAngle + 2.0f * halfAngle * i / segments;
        const float b = a + 2.0f * halfAngle / segments;
        addWallStrip(roof.scoreboard, panel, a, b, 8.0f, 17.0f);
        addWallStrip(roof.clockText, screen, a, b, 8.45f, 8.59f);
        addWallStrip(roof.clockText, screen, a, b, 16.41f, 16.55f);
    }
    addWallText(roof.scoreText, screen, 0.0f, "BAYERN 8-2 BARCELONA", 15.8f, 0.0068f, 0.51f);
    addWallText(roof.clockText, screen, 0.0f, "90:00", 11.5f, 0.0105f, 0.35f);
}

void enableBannerLight(GLenum light, glm::vec3 position, float intensity) {
    const GLfloat location[]{position.x, position.y, position.z, 1.0f};
    const GLfloat aim[]{0.0f, -0.75f, std::copysign(0.66f, position.z)};
    const GLfloat color[]{intensity, intensity * 0.74f, intensity * 0.67f, 1.0f};
    glLightfv(light, GL_POSITION, location);
    glLightfv(light, GL_SPOT_DIRECTION, aim);
    glLightfv(light, GL_DIFFUSE, color);
    glLightf(light, GL_SPOT_CUTOFF, 80.0f);
    glLightf(light, GL_SPOT_EXPONENT, 1.0f);
    glEnable(light);
}

void drawInteriorSigns(const StadiumRoof& roof, float nightLight) {
    for (const auto position : roof.bannerFixtures) {
        const float wallward = std::copysign(1.0f, position.z);
        drawBox(position + glm::vec3{0.0f, 0.06f, wallward * 0.94f},
                {0.14f, 0.12f, 1.95f}, palette::steel);
        drawBox(position, {0.80f, 0.26f, 0.50f}, palette::steel);
    }
    glPushAttrib(GL_LIGHTING_BIT);
    const GLfloat lamp[]{0.84f * nightLight, 0.68f * nightLight, 0.48f * nightLight, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, lamp);
    for (const auto p : roof.bannerFixtures) {
        const float y = p.y - 0.14f;
        glColor3f(0.76f, 0.68f, 0.52f);
        glNormal3f(0.0f, -1.0f, 0.0f);
        glBegin(GL_QUADS);
        glVertex3f(p.x - 0.34f, y, p.z - 0.20f);
        glVertex3f(p.x + 0.34f, y, p.z - 0.20f);
        glVertex3f(p.x + 0.34f, y, p.z + 0.20f);
        glVertex3f(p.x - 0.34f, y, p.z + 0.20f);
        glEnd();
    }
    if (nightLight > 0.0f) {
        enableBannerLight(GL_LIGHT2, roof.bannerFixtures[1], nightLight * 0.93f);
        enableBannerLight(GL_LIGHT3, roof.bannerFixtures[4], nightLight * 0.93f);
    }
    const GLfloat bannerGlow[]{0.18f * nightLight, 0.018f * nightLight, 0.036f * nightLight, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, bannerGlow);
    drawMesh(roof.banners, {0.54f, 0.035f, 0.10f});
    const GLfloat whiteGlow[]{0.39f * nightLight, 0.37f * nightLight, 0.33f * nightLight, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, whiteGlow);
    drawMesh(roof.lettering, {0.97f, 0.95f, 0.92f});
    glPopAttrib();

    drawMesh(roof.scoreboard, {0.025f, 0.035f, 0.052f});
    glPushAttrib(GL_LIGHTING_BIT);
    const GLfloat display[]{0.72f, 0.76f, 0.81f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, display);
    drawMesh(roof.scoreText, {0.22f, 0.25f, 0.28f});
    const GLfloat clock[]{0.87f, 0.58f, 0.12f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, clock);
    drawMesh(roof.clockText, {0.22f, 0.15f, 0.03f});
    glPopAttrib();
}

}  // namespace

StadiumRoof makeStadiumRoof(const StadiumLayout& layout) {
    StadiumRoof roof;
    constexpr int segments = 64;
    constexpr float fullTurn = glm::two_pi<float>();
    const auto outer = layout.outerSize * 0.5f;
    const auto inner = layout.innerSize * 0.5f;
    // A rounded rectangle shades the touchlines while retaining the pitch corners.
    const auto opening = inner * glm::vec2{41.0f / 48.0f, 25.5f / 30.0f};
    const auto highEdge = inner + glm::vec2{2.2f};
    const auto point = [](glm::vec2 radii, float angle, float y) {
        const auto p = ovalPoint(radii, angle);
        return glm::vec3{p.x, y, p.y};
    };
    const auto profile = [&](float angle) -> std::array<glm::vec3, 6> {
        const auto lip = aperturePoint(opening, angle);
        return {point(outer, angle, layout.shellSpringHeight + 0.20f),
                point(highEdge, angle, layout.roofHeight),
                {lip.x, layout.roofHeight - 0.35f, lip.y},
                {lip.x, layout.roofHeight - 0.78f, lip.y},
                point(highEdge, angle, layout.roofHeight - 0.50f),
                point(outer, angle, layout.shellSpringHeight)};
    };
    const auto first = profile(0.0f);
    auto a = first;
    for (int i = 1; i <= segments; ++i) {
        const auto b = i == segments ? first : profile(i * fullTurn / segments);
        for (int edge = 0; edge < static_cast<int>(a.size()); ++edge) {
            const int next = (edge + 1) % static_cast<int>(a.size());
            auto& mesh = edge == 2 || edge == 5 ? roof.fascia : (edge == 1 || edge == 3 ? roof.liner : roof.membrane);
            addQuad(mesh, a[edge], a[next], b[next], b[edge]);
        }
        a = b;
    }

    constexpr int supportCount = 24;
    for (int i = 0; i < supportCount; ++i) {
        const float angle = (i + 0.5f) * fullTurn / supportCount;
        const auto column = point(outer - glm::vec2{1.14f}, angle, layout.shellSpringHeight + 0.68f);
        const auto tunnel = layout.playerTunnel;
        if (column.x > tunnel.center.x - tunnel.size.x * 0.5f &&
            std::abs(column.z - tunnel.center.y) < tunnel.size.y * 0.5f + 0.45f) {
            continue;
        }
        const auto lip = aperturePoint(opening + glm::vec2{0.3f}, angle);
        const glm::vec3 tip{lip.x, layout.roofHeight - 0.83f, lip.y};
        const auto delta = tip - column;
        roof.supports.push_back({column, (column + tip) * 0.5f, glm::length(delta),
            glm::degrees(std::atan2(delta.x, delta.z)),
            -glm::degrees(std::atan2(delta.y, glm::length(glm::vec2{delta.x, delta.z})))});
    }
    roof.canopyHinge = {layout.mainEntrance.x, 4.8f, layout.mainEntrance.y - 0.4f};
    addBanner(roof, outer, glm::half_pi<float>(), "MIA SAN MIA");
    addBanner(roof, outer, 3.0f * glm::half_pi<float>(), "FC BAYERN");
    addScoreboard(roof, outer);
    std::size_t fixtureIndex = 0;
    for (const float center : {glm::half_pi<float>(), 3.0f * glm::half_pi<float>()}) {
        for (const float offset : {-0.29f, 0.0f, 0.29f}) {
            roof.bannerFixtures[fixtureIndex++] = point(outer - glm::vec2{3.0f}, center + offset, 15.0f);
        }
    }
    for (int i = 0; i < segments; ++i) {
        addWallStrip(roof.banners, outer - glm::vec2{1.48f}, i * fullTurn / segments,
                     (i + 1) * fullTurn / segments, 6.4f, 7.05f);
    }
    return roof;
}

void drawStadiumRoof(const StadiumRoof& roof, float nightLight) {
    drawMesh(roof.membrane, {0.78f, 0.80f, 0.83f});
    drawMesh(roof.liner, {0.88f, 0.91f, 0.93f});
    drawMesh(roof.fascia, palette::steel);
    drawInteriorSigns(roof, nightLight);
    for (const auto& support : roof.supports) {
        const auto top = support.columnTop;
        drawBox({top.x, top.y * 0.5f, top.z}, {0.22f, top.y, 0.22f}, palette::concrete);
        const ScopedMatrix beam;
        const auto center = support.beamCenter;
        glTranslatef(center.x, center.y, center.z);
        glRotatef(support.yaw, 0.0f, 1.0f, 0.0f);
        glRotatef(support.pitch, 1.0f, 0.0f, 0.0f);
        glScalef(0.30f, 0.34f, support.beamLength);
        drawBox({}, {1.0f, 1.0f, 1.0f}, palette::steel);
    }

    const ScopedMatrix canopy;
    const auto hinge = roof.canopyHinge;
    glTranslatef(hinge.x, hinge.y, hinge.z);  // T(p): hinge at the entrance lintel.
    glRotatef(12.0f, 1.0f, 0.0f, 0.0f);  // R: static tilt about the hinge axis.
    glTranslatef(-hinge.x, -hinge.y, -hinge.z);  // T(-p): move the hinge to the origin.
    drawBox({hinge.x, hinge.y, hinge.z - 1.3f}, {5.25f, 0.20f, 2.6f}, palette::steel);
    for (const float x : {-2.2f, 2.2f}) {
        drawBox({hinge.x + x, hinge.y - 0.17f, hinge.z - 1.3f}, {0.15f, 0.14f, 2.6f}, palette::concrete);
    }
}

}  // namespace team12
