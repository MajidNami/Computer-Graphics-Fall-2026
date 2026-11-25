#include "Landscape.h"

#include <algorithm>
#include <cmath>
#include <citylab/CityLab.h>

namespace team12 {
namespace {

constexpr glm::vec3 lawn{0.24f, 0.36f, 0.22f};
constexpr glm::vec3 foliage{0.17f, 0.31f, 0.19f};
constexpr glm::vec3 wood{0.37f, 0.22f, 0.13f};
constexpr glm::vec3 ivory{0.93f, 0.91f, 0.83f};

glm::vec3 truncate(glm::vec3 from, glm::vec3 toward) {
    return (from * 2.0f + toward) / 3.0f;
}

template<std::size_t N>
void addBallPanel(Mesh& mesh, std::array<glm::vec3, N> corners) {
    glm::vec3 center{};
    for (const auto corner : corners) {
        center += corner;
    }
    center /= static_cast<float>(N);
    if (glm::dot(glm::cross(corners[1] - corners[0], corners[2] - corners[0]), center) < 0.0f) {
        std::reverse(corners.begin(), corners.end());
    }
    for (std::size_t i = 1; i + 1 < N; ++i) {
        addTriangle(mesh, corners[0], corners[i], corners[i + 1]);
    }
}

void buildFootball(StadiumGrounds& grounds) {
    const float golden = (1.0f + std::sqrt(5.0f)) * 0.5f;
    const std::array<glm::vec3, 12> corners{{
        {0,-1,-golden}, {0,-1,golden}, {0,1,-golden}, {0,1,golden},
        {-1,-golden,0}, {-1,golden,0}, {1,-golden,0}, {1,golden,0},
        {-golden,0,-1}, {golden,0,-1}, {-golden,0,1}, {golden,0,1},
    }};
    const auto adjacent = [&](int a, int b) {
        const auto delta = corners[a] - corners[b];
        return std::abs(glm::dot(delta, delta) - 4.0f) < 0.001f;
    };
    for (int a = 0; a < 12; ++a) {
        std::array<glm::vec3, 5> pentagon{};
        std::size_t count = 0;
        for (int b = 0; b < 12; ++b) {
            if (adjacent(a, b)) pentagon[count++] = truncate(corners[a], corners[b]);
        }
        const auto axis = glm::normalize(corners[a]);
        const auto right = glm::normalize(pentagon[0] - axis * glm::dot(axis, pentagon[0]));
        const auto up = glm::cross(axis, right);
        const auto angle = [&](glm::vec3 v) { return std::atan2(glm::dot(v, up), glm::dot(v, right)); };
        std::sort(pentagon.begin(), pentagon.end(),
            [&](glm::vec3 lhs, glm::vec3 rhs) { return angle(lhs) < angle(rhs); });
        addBallPanel(grounds.blackBallPanels, pentagon);

        for (int b = a + 1; b < 12; ++b) {
            if (!adjacent(a, b)) continue;
            for (int c = b + 1; c < 12; ++c) {
                if (!adjacent(a, c) || !adjacent(b, c)) continue;
                const auto x = corners[a], y = corners[b], z = corners[c];
                addBallPanel(grounds.whiteBallPanels, std::array<glm::vec3, 6>{
                    truncate(x, y), truncate(y, x), truncate(y, z),
                    truncate(z, y), truncate(z, x), truncate(x, z)});
            }
        }
    }
}

void drawMonument(const StadiumLayout& layout, const StadiumGrounds& grounds) {
    const ScopedMatrix monument;
    glTranslatef(layout.monument.center.x, 0.02f, layout.monument.center.y);
    const float scale = layout.monument.size.x / 5.0f;
    glScalef(scale, 1.0f, scale);
    drawBox({0,0.16f,0}, {4.6f,0.32f,4.6f}, palette::concrete);
    drawBox({0,0.40f,0}, {3.9f,0.19f,3.9f}, palette::steel);
    drawBox({0,1.70f,0}, {1.85f,2.43f,1.85f}, palette::facadeRed);
    drawBox({0,2.98f,0}, {2.2f,0.18f,2.2f}, ivory);
    drawBox({0,1.30f,0.94f}, {1.55f,0.50f,0.08f}, palette::steel);
    const ScopedMatrix ball;
    glTranslatef(0.0f, 4.19f, 0.0f);
    glRotatef(18.0f, 0.0f, 0.0f, 1.0f);
    glScalef(0.75f, 0.75f, 0.75f);
    drawMesh(grounds.whiteBallPanels, ivory);
    drawMesh(grounds.blackBallPanels, {0.08f, 0.09f, 0.10f});
}

void drawTree() {
    glColor3f(wood.r, wood.g, wood.b);
    citylab::shapes::cylinder(0.15f, 1.45f, 6);
    glTranslatef(0.0f, 1.02f, 0.0f);
    glColor3f(foliage.r, foliage.g, foliage.b);
    citylab::shapes::cone(1.20f, 2.5f, 8);
    glTranslatef(0.0f, 1.02f, 0.0f);
    glColor3f(0.22f, 0.38f, 0.24f);
    citylab::shapes::cone(0.83f, 1.9f, 8);
}

void drawLamp() {
    drawBox({0,0.11f,0}, {0.42f,0.22f,0.42f}, palette::steel);
    drawBox({0,1.49f,0}, {0.13f,2.65f,0.13f}, palette::steel);
    drawBox({0,2.86f,0.22f}, {0.26f,0.14f,0.58f}, palette::steel);
    drawBox({0,2.76f,0.36f}, {0.20f,0.07f,0.26f}, ivory);
}

void drawBench() {
    drawBox({0,0.49f,0}, {1.75f,0.13f,0.45f}, wood);
    drawBox({0,0.84f,-0.19f}, {1.75f,0.27f,0.10f}, wood);
    for (const float x : {-0.65f, 0.65f}) {
        drawBox({x,0.28f,-0.14f}, {0.10f,0.54f,0.32f}, palette::steel);
    }
}

void drawBollard() {
    drawBox({0,0.07f,0}, {0.30f,0.14f,0.30f}, palette::steel);
    drawBox({0,0.43f,0}, {0.17f,0.70f,0.17f}, palette::concrete);
    drawBox({0,0.77f,0}, {0.19f,0.09f,0.19f}, palette::facadeRed);
}

void drawObjects(const std::vector<LandscapeObject>& objects, void (*draw)()) {
    for (const auto& object : objects) {
        const ScopedMatrix matrix;
        glTranslatef(object.position.x, 0.015f, object.position.y);
        glRotatef(object.yaw, 0.0f, 1.0f, 0.0f);
        glScalef(object.scale, object.scale, object.scale);
        draw();
    }
}

}  // namespace

StadiumGrounds makeGrounds(const StadiumLayout& layout, citylab::Context& ctx) {
    StadiumGrounds grounds;
    const float scale = layout.outerSize.x / 112.0f;
    const auto apron = layout.paths[1];
    grounds.plazaApron = {{apron.center.x, apron.center.y + 0.85f * apron.size.y},
                          {apron.size.x * 0.82f, apron.size.y * 0.68f}};
    const auto garden = [&](glm::vec2 center, glm::vec2 size) {
        grounds.gardens.push_back({center * scale, size * scale});
    };
    garden({-18,-46.5f}, {117,4});
    garden({-77,0}, {4,89});
    garden({76,0}, {5,91});
    garden({-50,39}, {51,15});
    garden({28,44}, {17,9});
    garden({37,-35}, {14,12});
    garden({-64,-36}, {11,6});
    garden({-68,28}, {9,5});

    for (const float x : {-71.0f,-51.0f,-31.0f,-11.0f,9.0f,29.0f}) {
        grounds.trees.push_back({glm::vec2{x,-46.3f} * scale, x, 0.97f * scale});
    }
    for (const float z : {-33.0f,-17.0f,-1.0f,15.0f,30.0f}) {
        grounds.trees.push_back({glm::vec2{-77.0f,z} * scale, z, 1.0f * scale});
        grounds.trees.push_back({glm::vec2{76.0f,z} * scale, -z, 0.82f * scale});
    }
    for (const glm::vec2 position : {glm::vec2{-66,40}, {-54,40}, {-43,40}, {-33,42},
                                    {34,44}, {40,-36}, {-65,-36}, {-68,28}}) {
        grounds.trees.push_back({position * scale, position.x, 1.05f * scale});
    }
    for (const float x : {-22.0f,-9.0f,8.0f,23.0f,35.0f,48.0f,60.0f}) {
        grounds.lamps.push_back({glm::vec2{x,38.8f} * scale, 180.0f, (x < 0 ? 1.0f : 0.88f) * scale});
    }
    for (const float z : {-18.0f,-3.0f,13.0f,27.0f}) {
        grounds.lamps.push_back({glm::vec2{45.8f,z} * scale, -90.0f, 0.92f * scale});
    }
    for (const float x : {-28.0f,-12.0f,21.0f,31.0f,51.0f}) {
        grounds.benches.push_back({glm::vec2{x,39.9f} * scale, x < 0 ? 150.0f : 180.0f, scale});
    }
    for (const float x : {-17.0f, -14.0f, -11.0f, -8.0f, 8.0f, 22.0f, 25.0f, 28.0f}) {
        grounds.bollards.push_back({glm::vec2{x,42.2f} * scale, x < 0.0f ? 0.0f : 20.0f,
                                    (x < 0.0f ? 0.92f : 1.0f) * scale});
    }
    const auto monument = layout.monument.center;
    const auto half = layout.monument.size * 0.46f;
    ctx.addCollider({monument.x - half.x, 0.0f, monument.y - half.y},
                    {monument.x + half.x, 3.0f, monument.y + half.y});
    buildFootball(grounds);
    return grounds;
}

void drawLandscape(const StadiumLayout& layout, const StadiumGrounds& grounds) {
    drawPad({{0.0f,0.0f}, {layout.plot.width, layout.plot.depth}}, 0.010f, palette::ground);
    for (const auto& garden : grounds.gardens) {
        drawPad(garden, 0.10f, lawn);
    }
    for (const auto& path : layout.paths) {
        drawPad(path, 0.12f, palette::paving);
    }
    drawPad(layout.playerRoute, 0.12f, palette::concrete);
    drawPad(grounds.plazaApron, 0.09f, {0.53f,0.51f,0.48f});
    drawObjects(grounds.trees, drawTree);
    drawObjects(grounds.lamps, drawLamp);
    drawObjects(grounds.benches, drawBench);
    drawObjects(grounds.bollards, drawBollard);
    drawMonument(layout, grounds);
}

}  // namespace team12
