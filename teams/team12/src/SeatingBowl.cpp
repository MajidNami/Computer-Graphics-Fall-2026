#include "Seating.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <glm/gtc/constants.hpp>

namespace team12 {
namespace {

constexpr float seatHeight = 0.46f;
constexpr float fullTurn = glm::two_pi<float>();

void registerTread(citylab::Context& ctx, const StadiumLayout& layout,
                   const std::array<glm::vec2, 4>& points, float height) {
    glm::vec2 low = points[0], high = points[0];
    for (const auto point : points) {
        low = glm::min(low, point);
        high = glm::max(high, point);
    }
    // A wedge's axis-aligned SDK collider can reach farther infield than its mesh.
    // Cut those corners back to keep the pitch, runoff and goal reservations walkable.
    struct Bounds {
        glm::vec2 low;
        glm::vec2 high;
    };
    constexpr float walkerMargin = 0.35f;
    const auto halfPitch = layout.pitch.size * 0.5f;
    const std::array<glm::vec2, 2> clearHalfSizes{{
        halfPitch + glm::vec2{0.0f, layout.sidelineClearance} + glm::vec2{walkerMargin},
        {halfPitch.x + layout.goalClearance.x + walkerMargin,
         layout.goalClearance.y * 0.5f + walkerMargin},
    }};
    std::vector<Bounds> boxes{{low, high}};
    for (const auto halfSize : clearHalfSizes) {
        const auto clearLow = layout.pitch.center - halfSize;
        const auto clearHigh = layout.pitch.center + halfSize;
        std::vector<Bounds> remainder;
        const auto add = [&](glm::vec2 lo, glm::vec2 hi) {
            if (lo.x < hi.x && lo.y < hi.y) remainder.push_back({lo, hi});
        };
        for (const auto& box : boxes) {
            const auto cutLow = glm::max(box.low, clearLow);
            const auto cutHigh = glm::min(box.high, clearHigh);
            if (cutLow.x >= cutHigh.x || cutLow.y >= cutHigh.y) {
                remainder.push_back(box);
                continue;
            }
            add(box.low, {cutLow.x, box.high.y});
            add({cutHigh.x, box.low.y}, box.high);
            add({cutLow.x, box.low.y}, {cutHigh.x, cutLow.y});
            add({cutLow.x, cutHigh.y}, {cutHigh.x, box.high.y});
        }
        boxes.swap(remainder);
    }
    const auto origin = layout.stadiumCenter;
    for (const auto& box : boxes) {
        ctx.addCollider({origin.x + box.low.x, 0.0f, origin.z + box.low.y},
                        {origin.x + box.high.x, height, origin.z + box.high.y});
    }
}

void buildSeat(SeatingBowl& bowl) {
    auto& seat = bowl.seat;
    // A wider molded pan and a raked back with a flat shoulder line.
    addQuad(seat, {-0.29f, -0.035f, 0.25f}, {0.29f, -0.035f, 0.25f},
            {0.25f, 0.0f, -0.17f}, {-0.25f, 0.0f, -0.17f});
    addQuad(seat, {-0.25f, 0.0f, -0.17f}, {0.25f, 0.0f, -0.17f},
            {0.30f, 0.48f, -0.25f}, {-0.30f, 0.48f, -0.25f});
    addQuad(bowl.seatMount, {-0.085f, -seatHeight, -0.05f}, {0.085f, -seatHeight, -0.05f},
            {0.07f, -0.01f, 0.02f}, {-0.07f, -0.01f, 0.02f});
}

void buildRow(SeatingRow& row, const StadiumLayout& layout, const SeatingSection& section,
              int rowIndex, int sector, citylab::Context& ctx) {
    const auto& tier = layout.tiers[section.tier];
    const glm::vec2 radii = tier.firstRowRadii + glm::vec2{rowIndex * tier.rowDepth};
    const float height = tier.firstRowHeight + rowIndex * tier.risePerRow;
    const float tread = height - seatHeight;
    const float step = fullTurn / nominalSeatsPerRow;
    const auto center = ovalPoint(radii, (sector + 0.5f) * glm::half_pi<float>());
    const glm::vec3 origin{center.x, height, center.y};
    row.origin = origin - section.origin;
    for (const auto& seat : layout.seats) {
        if (seat.tier == section.tier && seat.sector == sector && seat.row == rowIndex) {
            auto local = seat;
            local.position -= origin;
            row.seats.push_back(local);
        }
    }
    const auto inner = radii - glm::vec2{tier.rowDepth * 0.5f};
    const auto outer = radii + glm::vec2{tier.rowDepth * 0.5f};
    const float tunnelZMin = layout.playerTunnel.center.y - layout.playerTunnel.size.y * 0.5f;
    const float tunnelZMax = layout.playerTunnel.center.y + layout.playerTunnel.size.y * 0.5f;
    constexpr float tunnelWallAllowance = 0.12f;
    const float tunnelLow = fullTurn + std::asin((tunnelZMin - tunnelWallAllowance) / inner.y);
    const float tunnelHigh = fullTurn + std::asin((tunnelZMax + tunnelWallAllowance) / outer.y);
    const auto aisle = layout.spectatorCorridors[sector];
    const float aisleHalfWidth = std::min(aisle.size.x, aisle.size.y) * 0.5f;
    const bool alongX = sector % 2 == 0;
    const float angleStart = sector * glm::half_pi<float>()
        + std::asin(aisleHalfWidth / (alongX ? inner.y : inner.x));
    const float angleEnd = (sector + 1) * glm::half_pi<float>()
        - std::asin(aisleHalfWidth / (alongX ? inner.x : inner.y));
    for (std::size_t first = 0; first < row.seats.size();) {
        std::size_t last = first;
        // A three-seat span keeps the chord deviation within the tread depth.
        while (last + 1 < row.seats.size() && last - first < 2 &&
               row.seats[last + 1].id == row.seats[last].id + 1) {
            ++last;
        }
        const float seatStart = (row.seats[first].id % nominalSeatsPerRow) * step;
        const float seatEnd = (row.seats[last].id % nominalSeatsPerRow) * step;
        float a = first == 0 ? angleStart : std::max(angleStart, seatStart - step * 0.5f);
        float b = last + 1 == row.seats.size() ? angleEnd : std::min(angleEnd, seatEnd + step * 0.5f);
        if (sector == seatingSectors - 1) {
            if (seatEnd < tunnelLow) b = std::min(b, tunnelLow);
            if (seatStart > tunnelHigh) a = std::max(a, tunnelHigh);
        }
        const std::array<glm::vec2, 4> outline{
            ovalPoint(outer, a), ovalPoint(inner, a), ovalPoint(inner, b), ovalPoint(outer, b)};
        registerTread(ctx, layout, outline, tread);
        const glm::vec2 rowOffset{origin.x, origin.z};
        addWedge(row.tread, {outline[0] - rowOffset, outline[1] - rowOffset,
                             outline[2] - rowOffset, outline[3] - rowOffset},
                 -height, tread - height);
        first = last + 1;
    }
}

// The stair takes one side of the 4 m aisle, leaving a continuous ground-level lane.
void buildStairs(SeatingBowl& bowl, const StadiumLayout& layout, citylab::Context& ctx) {
    constexpr float stairOuterEdge = 2.05f;
    constexpr float stairSide = 0.25f;
    constexpr float firstDistance = -1.25f;
    const auto inner = layout.innerSize * 0.5f;
    for (int aisle = 0; aisle < seatingSectors; ++aisle) {
        const float angle = aisle * glm::half_pi<float>();
        const glm::vec2 outward{std::round(std::cos(angle)), std::round(std::sin(angle))};
        const glm::vec2 across{-outward.y, outward.x};
        const auto foot = ovalPoint(inner, angle);
        const auto point = [&](float depth, float side) {
            return foot + outward * depth + across * side;
        };
        const auto railPoint = [&](float depth, float y, float side) {
            const auto p = point(depth, side);
            return glm::vec3{p.x, y, p.y};
        };
        float start = firstDistance;
        float base = 0.0f;
        for (const auto& tier : layout.tiers) {
            for (int row = 0; row < tier.rows; ++row) {
                const float landing = tier.firstRowRadii.x - inner.x + (row + 0.5f) * tier.rowDepth;
                const float height = tier.firstRowHeight + row * tier.risePerRow - seatHeight;
                constexpr float maxRiser = 0.30f;
                const int count = static_cast<int>(std::ceil((height - base) / maxRiser));
                const float run = (landing - start) / count;
                const float rise = (height - base) / count;
                for (int step = 0; step < count; ++step) {
                    const float a = start + run * step, b = a + run;
                    const float top = base + rise * (step + 1);
                    const std::array<glm::vec2, 4> outline{
                        point(a, stairSide), point(a, stairOuterEdge),
                        point(b, stairOuterEdge), point(b, stairSide)};
                    addWedge(bowl.stairs, outline, 0.0f, top);
                    registerTread(ctx, layout, outline, top);
                }
                addQuad(bowl.rails, railPoint(start, base + 0.87f, stairSide),
                        railPoint(landing, height + 0.87f, stairSide),
                        railPoint(landing, height + 0.94f, stairSide),
                        railPoint(start, base + 0.94f, stairSide));
                addQuad(bowl.rails, railPoint(landing - 0.06f, height, stairSide),
                        railPoint(landing, height, stairSide),
                        railPoint(landing, height + 0.9f, stairSide),
                        railPoint(landing - 0.06f, height + 0.9f, stairSide));
                start = landing;
                base = height;
            }
        }
    }
}

}  // namespace

SeatingBowl makeSeatingBowl(const StadiumLayout& layout, citylab::Context& ctx) {
    SeatingBowl bowl;
    buildSeat(bowl);
    for (int tier = 0; tier < static_cast<int>(layout.tiers.size()); ++tier) {
        for (int sector = 0; sector < seatingSectors; ++sector) {
            const auto center = ovalPoint(layout.tiers[tier].firstRowRadii,
                                          (sector + 0.5f) * glm::half_pi<float>());
            SeatingSection section{{center.x, 0.0f, center.y}, tier, {}};
            for (int row = 0; row < layout.tiers[tier].rows; ++row) {
                buildRow(section.rows.emplace_back(), layout, section, row, sector, ctx);
            }
            bowl.sections.push_back(std::move(section));
        }
    }
    buildStairs(bowl, layout, ctx);
    return bowl;
}

void drawSeatingBowl(const SeatingBowl& bowl) {
    drawMesh(bowl.stairs, palette::concrete);
    drawMesh(bowl.rails, palette::steel);
    for (const auto& section : bowl.sections) {
        const ScopedMatrix sectionFrame;
        glTranslatef(section.origin.x, section.origin.y, section.origin.z);
        const auto treadColor = section.tier == 0 ? glm::vec3{0.38f, 0.38f, 0.40f} : glm::vec3{0.44f, 0.42f, 0.44f};
        for (const auto& row : section.rows) {
            const ScopedMatrix rowFrame;
            glTranslatef(row.origin.x, row.origin.y, row.origin.z);
            drawMesh(row.tread, treadColor);
            for (const auto& seat : row.seats) {
                const ScopedMatrix seatFrame;
                glTranslatef(seat.position.x, seat.position.y, seat.position.z);
                glRotatef(seat.yawDegrees, 0.0f, 1.0f, 0.0f);
                const int block = (seat.id % nominalSeatsPerRow) / 6;
                const bool whiteAccent = section.tier == 1 && block % 4 == 1;
                drawMesh(bowl.seatMount, palette::steel);
                drawMesh(bowl.seat, whiteAccent ? palette::facadeWhite : glm::vec3{0.69f, 0.045f, 0.12f});
            }
        }
    }
}

}  // namespace team12
