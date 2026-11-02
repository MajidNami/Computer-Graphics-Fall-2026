#include "city/CityScene.h"

#include <algorithm>
#include <cmath>

#include "citylab/Shapes.h"
#include "city/Density.h"
#include "city/Geometry.h"
#include "city/Procedural.h"
#include "engine/Collision.h"
#include "engine/Log.h"
#include "engine/Texture.h"

namespace citylab {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kRoadY = -0.15f;     // asphalt; sidewalks, squares and plots are at y = 0
constexpr float kBaseY = -0.05f;     // lawn outside the city and under the plots
constexpr float kKerb = 0.3f;        // the kerb stone along every sidewalk
constexpr float kTramGauge = 1.435f;
constexpr int kLakeSegments = 96;
constexpr float kExitLength = Layout::kExitLength;

GLuint beginList() {
    const GLuint list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    return list;
}

constexpr float kPiece = 160.0f;  // the longest stretch of ground that is one chunk

// Flat surfaces that lie on another surface (the string lines of a free plot)
// are pulled towards the viewer so they never flicker against it. The empty
// plots are lists of their own, which are not measured, so it is safe here.
class Decal {
public:
    explicit Decal(float strength = 1.0f) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-strength, -2.0f * strength);
    }
    ~Decal() { glDisable(GL_POLYGON_OFFSET_FILL); }
};

// `r` cut into pieces of at most `length` metres along its long side.
std::vector<Rect> pieces(const Rect& r, float length) {
    const bool alongX = r.width() >= r.depth();
    const float a0 = alongX ? r.x0 : r.z0, a1 = alongX ? r.x1 : r.z1;
    const int n = std::max(1, static_cast<int>(std::ceil((a1 - a0) / length)));
    std::vector<Rect> out;
    for (int i = 0; i < n; ++i) {
        const float p0 = a0 + (a1 - a0) * static_cast<float>(i) / static_cast<float>(n);
        const float p1 = i + 1 == n ? a1 : a0 + (a1 - a0) * static_cast<float>(i + 1) / static_cast<float>(n);
        out.push_back(alongX ? Rect{p0, p1, r.z0, r.z1} : Rect{r.x0, r.x1, p0, p1});
    }
    return out;
}

// Splits [a0, a1] at the given intervals, keeping the parts between them.
std::vector<std::pair<float, float>> cut(float a0, float a1, std::vector<std::pair<float, float>> holes) {
    std::sort(holes.begin(), holes.end());
    std::vector<std::pair<float, float>> parts;
    float cursor = a0;
    for (const auto& [h0, h1] : holes) {
        if (h1 <= cursor || h0 >= a1) continue;
        if (h0 > cursor + 0.01f) parts.emplace_back(cursor, h0);
        cursor = std::max(cursor, h1);
    }
    if (a1 > cursor + 0.01f) parts.emplace_back(cursor, a1);
    return parts;
}

// `outer` minus `inner` as up to four rectangles.
std::vector<Rect> ring(const Rect& outer, const Rect& inner) {
    return {{outer.x0, outer.x1, outer.z0, inner.z0}, {outer.x0, outer.x1, inner.z1, outer.z1},
            {outer.x0, inner.x0, inner.z0, inner.z1}, {inner.x1, outer.x1, inner.z0, inner.z1}};
}

// `r` minus every rectangle in `holes`, as a list of rectangles.
std::vector<Rect> subtract(const Rect& r, const std::vector<Rect>& holes) {
    std::vector<Rect> pieces{r};
    for (const Rect& h : holes) {
        std::vector<Rect> next;
        for (const Rect& p : pieces) {
            if (!p.overlaps(h)) {
                next.push_back(p);
                continue;
            }
            const Rect inner{std::max(p.x0, h.x0), std::min(p.x1, h.x1), std::max(p.z0, h.z0), std::min(p.z1, h.z1)};
            for (const Rect& q : ring(p, inner))
                if (q.width() > 0.01f && q.depth() > 0.01f) next.push_back(q);
        }
        pieces = std::move(next);
    }
    return pieces;
}

// Vertical faces around a rectangle from y0 to y1, facing out (or in),
// left open where a side runs through one of the `gaps`.
void kerbFaces(const Rect& r, float y0, float y1, bool inward, const std::vector<Rect>& gaps = {}) {
    const float s = inward ? -1.0f : 1.0f;
    glBegin(GL_QUADS);
    auto face = [&](float ax, float az, float bx, float bz, float nx, float nz) {
        glNormal3f(nx * s, 0, nz * s);
        if (inward) {
            std::swap(ax, bx);
            std::swap(az, bz);
        }
        const float la = ax + az, lb = bx + bz;
        glTexCoord2f(la * 0.5f, y0); glVertex3f(ax, y0, az);
        glTexCoord2f(lb * 0.5f, y0); glVertex3f(bx, y0, bz);
        glTexCoord2f(lb * 0.5f, y1); glVertex3f(bx, y1, bz);
        glTexCoord2f(la * 0.5f, y1); glVertex3f(ax, y1, az);
    };
    // One side, from (ax, az) to (bx, bz) along x or z, cut at the gaps.
    auto side = [&](float ax, float az, float bx, float bz, float nx, float nz) {
        const bool alongX = std::fabs(bx - ax) > std::fabs(bz - az);
        const float a0 = alongX ? std::min(ax, bx) : std::min(az, bz), a1 = alongX ? std::max(ax, bx) : std::max(az, bz);
        std::vector<std::pair<float, float>> holes;
        for (const Rect& g : gaps)
            if (alongX ? g.contains(g.center().x, az, 0.01f) && az >= g.z0 - 0.01f && az <= g.z1 + 0.01f
                       : ax >= g.x0 - 0.01f && ax <= g.x1 + 0.01f)
                holes.push_back(alongX ? std::make_pair(g.x0, g.x1) : std::make_pair(g.z0, g.z1));
        const bool forward = alongX ? bx > ax : bz > az;
        for (const auto& [p0, p1] : cut(a0, a1, holes)) {
            const float from = forward ? p0 : p1, to = forward ? p1 : p0;
            if (alongX) face(from, az, to, bz, nx, nz);
            else face(ax, from, bx, to, nx, nz);
        }
    };
    side(r.x0, r.z1, r.x1, r.z1, 0, 1);   // south side, facing +z
    side(r.x1, r.z0, r.x0, r.z0, 0, -1);  // north side
    side(r.x1, r.z1, r.x1, r.z0, 1, 0);   // east side
    side(r.x0, r.z0, r.x0, r.z1, -1, 0);  // west side
    glEnd();
}

}  // namespace

void CityScene::ground(const Rect& r, float y, float uv) {
    // Long strips are cut so that a road leaving the city can be skipped
    // piece by piece; short ones share the chunk they are in.
    const float length = std::max(r.width(), r.depth());
    for (const Rect& piece : pieces(r, kPiece)) {
        geom::ground(piece, y, uv);
        if (length > 24.0f) chunks_.next();
    }
}

// --------------------------------------------------------------------------
// Build
// --------------------------------------------------------------------------

void CityScene::build(const Layout& layout, const Registry& registry, const std::filesystem::path& root,
                      CollisionWorld& collisions) {
    layout_ = &layout;
    collisions_ = &collisions;
    root_ = root;
    materials_.build();
    props_.build(materials_);

    // Textures must exist before display lists that bind them are compiled.
    for (const auto& d : layout.districts) loadSign("district-" + d.id);
    for (const auto& p : layout.plots) loadSign("plot-" + p.id);

    planStreets();
    // Areas where nothing may be planted at random.
    for (const auto& p : layout.plots) occupied_.push_back(p.bounds().grown(2.0f));
    for (const Rect& r : carriageways_) occupied_.push_back(r.grown(1.0f));
    for (const Rect& r : junctions_) occupied_.push_back(r.grown(1.0f));
    for (const Rect& r : exits_) occupied_.push_back(r.grown(1.0f));

    glPushAttrib(GL_ENABLE_BIT);
    glEnable(GL_TEXTURE_2D);

    // Everything lit and static. A seam (chunks_.next()) is where one piece
    // ends; every piece is measured, so the frame can leave it out.
    chunks_.begin(Layer::Base);
    buildGround();
    buildStreets();
    buildSidewalks();
    buildPark();
    buildTower();
    buildPlotFrames(registry);
    buildSigns();
    chunks_.begin(Layer::Lights, true);
    buildTowerLights();
    buildLampsAndBenches();
    buildScatter();
    chunks_.end();

    buildEmptyPlots();
    glPopAttrib();

    const Rect walkable = layout.city.grown(140.0f);
    collisions.setBounds(walkable.x0, walkable.x1, walkable.z0, walkable.z1);
    log::info("city", std::to_string(layout.plots.size()) + " plots, " + std::to_string(lamps_.size()) +
                          " lamps, " + std::to_string(benches_.size() + flowerBeds_.size()) + " benches and flower beds, " +
                          std::to_string(collisions.size()) + " colliders, " + std::to_string(chunks_.count()) +
                          " chunks");
}

GLuint CityScene::loadSign(const std::string& name) {
    const auto found = signs_.find(name);
    if (found != signs_.end()) return found->second;
    const auto file = root_ / "city" / "signs" / (name + ".png");
    GLuint texture = 0;
    if (std::filesystem::exists(file)) {
        TextureOptions options;
        options.repeat = false;
        std::string error;
        texture = loadTexture(file, options, &error);
        if (!texture) log::warn("city", error);
    }
    signs_[name] = texture;
    return texture;
}

void CityScene::solid(const glm::vec3& min, const glm::vec3& max) { collisions_->add({min, max}); }

bool CityScene::occupied(float x, float z, float margin) const {
    for (const Rect& r : occupied_)
        if (r.contains(x, z, margin)) return true;
    return false;
}

bool CityScene::onPath(float x, float z, float margin) const {
    const Layout& L = *layout_;
    for (const Rect& r : parkPaths_)
        if (r.contains(x, z, margin)) return true;
    if (glm::length(glm::vec2(x, z) - L.tower) < 30.0f + margin) return true;
    if (glm::length(glm::vec2(x, z) - L.fountain) < L.fountainRadius + 12.0f + margin) return true;
    return L.inLake(x, z, 6.0f + margin);
}

void CityScene::streetLamp(float x, float z, float yawDegrees) {
    const float a = glm::radians(yawDegrees);
    const glm::vec2 head = glm::vec2(x, z) + glm::vec2(std::sin(a), std::cos(a)) * Props::kStreetLampReach;
    lamps_.push_back({{head.x, Props::kStreetLampHeight - 0.1f, head.y}, 10.0f});
    streetLampFrames_.push_back({x, z, yawDegrees});
    ++census_.streetLamps;
    solid({x - 0.2f, 0.0f, z - 0.2f}, {x + 0.2f, 6.0f, z + 0.2f});
    shadows_.push_back({x, z, 0.6f});
}

void CityScene::parkLamp(float x, float z) {
    lamps_.push_back({{x, Props::kParkLampHeight, z}, 5.0f});
    parkLamps_.push_back({x, z});
    ++census_.parkLamps;
    solid({x - 0.15f, 0.0f, z - 0.15f}, {x + 0.15f, 3.5f, z + 0.15f});
    occupied_.push_back({x - 1.5f, x + 1.5f, z - 1.5f, z + 1.5f});
}

void CityScene::benchAt(float x, float z, float yawDegrees) {
    benches_.push_back({x, z, yawDegrees});
    ++census_.benches;
    const bool alongX = std::fmod(std::fabs(yawDegrees), 180.0f) < 45.0f;
    const float hx = alongX ? 1.0f : 0.3f, hz = alongX ? 0.3f : 1.0f;
    solid({x - hx, 0.0f, z - hz}, {x + hx, 0.9f, z + hz});
    occupied_.push_back({x - 1.5f, x + 1.5f, z - 1.5f, z + 1.5f});
}

void CityScene::flowerBedAt(float x, float z, float yawDegrees) {
    flowerBeds_.push_back({x, z, yawDegrees});
    ++census_.flowerBeds;
    solid({x - 1.6f, 0.0f, z - 1.6f}, {x + 1.6f, 0.5f, z + 1.6f});
}

// --------------------------------------------------------------------------
// Streets
// --------------------------------------------------------------------------

void CityScene::planStreets() {
    const Layout& L = *layout_;
    const float sw = L.sidewalk;
    std::vector<Rect> alongX, alongZ;  // carriageways before cutting
    for (const StreetLayout& s : L.streets) {
        const Rect c{s.rect.x0 + sw, s.rect.x1 - sw, s.rect.z0 + sw, s.rect.z1 - sw};
        (s.alongX() ? alongX : alongZ).push_back(c);
    }
    junctions_ = L.junctions();
    for (const Rect& h : alongX) {
        std::vector<std::pair<float, float>> holes;
        for (const Rect& j : junctions_)
            if (j.overlaps(h)) holes.emplace_back(j.x0, j.x1);
        for (const auto& [a, b] : cut(h.x0, h.x1, holes)) carriageways_.push_back({a, b, h.z0, h.z1});
    }
    for (const Rect& v : alongZ) {
        std::vector<std::pair<float, float>> holes;
        for (const Rect& j : junctions_)
            if (j.overlaps(v)) holes.emplace_back(j.z0, j.z1);
        for (const auto& [a, b] : cut(v.z0, v.z1, holes)) carriageways_.push_back({v.x0, v.x1, a, b});
    }
    for (const auto& d : L.districts) islands_.push_back(d.rect.grown(sw));
    islands_.push_back(L.park.grown(sw));

    // The avenues go on out of the city towards the rest of town: a band of
    // two sidewalks and a carriageway from the city edge outwards. The
    // carriageway also crosses the sidewalk along the city edge.
    const Rect& c = L.city;
    for (const StreetLayout& s : L.streets) {
        const Rect& b = s.rect;
        std::vector<Rect> bands;
        if (s.alongX()) {
            if (b.x0 <= c.x0 + 0.1f) bands.push_back({c.x0 - kExitLength, c.x0, b.z0, b.z1});
            if (b.x1 >= c.x1 - 0.1f) bands.push_back({c.x1, c.x1 + kExitLength, b.z0, b.z1});
        } else {
            if (b.z0 <= c.z0 + 0.1f) bands.push_back({b.x0, b.x1, c.z0 - kExitLength, c.z0});
            if (b.z1 >= c.z1 - 0.1f) bands.push_back({b.x0, b.x1, c.z1, c.z1 + kExitLength});
        }
        for (const Rect& band : bands) {
            exits_.push_back(band);
            Rect road = s.alongX() ? Rect{band.x0, band.x1, band.z0 + sw, band.z1 - sw}
                                   : Rect{band.x0 + sw, band.x1 - sw, band.z0, band.z1};
            // Through the city's edge sidewalk, up to the avenue inside.
            if (s.alongX()) (band.x0 < c.x0 ? road.x1 : road.x0) += band.x0 < c.x0 ? sw : -sw;
            else (band.z0 < c.z0 ? road.z1 : road.z0) += band.z0 < c.z0 ? sw : -sw;
            exitRoads_.push_back(road);
            if (s.alongX()) {
                exitWalks_.push_back({band.x0, band.x1, band.z0, band.z0 + sw});
                exitWalks_.push_back({band.x0, band.x1, band.z1 - sw, band.z1});
            } else {
                exitWalks_.push_back({band.x0, band.x0 + sw, band.z0, band.z1});
                exitWalks_.push_back({band.x1 - sw, band.x1, band.z0, band.z1});
            }
        }
    }

    // Straight park paths: a loop just inside the park, the north-south axis
    // through the tower, the two paths that continue the inner avenues, and
    // the east-west path from the lake to the fountain and the east gate.
    const Rect& pk = L.park;
    const Rect loopOuter = pk.grown(-6.0f), loopInner = pk.grown(-12.0f);
    for (const Rect& r : ring(loopOuter, loopInner)) parkPaths_.push_back(r);
    parkPaths_.push_back({L.tower.x - 4.0f, L.tower.x + 4.0f, pk.z0, pk.z1});
    for (const StreetLayout& s : L.streets) {
        if (s.alongX() || s.rect.x0 < pk.x0 || s.rect.x1 > pk.x1) continue;
        const float x = s.rect.center().x;
        if (std::none_of(parkPaths_.begin(), parkPaths_.end(), [&](const Rect& r) { return r.contains(x, pk.z0 + 20); }))
            parkPaths_.push_back({x - 3.0f, x + 3.0f, pk.z0, pk.z1});
    }
    parkPaths_.push_back({L.lake.x + L.lakeRadius.x + 2.0f, pk.x1, L.tower.y - 3.0f, L.tower.y + 3.0f});
}

void CityScene::buildGround() {
    // Meadow outside the city, cut into 50 m tiles so fog and lighting are
    // evaluated often enough on every driver; fine gravel under the plots.
    const Layout& L = *layout_;
    std::vector<Rect> holes{L.city};
    holes.insert(holes.end(), exits_.begin(), exits_.end());
    bind(materials_.meadow);
    white();
    // One chunk for every 200 m square of meadow: far enough that there are
    // not thousands, near enough that the ones behind you can be skipped.
    const float extent = 2000.0f, tile = 50.0f, chunk = 200.0f;
    for (float cx = -extent; cx < extent; cx += chunk) {
        for (float cz = -extent; cz < extent; cz += chunk) {
            for (float x = cx; x < cx + chunk; x += tile)
                for (float z = cz; z < cz + chunk; z += tile)
                    for (const Rect& piece : subtract({x, x + tile, z, z + tile}, holes))
                        geom::ground(piece, kBaseY, 0.05f, Materials::meadowColour);
            chunks_.next();
        }
    }
    white();
    // Not under the plots themselves: a zone may dig below the ground (a
    // pool, a basin), and an empty plot draws its own lawn or building site.
    std::vector<Rect> plots;
    for (const auto& p : L.plots) plots.push_back(p.bounds());
    bind(materials_.gravel);
    for (const auto& d : L.districts)
        for (const Rect& r : subtract(d.rect, plots)) ground(r, kBaseY, 0.25f);
    // The bottom of each plot's basement, seen only where a running zone
    // leaves its ground open.
    color(0.55f, 0.50f, 0.44f);
    for (const Rect& r : plots) {
        geom::ground(r, -L.basement, 0.25f);
        chunks_.next();
    }
}

void CityScene::buildStreets() {
    const Layout& L = *layout_;
    const auto isTram = [&](const Rect& r) { return r.contains(r.center().x, L.tramZ) && r.width() > r.depth(); };

    bind(materials_.asphalt);
    white();
    for (const Rect& r : carriageways_) ground(r, kRoadY, 0.25f);
    for (const Rect& r : junctions_) ground(r, kRoadY, 0.25f);
    for (const Rect& r : exitRoads_) ground(r, kRoadY, 0.25f);

    // Crossings: at both ends of every stretch that meets a junction, and
    // where a park path meets the avenue in mid-block, and at the city entrance.
    struct Zebra {
        Rect r;
        bool alongX;  // the road runs along x (the stripes too)
        bool tram;    // the road has the tram track in its median
    };
    std::vector<Zebra> zebras;
    auto touchesJunction = [&](float x, float z) {
        return std::any_of(junctions_.begin(), junctions_.end(), [&](const Rect& j) { return j.contains(x, z, 0.5f); });
    };
    for (const Rect& r : carriageways_) {
        const bool ax = r.width() > r.depth();
        if (ax) {
            const bool tram = isTram(r);
            if (touchesJunction(r.x0, r.center().y)) zebras.push_back({{r.x0 + 1.0f, r.x0 + 5.0f, r.z0, r.z1}, true, tram});
            if (touchesJunction(r.x1, r.center().y)) zebras.push_back({{r.x1 - 5.0f, r.x1 - 1.0f, r.z0, r.z1}, true, tram});
        } else {
            if (touchesJunction(r.center().x, r.z0)) zebras.push_back({{r.x0, r.x1, r.z0 + 1.0f, r.z0 + 5.0f}, false, false});
            if (touchesJunction(r.center().x, r.z1)) zebras.push_back({{r.x0, r.x1, r.z1 - 5.0f, r.z1 - 1.0f}, false, false});
        }
    }
    // Where an avenue leaves the city, its crossing is the edge sidewalk.
    for (const Rect& r : exitRoads_) {
        const bool ax = r.width() > r.depth();
        const Rect& c = L.city;
        const float sw = L.sidewalk;
        Rect z = r;
        if (ax) z = r.x0 < c.x0 ? Rect{c.x0, c.x0 + sw, r.z0, r.z1} : Rect{c.x1 - sw, c.x1, r.z0, r.z1};
        else z = r.z0 < c.z0 ? Rect{r.x0, r.x1, c.z0, c.z0 + sw} : Rect{r.x0, r.x1, c.z1 - sw, c.z1};
        zebras.push_back({z.grown(-0.5f), ax, ax && r.contains(r.center().x, L.tramZ)});
    }
    for (const Rect& r : carriageways_) {
        if (r.width() <= r.depth()) continue;
        // The tower axis crosses the park avenue and the tram boulevard; the
        // north avenue is crossed at the city entrance.
        const float x = L.tower.x;
        const bool parkEdge = std::fabs(r.z1 - L.park.z0) < L.sidewalk + 0.5f ||
                              std::fabs(r.z0 - L.park.z1) < L.sidewalk + 0.5f;
        const bool entrance = std::fabs(r.center().y - L.spawnPosition.z) < 1.0f;
        if ((parkEdge || entrance) && x > r.x0 + 8.0f && x < r.x1 - 8.0f)
            zebras.push_back({{x - 2.0f, x + 2.0f, r.z0, r.z1}, true, isTram(r)});
    }

    // Markings, in white and in the green of the cycle lanes. The cycle lanes
    // are wide and always drawn (Paint). The white lines are thinner than a pixel
    // from afar, where they would only crawl: they are detail, which the frame
    // draws only near the viewer.
    bind(materials_.plain);
    auto line = [&](bool ax, float a0, float a1, float across, float width, float dash, float gap) {
        for (float a = a0; a < a1; a += dash + gap) {
            const float b = std::min(a1, a + dash);
            if (ax) geom::ground({a, b, across - width * 0.5f, across + width * 0.5f}, kRoadY, 0.25f);
            else geom::ground({across - width * 0.5f, across + width * 0.5f, a, b}, kRoadY, 0.25f);
        }
    };
    std::vector<Rect> marked = carriageways_;
    marked.insert(marked.end(), exitRoads_.begin(), exitRoads_.end());
    // Every stretch of road that is marked, in pieces of 162 m = 18 dash periods, so
    // the dashes of every piece line up with the next one's.
    struct Stretch {
        bool ax, tram;
        float p0, p1, c0, c1;
    };
    std::vector<Stretch> stretches;
    for (const Rect& r : marked) {
        const bool ax = r.width() > r.depth();
        const float a0 = (ax ? r.x0 : r.z0), a1 = (ax ? r.x1 : r.z1);
        const float c0 = (ax ? r.z0 : r.x0), c1 = (ax ? r.z1 : r.x1);
        // Leave the zebra crossings free of other markings.
        std::vector<std::pair<float, float>> holes;
        for (const Zebra& z : zebras)
            if (z.r.overlaps(r.grown(0.1f))) holes.push_back(ax ? std::make_pair(z.r.x0 - 1.0f, z.r.x1 + 1.0f)
                                                                : std::make_pair(z.r.z0 - 1.0f, z.r.z1 + 1.0f));
        for (const auto& [s0, s1] : cut(a0, a1, holes)) {
            if (s1 - s0 < 2.0f) continue;
            for (float p0 = s0; p0 < s1; p0 += 162.0f) stretches.push_back({ax, isTram(r), p0, std::min(s1, p0 + 162.0f), c0, c1});
        }
    }
    chunks_.begin(Layer::Paint);
    bind(materials_.plain);
    color(0.26f, 0.50f, 0.34f);
    for (const Stretch& m : stretches) {
        line(m.ax, m.p0, m.p1, m.c0 + 1.0f, 2.0f, 1e4f, 0.0f);  // cycle lanes along both kerbs
        line(m.ax, m.p0, m.p1, m.c1 - 1.0f, 2.0f, 1e4f, 0.0f);
        chunks_.next();
    }
    chunks_.begin(Layer::Detail);
    bind(materials_.plain);
    color(0.93f, 0.93f, 0.90f);
    for (const Stretch& m : stretches) {
        const float mid = (m.c0 + m.c1) * 0.5f;
        line(m.ax, m.p0, m.p1, m.c0 + 2.1f, 0.15f, 1e4f, 0.0f);
        line(m.ax, m.p0, m.p1, m.c1 - 2.1f, 0.15f, 1e4f, 0.0f);
        if (!m.tram) {  // the tram track is the median
            line(m.ax, m.p0, m.p1, mid - 0.18f, 0.12f, 1e4f, 0.0f);
            line(m.ax, m.p0, m.p1, mid + 0.18f, 0.12f, 1e4f, 0.0f);
            line(m.ax, m.p0 + 1.0f, m.p1, mid - 4.0f, 0.12f, 3.0f, 6.0f);
            line(m.ax, m.p0 + 1.0f, m.p1, mid + 4.0f, 0.12f, 3.0f, 6.0f);
        }
        chunks_.next();
    }
    color(0.95f, 0.95f, 0.93f);
    for (const Zebra& z : zebras) {
        const float c0 = z.alongX ? z.r.z0 : z.r.x0, c1 = z.alongX ? z.r.z1 : z.r.x1;
        for (float c = c0 + 0.6f; c + 0.5f < c1 - 0.3f; c += 1.0f) {
            if (z.tram && std::fabs(c + 0.25f - L.tramZ) < L.tramWidth * 0.5f + 0.3f) continue;
            if (z.alongX) geom::ground({z.r.x0, z.r.x1, c, c + 0.5f}, kRoadY, 0.25f);
            else geom::ground({c, c + 0.5f, z.r.z0, z.r.z1}, kRoadY, 0.25f);
        }
        chunks_.next();
    }

    // The tram track: a grass bed in the median of the tram boulevard
    // (paved where it crosses the avenues), with two tracks on it.
    chunks_.begin(Layer::Base);
    const float half = L.tramWidth * 0.5f, bed = kBaseY;
    for (const Rect& r : marked) {
        if (!isTram(r)) continue;
        bind(materials_.grass);
        color(0.85f, 0.95f, 0.80f);
        for (const Rect& piece : pieces(r, kPiece)) {
            geom::block({piece.x0, kRoadY, L.tramZ - half}, {piece.x1, bed, L.tramZ + half}, 0.25f);
            chunks_.next();
        }
    }
    for (const Rect& j : junctions_) {
        if (!j.contains(j.center().x, L.tramZ)) continue;
        bind(materials_.concrete);
        color(0.80f, 0.80f, 0.78f);
        geom::block({j.x0, kRoadY, L.tramZ - half}, {j.x1, bed, L.tramZ + half}, 0.25f);
        chunks_.next();
    }
    bind(materials_.plain);
    color(0.55f, 0.55f, 0.58f);
    for (const float track : {L.tramZ - half * 0.5f, L.tramZ + half * 0.5f}) {
        for (const float rail : {track - kTramGauge * 0.5f, track + kTramGauge * 0.5f}) {
            int laid = 0;
            for (float x = L.city.x0 - kExitLength; x < L.city.x1 + kExitLength; x += 50.0f) {
                geom::block({x, bed, rail - 0.04f}, {x + 50.0f, bed + 0.05f, rail + 0.04f}, 1.0f);
                if (++laid % 3 == 0) chunks_.next();  // every 150 m
            }
            chunks_.next();
        }
    }
    // Overhead line: poles between the tracks every 36 m, wires drawn as lines.
    color(0.30f, 0.32f, 0.35f);
    int raised = 0;
    for (float x = L.city.x0 - kExitLength + 10.0f; x < L.city.x1 + kExitLength; x += 36.0f) {
        if (std::any_of(junctions_.begin(), junctions_.end(), [&](const Rect& j) { return j.contains(x, L.tramZ, 2.0f); }))
            continue;
        glPushMatrix();
        glTranslatef(x, bed, L.tramZ);
        shapes::cylinder(0.12f, 6.6f, 8, true);
        glTranslatef(0.0f, 6.3f, 0.0f);
        shapes::box(0.12f, 0.12f, L.tramWidth - 0.6f);
        glPopMatrix();
        solid({x - 0.2f, kRoadY, L.tramZ - 0.2f}, {x + 0.2f, 6.0f, L.tramZ + 0.2f});
        if (++raised % 5 == 0) chunks_.next();
    }
    chunks_.next();
    // The wires are lines a pixel wide at most: detail, in pieces of 150 m.
    chunks_.begin(Layer::Detail);
    for (const float track : {L.tramZ - half * 0.5f, L.tramZ + half * 0.5f}) {
        for (float x = L.city.x0 - kExitLength; x < L.city.x1 + kExitLength; x += 150.0f) {
            glDisable(GL_TEXTURE_2D);
            color(0.15f, 0.15f, 0.17f);
            glBegin(GL_LINES);
            glVertex3f(x, 6.2f, track);
            glVertex3f(std::min(x + 150.0f, L.city.x1 + kExitLength), 6.2f, track);
            glEnd();
            glEnable(GL_TEXTURE_2D);
            chunks_.next();
        }
    }
    chunks_.begin(Layer::Base);
}

void CityScene::buildSidewalks() {
    const Layout& L = *layout_;
    const float sw = L.sidewalk;
    auto pavement = [&](const Rect& outer, const Rect& inner, bool kerbOutside) {
        // Pavers, with the kerb stone along the road side of the band.
        const Rect kerbOuter = kerbOutside ? outer : inner.grown(kKerb);
        const Rect kerbInner = kerbOutside ? outer.grown(-kKerb) : inner;
        const Rect paveOuter = kerbOutside ? kerbInner : outer, paveInner = kerbOutside ? inner : kerbOuter;
        bind(materials_.pavers);
        white();
        for (const Rect& r : ring(paveOuter, paveInner)) ground(r, 0.0f, 0.25f);
        bind(materials_.concrete);
        color(0.95f, 0.95f, 0.93f);
        for (const Rect& r : ring(kerbOuter, kerbInner)) ground(r, 0.0f, 0.5f);
    };
    for (const Rect& island : islands_) {
        pavement(island, island.grown(-sw), true);
        bind(materials_.concrete);
        color(0.82f, 0.82f, 0.80f);
        kerbFaces(island, kRoadY, 0.0f, false);
        chunks_.next();
    }
    // The sidewalk along the edge of the city, with a low step to the
    // meadow, open where the avenues leave the city.
    const Rect inner = L.city.grown(-sw);
    bind(materials_.pavers);
    white();
    for (const Rect& band : ring(L.city, inner.grown(kKerb)))
        for (const Rect& r : subtract(band, exitRoads_)) ground(r, 0.0f, 0.25f);
    bind(materials_.concrete);
    color(0.95f, 0.95f, 0.93f);
    for (const Rect& band : ring(inner.grown(kKerb), inner))
        for (const Rect& r : subtract(band, exitRoads_)) ground(r, 0.0f, 0.5f);
    color(0.82f, 0.82f, 0.80f);
    kerbFaces(inner, kRoadY, 0.0f, true, exitRoads_);
    chunks_.next();
    kerbFaces(L.city, kBaseY, 0.0f, false, exits_);
    chunks_.next();

    // Sidewalks of the avenues out of the city: a kerb towards the road and
    // a low step down to the meadow.
    for (const Rect& w : exitWalks_) {
        bind(materials_.pavers);
        white();
        ground(w, 0.0f, 0.25f);
        bind(materials_.concrete);
        color(0.82f, 0.82f, 0.80f);
        const bool ax = w.width() > w.depth();
        for (const Rect& band : exits_) {
            if (!band.contains(w.center().x, w.center().y)) continue;
            const bool first = ax ? std::fabs(w.z0 - band.z0) < 0.1f : std::fabs(w.x0 - band.x0) < 0.1f;
            const float road = ax ? (first ? w.z1 : w.z0) : (first ? w.x1 : w.x0);
            const float meadow = ax ? (first ? w.z0 : w.z1) : (first ? w.x0 : w.x1);
            // A rectangle of no width has faces on both sides of its line.
            kerbFaces(ax ? Rect{w.x0, w.x1, road, road} : Rect{road, road, w.z0, w.z1}, kRoadY, 0.0f, false);
            chunks_.next();
            kerbFaces(ax ? Rect{w.x0, w.x1, meadow, meadow} : Rect{meadow, meadow, w.z0, w.z1}, kBaseY, 0.0f, false);
            chunks_.next();
        }
    }
}

// --------------------------------------------------------------------------
// The park
// --------------------------------------------------------------------------

void CityScene::buildPark() {
    const Layout& L = *layout_;
    const Rect& pk = L.park;
    const glm::vec2 c = L.lake, r = L.lakeRadius;
    const float bank = 1.2f, promenade = 4.5f;
    const glm::vec2 rBank = r + glm::vec2(bank), rWalk = rBank + glm::vec2(promenade);

    // Lawn: the park without the lake. Around the lake the lawn is a band
    // from the bank's ellipse out to a box, sampled so the box corners are
    // exact.
    const Rect box{c.x - rWalk.x - 8.0f, c.x + rWalk.x + 8.0f, c.y - rWalk.y - 8.0f, c.y + rWalk.y + 8.0f};
    bind(materials_.grass);
    white();
    for (const Rect& piece : ring(pk, box)) ground(piece, 0.0f, 0.25f);
    std::vector<float> angles;
    for (int i = 0; i < kLakeSegments; ++i) angles.push_back(2.0f * kPi * static_cast<float>(i) / kLakeSegments);
    const float hx = box.width() * 0.5f, hz = box.depth() * 0.5f;
    const float corner = std::atan2(hz / rBank.y, hx / rBank.x);
    for (const float a : {corner, kPi - corner, kPi + corner, 2.0f * kPi - corner}) angles.push_back(a);
    std::sort(angles.begin(), angles.end());
    angles.push_back(2.0f * kPi);
    glBegin(GL_QUAD_STRIP);
    glNormal3f(0, 1, 0);
    for (const float a : angles) {
        // Going clockwise seen from above (-a), the inner point first keeps the band facing up.
        const glm::vec2 d{std::cos(-a), std::sin(-a)};
        const glm::vec2 in = c + d * rBank;
        const float t = std::min(std::fabs(d.x) > 1e-4f ? hx / std::fabs(d.x * rBank.x) : 1e9f,
                                 std::fabs(d.y) > 1e-4f ? hz / std::fabs(d.y * rBank.y) : 1e9f);
        const glm::vec2 out = c + d * rBank * t;
        glTexCoord2f(in.x * 0.25f, -in.y * 0.25f);
        glVertex3f(in.x, 0.0f, in.y);
        glTexCoord2f(out.x * 0.25f, -out.y * 0.25f);
        glVertex3f(out.x, 0.0f, out.y);
    }
    glEnd();
    chunks_.next();

    // The lake: a bed, a stone edge a little above the lawn, a promenade.
    bind(materials_.soil);
    color(0.35f, 0.55f, 0.55f);
    geom::ellipse(c, r, -1.4f, kLakeSegments, 0.25f);
    bind(materials_.concrete);
    color(0.92f, 0.92f, 0.90f);
    geom::ellipseWall(c, r, -1.4f, 0.12f, kLakeSegments, 0.5f, true);
    geom::ellipseRing(c, r, rBank, 0.12f, kLakeSegments, 0.5f);
    geom::ellipseWall(c, rBank, 0.0f, 0.12f, kLakeSegments, 0.5f, false);
    chunks_.begin(Layer::Paint);
    bind(materials_.parkPath);
    white();
    geom::ellipseRing(c, rBank, rWalk, 0.0f, kLakeSegments, 0.25f);
    chunks_.next();
    for (const Rect& p : parkPaths_) ground(p, 0.0f, 0.25f);
    chunks_.begin(Layer::Base);
    // Walkers stay on the bank: the water is a row of boxes along z.
    for (float z = c.y - r.y; z < c.y + r.y; z += 2.0f) {
        // The narrowest point of the strip, so the box never covers the bank.
        const float zm = std::fabs(z - c.y) > std::fabs(z + 2.0f - c.y) ? z : z + 2.0f;
        const float w = r.x * std::sqrt(std::max(0.0f, 1.0f - std::pow((zm - c.y) / r.y, 2.0f)));
        if (w > 0.5f) solid({c.x - w + 0.2f, -1.4f, z}, {c.x + w - 0.2f, 1.0f, z + 2.0f});
    }

    // Squares around the tower and the fountain.
    chunks_.begin(Layer::Plaza);
    bind(materials_.plaza);
    white();
    geom::ellipse(L.tower, glm::vec2(28.0f), 0.0f, 64, 0.25f);
    chunks_.next();
    geom::ellipse(L.fountain, glm::vec2(L.fountainRadius + 12.0f), 0.0f, 64, 0.25f);
    chunks_.begin(Layer::Base);
    // The fountain basin: a low concrete rim around a pale blue floor.
    const float fr = L.fountainRadius;
    bind(materials_.concrete);
    color(0.95f, 0.95f, 0.93f);
    geom::ellipseWall(L.fountain, glm::vec2(fr + 0.6f), 0.0f, 0.6f, 64, 0.5f, false);
    geom::ellipseRing(L.fountain, glm::vec2(fr), glm::vec2(fr + 0.6f), 0.6f, 64, 0.5f);
    geom::ellipseWall(L.fountain, glm::vec2(fr), 0.05f, 0.6f, 64, 0.5f, true);
    bind(materials_.plain);
    color(0.55f, 0.80f, 0.92f);
    geom::ellipse(L.fountain, glm::vec2(fr), 0.05f, 64, 0.25f);
    chunks_.next();
    const float s = fr * 0.7071f + 0.6f;  // three boxes cover the round basin
    solid({L.fountain.x - s, 0.0f, L.fountain.y - s}, {L.fountain.x + s, 0.8f, L.fountain.y + s});
    solid({L.fountain.x - fr - 0.6f, 0.0f, L.fountain.y - fr * 0.4f}, {L.fountain.x + fr + 0.6f, 0.8f, L.fountain.y + fr * 0.4f});
    solid({L.fountain.x - fr * 0.4f, 0.0f, L.fountain.y - fr - 0.6f}, {L.fountain.x + fr * 0.4f, 0.8f, L.fountain.y + fr + 0.6f});
    fountains_.push_back({{L.fountain.x, 0.3f, L.fountain.y}, 7.0f, 0.35f});
    for (int k = 0; k < 12; ++k) {
        const float a = 2.0f * kPi * static_cast<float>(k) / 12.0f;
        fountains_.push_back({{L.fountain.x + std::cos(a) * fr * 0.62f, 0.3f, L.fountain.y + std::sin(a) * fr * 0.62f},
                              2.4f, 0.25f});
    }
}

// --------------------------------------------------------------------------
// Lamps, benches and flower beds
// --------------------------------------------------------------------------

void CityScene::buildLampsAndBenches() {
    const Layout& L = *layout_;

    // Plot gates and mid-block crossings stay clear of lamps.
    std::vector<glm::vec2> gates;
    for (const auto& p : L.plots) gates.push_back(p.gate());
    for (const float z : {L.park.z0, L.park.z1, L.city.z0 + L.sidewalk})
        gates.push_back({L.tower.x, z});
    auto nearGate = [&](const glm::vec2& q, float distance) {
        if (std::any_of(keepClear_.begin(), keepClear_.end(), [&](const Rect& r) { return r.contains(q.x, q.y); }))
            return true;
        return std::any_of(gates.begin(), gates.end(), [&](const glm::vec2& g) { return glm::length(g - q) < distance; });
    };
    // Lamps along every sidewalk, from `a` to `b` along the road edge; `inward`
    // points from it across the sidewalk.
    auto side = [&](glm::vec2 a, glm::vec2 b, glm::vec2 inward) {
        const float length = glm::length(b - a);
        const glm::vec2 d = (b - a) / length;
        const float yaw = glm::degrees(std::atan2(-inward.x, -inward.y));  // lamp arms over the road
        for (float t = 15.0f; t < length - 8.0f; t += density::kStreetLampSpacing) {
            const glm::vec2 p = a + d * t + inward * 0.55f;
            if (nearGate(p, 6.0f)) continue;
            streetLamp(p.x, p.y, yaw);
        }
    };
    auto around = [&](const Rect& r, bool facingOut) {
        // facingOut: the road is outside the rectangle (an island); else inside.
        const float s = facingOut ? 1.0f : -1.0f;
        side({r.x0, r.z0}, {r.x1, r.z0}, {0.0f, s});
        side({r.x0, r.z1}, {r.x1, r.z1}, {0.0f, -s});
        side({r.x0, r.z0}, {r.x0, r.z1}, {s, 0.0f});
        side({r.x1, r.z0}, {r.x1, r.z1}, {-s, 0.0f});
    };
    for (const Rect& island : islands_) around(island, true);
    around(L.city.grown(-L.sidewalk), false);
    // The first stretch of every avenue out of the city.
    for (const Rect& band : exits_) {
        const bool ax = band.width() > band.depth();
        const bool west = ax && band.x1 <= L.city.x0 + 0.1f, north = !ax && band.z1 <= L.city.z0 + 0.1f;
        const float sw = L.sidewalk, length = 420.0f;
        if (ax) {
            const float x0 = west ? band.x1 - length : band.x0, x1 = west ? band.x1 : band.x0 + length;
            side({x0, band.z0 + sw}, {x1, band.z0 + sw}, {0.0f, -1.0f});
            side({x0, band.z1 - sw}, {x1, band.z1 - sw}, {0.0f, 1.0f});
        } else {
            const float z0 = north ? band.z1 - length : band.z0, z1 = north ? band.z1 : band.z0 + length;
            side({band.x0 + sw, z0}, {band.x0 + sw, z1}, {-1.0f, 0.0f});
            side({band.x1 - sw, z0}, {band.x1 - sw, z1}, {1.0f, 0.0f});
        }
    }

    // Park: lamps and benches along the paths, flower beds around the tower
    // square.
    const Rect& pk = L.park;
    for (const Rect& p : parkPaths_) {
        const bool ax = p.width() > p.depth();
        const float a0 = ax ? p.x0 : p.z0, a1 = ax ? p.x1 : p.z1;
        const float edge = ax ? p.z1 + 0.8f : p.x1 + 0.8f, other = ax ? p.z0 - 1.2f : p.x0 - 1.2f;
        for (float t = a0 + 14.0f; t < a1 - 8.0f; t += 26.0f) {
            const glm::vec2 lamp = ax ? glm::vec2(t, edge) : glm::vec2(edge, t);
            if (!onPath(lamp.x, lamp.y, -1.0f) && pk.contains(lamp.x, lamp.y, -1.0f) &&
                !L.inLake(lamp.x, lamp.y, 2.0f) && !occupied(lamp.x, lamp.y, 0.0f))
                parkLamp(lamp.x, lamp.y);
            const glm::vec2 seat = ax ? glm::vec2(t + 13.0f, other) : glm::vec2(other, t + 13.0f);
            if (t + 13.0f < a1 - 6.0f && !onPath(seat.x, seat.y, -1.5f) && pk.contains(seat.x, seat.y, -2.0f) &&
                !L.inLake(seat.x, seat.y, 2.0f) && !occupied(seat.x, seat.y, 0.5f))
                benchAt(seat.x, seat.y, ax ? 180.0f : 90.0f);
        }
    }
    for (int k = 0; k < 16; ++k) {
        const float a = 2.0f * kPi * (static_cast<float>(k) + 0.5f) / 16.0f;
        const glm::vec2 p = L.tower + glm::vec2(std::cos(a), std::sin(a)) * 23.0f;
        if (onPath(p.x, p.y, 0.0f) && glm::length(p - L.tower) > 30.0f) continue;
        bool onAxis = false;
        for (const Rect& r : parkPaths_) onAxis = onAxis || r.contains(p.x, p.y, 2.5f);
        if (onAxis) continue;
        flowerBedAt(p.x, p.y, glm::degrees(-a) + 90.0f);
    }
}

// Files what stands in the city by the square of 160 m it stands in, and
// compiles each square's share into chunks: shadows, lamps, benches and
// flower beds.
void CityScene::buildScatter() {
    constexpr float kCell = 160.0f;
    const auto cellOf = [&](float x, float z) {
        return std::make_pair(static_cast<int>(std::floor(x / kCell)), static_cast<int>(std::floor(z / kCell)));
    };
    using Cell = std::pair<int, int>;

    // Lamps, benches and flower beds in one chunk per square: Base for the posts, a
    // layer of their own for the glowing heads.
    std::map<Cell, std::vector<glm::vec3>> streetLamps, shadowSpots;
    std::map<Cell, std::vector<glm::vec2>> globes;
    std::map<Cell, std::vector<Prop>> benches, beds;
    std::map<Cell, std::vector<Lamp>> pools;
    for (const glm::vec3& f : streetLampFrames_) streetLamps[cellOf(f.x, f.y)].push_back(f);
    for (const glm::vec2& g : parkLamps_) globes[cellOf(g.x, g.y)].push_back(g);
    for (const Prop& p : benches_) benches[cellOf(p.x, p.z)].push_back(p);
    for (const Prop& p : flowerBeds_) beds[cellOf(p.x, p.z)].push_back(p);
    for (const Lamp& l : lamps_) pools[cellOf(l.light.x, l.light.z)].push_back(l);
    for (const glm::vec3& s : shadows_) shadowSpots[cellOf(s.x, s.y)].push_back(s);

    chunks_.begin(Layer::Base, true);
    for (const auto& [key, spots] : streetLamps) {
        for (const glm::vec3& f : spots) {
            glPushMatrix();
            glTranslatef(f.x, 0.0f, f.y);
            glRotatef(f.z, 0, 1, 0);
            glCallList(props_.streetLamp);
            glPopMatrix();
        }
        chunks_.next();
    }
    for (const auto& [key, list] : benches) {
        for (const Prop& p : list) {
            glPushMatrix();
            glTranslatef(p.x, 0.0f, p.z);
            glRotatef(p.yaw, 0, 1, 0);
            glCallList(props_.bench);
            glPopMatrix();
        }
        chunks_.next();
    }
    for (const auto& [key, list] : beds) {
        for (const Prop& p : list) {
            glPushMatrix();
            glTranslatef(p.x, 0.0f, p.z);
            glRotatef(p.yaw, 0, 1, 0);
            glCallList(props_.flowerBed);
            glPopMatrix();
        }
        chunks_.next();
    }
    // The park lamp posts are not recorded with an angle: they are round.
    for (const auto& [key, list] : globes) {
        for (const glm::vec2& g : list) {
            glPushMatrix();
            glTranslatef(g.x, 0.0f, g.y);
            glCallList(props_.parkLamp);
            glPopMatrix();
        }
        chunks_.next();
    }

    chunks_.begin(Layer::Head, true);
    for (const auto& [key, spots] : streetLamps) {
        for (const glm::vec3& f : spots) {
            glPushMatrix();
            glTranslatef(f.x, 0.0f, f.y);
            glRotatef(f.z, 0, 1, 0);
            glCallList(props_.lampHead);
            glPopMatrix();
        }
        chunks_.next();
    }
    chunks_.begin(Layer::Globe, true);
    for (const auto& [key, list] : globes) {
        for (const glm::vec2& g : list) {
            glPushMatrix();
            glTranslatef(g.x, 0.0f, g.y);
            glCallList(props_.parkGlobe);
            glPopMatrix();
        }
        chunks_.next();
    }

    // Soft contact shadows under the lamps; pools of lamplight on the ground.
    chunks_.begin(Layer::Shadow, true);
    for (const auto& [key, spots] : shadowSpots) {
        glBegin(GL_QUADS);
        for (const glm::vec3& s : spots) {
            const float r = s.z;
            glTexCoord2f(0, 0); glVertex3f(s.x - r, 0.02f, s.y + r);
            glTexCoord2f(1, 0); glVertex3f(s.x + r, 0.02f, s.y + r);
            glTexCoord2f(1, 1); glVertex3f(s.x + r, 0.02f, s.y - r);
            glTexCoord2f(0, 1); glVertex3f(s.x - r, 0.02f, s.y - r);
        }
        glEnd();
        chunks_.next();
    }
    chunks_.begin(Layer::Pool, true);
    for (const auto& [key, list] : pools) {
        glBegin(GL_QUADS);
        for (const Lamp& l : list) {
            const float r = l.pool;
            glTexCoord2f(0, 0); glVertex3f(l.light.x - r, 0.03f, l.light.z + r);
            glTexCoord2f(1, 0); glVertex3f(l.light.x + r, 0.03f, l.light.z + r);
            glTexCoord2f(1, 1); glVertex3f(l.light.x + r, 0.03f, l.light.z - r);
            glTexCoord2f(0, 1); glVertex3f(l.light.x - r, 0.03f, l.light.z - r);
        }
        glEnd();
        chunks_.next();
    }
}

// --------------------------------------------------------------------------
// Empty plots
// --------------------------------------------------------------------------

void CityScene::buildEmptyPlots() {
    const Layout& L = *layout_;
    std::vector<int> widths;
    for (const auto& p : L.plots) widths.push_back(static_cast<int>(std::lround(p.width)));
    std::sort(widths.begin(), widths.end());
    widths.erase(std::unique(widths.begin(), widths.end()), widths.end());

    for (const int width : widths) {
        const float hw = static_cast<float>(width) * 0.5f, hd = L.lotDepth * 0.5f;
        // A free plot: unmown meadow, staked out with string lines every 10 m so
        // a visitor can read its size. The meadow is drawn in a colour of its own
        // for every plot (drawEmptyPlot), and its texture is turned a different
        // way on each, so the plots do not repeat each other. The lines and
        // flower beds are details, in a list of their own.
        for (const PlotLayout& p : L.plots) {
            if (static_cast<int>(std::lround(p.width)) != width) continue;
            // The colour of every corner is the colour of the meadow at that place in the
            // world, so the plot is part of the field it lies in.
            const auto field = [&p](float x, float z) {
                const glm::vec3 w = p.localToWorld({x, 0.0f, z});
                return Materials::meadowColour(w.x, w.z);
            };
            GLuint lawn = beginList();
            bind(materials_.meadow);
            geom::ground({-hw, hw, -hd, hd}, 0.0f, 0.05f, field);
            glEndList();
            lawnLists_[p.id] = lawn;
        }

        GLuint details = beginList();
        {
            const Decal decal;
            bind(materials_.plain);
            color(0.88f, 0.86f, 0.80f);
            for (float x = -hw + 10.0f; x < hw - 1.0f; x += 10.0f) geom::ground({x - 0.04f, x + 0.04f, -hd, hd}, 0.0f, 1.0f);
            for (float z = -hd + 10.0f; z < hd - 1.0f; z += 10.0f) geom::ground({-hw, hw, z - 0.04f, z + 0.04f}, 0.0f, 1.0f);
            bind(materials_.concrete);
            white();
            geom::ground({-2.0f, 2.0f, hd - 14.0f, hd}, 0.0f, 0.25f);  // the way in from the gate
        }
        for (const float x : {-6.0f, 6.0f}) {
            glPushMatrix();
            glTranslatef(x, 0.0f, hd - 6.0f);
            glRotatef(90.0f, 0, 1, 0);
            glCallList(props_.flowerBed);
            glPopMatrix();
        }
        glEndList();
        detailLists_[width] = details;

        // A plot whose team is not running: a building site. The crane is
        // drawn separately because its jib turns.
        GLuint site = beginList();
        bind(materials_.soil);
        white();
        geom::ground({-hw, hw, -hd, hd}, 0.0f, 0.25f);
        bind(materials_.plain);
        const float cabins[3][3] = {{0.95f, 0.55f, 0.15f}, {0.25f, 0.55f, 0.85f}, {0.95f, 0.95f, 0.92f}};
        for (int k = 0; k < 3; ++k) {
            color(cabins[k][0], cabins[k][1], cabins[k][2]);
            glPushMatrix();
            glTranslatef(hw - 9.0f, 1.3f + 2.6f * (k == 2), hd - 12.0f - 3.2f * (k % 2));
            shapes::box(6.0f, 2.6f, 2.5f);
            glPopMatrix();
        }
        color(0.75f, 0.62f, 0.45f);
        for (int k = 0; k < 4; ++k) {  // stacks of material
            glPushMatrix();
            glTranslatef(-hw + 10.0f + 4.0f * k, 0.5f, hd - 10.0f);
            shapes::box(3.0f, 1.0f, 2.0f);
            glPopMatrix();
        }
        bind(materials_.concrete);
        color(0.85f, 0.85f, 0.83f);
        geom::block({-12.0f, 0.0f, -20.0f}, {12.0f, 1.2f, 4.0f}, 0.25f);  // the first slab
        glEndList();
        siteLists_[width] = site;
    }
}

// --------------------------------------------------------------------------
// Drawing
// --------------------------------------------------------------------------

// How far each layer is drawn, in metres. A lane line or a pool of light a
// few hundred metres off is thinner than a pixel and only crawls.
constexpr float kLayerDistance[kLayerCount] = {
    1e9f,    // Base
    1e9f,    // Paint
    1e9f,    // Plaza
    160.0f,  // Detail
    260.0f,  // Shadow
    420.0f,  // Pool
    520.0f,  // Head
    360.0f,  // Globe
    1e9f,    // Lights
};

void CityScene::beginFrame(const Viewer& viewer) {
    viewer_ = viewer;
    for (int l = 0; l < kLayerCount; ++l) {
        std::vector<unsigned>& in = visible_[l];
        in.clear();
        const std::vector<Chunk>& all = chunks_.layer(static_cast<Layer>(l));
        const float limit = std::min(viewer.distance, kLayerDistance[l]);
        for (unsigned i = 0; i < all.size(); ++i) {
            const Chunk& c = all[i];
            if (distanceToBox(viewer.eye, c.min, c.max) > limit) continue;
            if (!viewer.frustum.box(c.min, c.max)) continue;
            in.push_back(i);
        }
    }
}

void CityScene::drawOpaque(const Environment& env, double time) const {
    const Layout& L = *layout_;
    stats_.chunks = static_cast<int>(chunks_.count());
    stats_.chunksDrawn = 0;
    stats_.vertices = 0;
    const auto draw = [&](Layer layer) {
        const std::vector<Chunk>& all = chunks_.layer(layer);
        for (const unsigned i : visible_[static_cast<size_t>(layer)]) {
            glCallList(all[i].list);
            stats_.vertices += all[i].vertices;
            ++stats_.chunksDrawn;
        }
    };

    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_POLYGON_BIT | GL_TEXTURE_BIT | GL_LIGHTING_BIT);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    if (faceCulling_) glEnable(GL_CULL_FACE);
    draw(Layer::Base);
    // Decals: pulled towards the viewer, the squares over the paths over the lawn.
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -2.0f);
    draw(Layer::Paint);
    draw(Layer::Detail);
    glPolygonOffset(-2.0f, -4.0f);
    draw(Layer::Plaza);
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Soft contact shadows under the lamps.
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-3.0f, -6.0f);
    glBindTexture(GL_TEXTURE_2D, materials_.blob);
    glColor4f(1.0f, 1.0f, 1.0f, 0.30f + 0.35f * env.daylight);
    draw(Layer::Shadow);
    if (env.lampLevel > 0.01f) {
        // Additive light pools. Fog is made black so it cannot add up too.
        const GLfloat black[4] = {0, 0, 0, 1};
        glPushAttrib(GL_FOG_BIT);
        glFogfv(GL_FOG_COLOR, black);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glBindTexture(GL_TEXTURE_2D, materials_.glow);
        glColor4f(1.0f, 0.86f, 0.64f, 0.55f * env.lampLevel);
        draw(Layer::Pool);
        glPopAttrib();
    }
    glPopAttrib();

    // Lamp heads and globes: emissive at night.
    glBindTexture(GL_TEXTURE_2D, materials_.plain);
    const float k = env.lampLevel;
    const GLfloat glow[4] = {1.0f * k, 0.94f * k, 0.82f * k, 1.0f};
    const GLfloat none[4] = {0, 0, 0, 1};
    glColor3f(0.85f, 0.85f, 0.82f);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, glow);
    draw(Layer::Head);
    draw(Layer::Globe);

    // The tower: rings of light that change colour slowly through the night,
    // and a red aviation light that blinks once a second.
    const float hue = static_cast<float>(std::fmod(time * 0.02, 1.0)) * 2.0f * kPi;
    const glm::vec3 led = glm::vec3(0.55f) + 0.45f * glm::vec3(std::cos(hue), std::cos(hue - 2.094f), std::cos(hue + 2.094f));
    const GLfloat ring[4] = {led.r * k, led.g * k, led.b * k, 1.0f};
    glColor3f(0.92f, 0.94f, 0.96f);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, ring);
    draw(Layer::Lights);
    const bool blink = std::fmod(time, 1.2) < 0.25;
    const GLfloat red[4] = {blink ? 1.0f : 0.25f, blink ? 0.1f : 0.02f, 0.02f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, red);
    glColor3f(0.9f, 0.1f, 0.05f);
    glPushMatrix();
    glTranslatef(L.tower.x, L.towerHeight, L.tower.y);
    shapes::sphere(0.45f, 10, 8);
    glPopMatrix();
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, none);
    glPopAttrib();
}

void CityScene::drawEmptyPlot(const PlotLayout& plot, bool assigned, double time) const {
    const int width = static_cast<int>(std::lround(plot.width));
    const auto lawn = lawnLists_.find(plot.id);
    const auto site = siteLists_.find(width);
    if (lawn == lawnLists_.end() || site == siteLists_.end()) return;
    const glm::vec3 centre{plot.center.x, 0.0f, plot.center.y};
    const float distance = glm::length(centre - viewer_.eye);
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_POLYGON_BIT | GL_TEXTURE_BIT);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
    glPushMatrix();
    glTranslatef(plot.center.x, 0.0f, plot.center.y);
    glRotatef(plot.yawDegrees, 0, 1, 0);
    if (!assigned) {
        glCallList(lawn->second);
        // String lines and flower beds are thinner than a pixel from afar.
        if (distance < 130.0f) glCallList(detailLists_.at(width));
    } else {
        glCallList(site->second);
        // The crane at the back corner; its jib turns slowly, each plot at
        // its own pace (T(mast) * Ry(angle) * jib).
        const float seed = plot.center.x * 0.37f + plot.center.y * 0.11f;
        const float angle = static_cast<float>(std::fmod(time * 4.0 + seed * 40.0, 360.0));
        glTranslatef(-plot.width * 0.5f + 9.0f, 0.0f, -plot.depth * 0.5f + 10.0f);
        glCallList(props_.craneMast);
        glRotatef(angle, 0, 1, 0);
        glCallList(props_.craneJib);
    }
    glPopMatrix();
    glPopAttrib();
}

void CityScene::drawTransparent(const glm::mat4& view, const Environment& env, double time) const {
    const Layout& L = *layout_;
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT |
                 GL_LIGHTING_BIT | GL_TRANSFORM_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);

    // Water: the ripple texture slides through the texture matrix, so no
    // geometry has to move.
    const float flow = static_cast<float>(std::fmod(time * 0.02, 1.0));
    glBindTexture(GL_TEXTURE_2D, materials_.water);
    const GLfloat spec[4] = {0.7f, 0.7f, 0.7f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 64.0f);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glTranslatef(flow, flow * 0.6f, 0.0f);
    glMatrixMode(GL_MODELVIEW);
    glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
    geom::ellipse(L.lake, L.lakeRadius, -0.3f, kLakeSegments, 0.05f);
    geom::ellipse(L.fountain, glm::vec2(L.fountainRadius), 0.4f, 64, 0.25f);
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    const GLfloat noSpec[4] = {0, 0, 0, 1};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, noSpec);

    // Billboards face the camera: take right/up from the view matrix.
    const glm::vec3 right{view[0][0], view[1][0], view[2][0]};
    const glm::vec3 up{view[0][1], view[1][1], view[2][1]};
    auto billboard = [&](const glm::vec3& c, float size) {
        const glm::vec3 r = right * size, u = up * size;
        glTexCoord2f(0, 0); glVertex3f(c.x - r.x - u.x, c.y - r.y - u.y, c.z - r.z - u.z);
        glTexCoord2f(1, 0); glVertex3f(c.x + r.x - u.x, c.y + r.y - u.y, c.z + r.z - u.z);
        glTexCoord2f(1, 1); glVertex3f(c.x + r.x + u.x, c.y + r.y + u.y, c.z + r.z + u.z);
        glTexCoord2f(0, 1); glVertex3f(c.x - r.x + u.x, c.y - r.y + u.y, c.z - r.z + u.z);
    };

    glDisable(GL_LIGHTING);
    glBindTexture(GL_TEXTURE_2D, materials_.glow);

    // Fountain jets: droplets on parabolas, each with its own phase. Only
    // while the fountain is near and in view.
    const glm::vec3 basin{L.fountain.x, 5.0f, L.fountain.y};
    const bool jetsInView = glm::length(basin - viewer_.eye) < 400.0f && viewer_.frustum.sphere(basin, L.fountainRadius + 10.0f);
    glBegin(GL_QUADS);
    const float t = static_cast<float>(time);
    for (size_t f = 0; jetsInView && f < fountains_.size(); ++f) {
        const Fountain& fo = fountains_[f];
        const float duration = std::sqrt(fo.height) * 0.9f;
        const float v0 = 2.0f * fo.height / (duration * 0.5f);
        const float g = v0 / (duration * 0.5f);
        const int drops = fo.height > 4.0f ? 90 : 30;
        for (int k = 0; k < drops; ++k) {
            const float phase = std::fmod(t / duration + static_cast<float>(k) / drops + f * 0.13f, 1.0f);
            const float s = phase * duration;
            const float a = static_cast<float>(k) * 2.39996f;  // golden angle spreads the droplets
            const float spread = fo.spread * s;
            const glm::vec3 p = fo.base + glm::vec3(std::cos(a) * spread, v0 * s - 0.5f * g * s * s, std::sin(a) * spread);
            glColor4f(0.88f, 0.95f, 1.0f, 0.55f * (1.0f - phase * 0.6f));
            billboard(p, 0.14f + 0.06f * phase);
        }
    }
    glEnd();

    // Lamp halos and the tower beacon at night (additive).
    if (env.lampLevel > 0.01f) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glBegin(GL_QUADS);
        glColor4f(1.0f, 0.92f, 0.78f, 0.5f * env.lampLevel);
        for (const Lamp& l : lamps_) {
            const glm::vec3 away = l.light - viewer_.eye;
            if (glm::dot(away, away) > 450.0f * 450.0f || !viewer_.frustum.sphere(l.light, 2.0f)) continue;
            billboard(l.light, l.pool > 6.0f ? 1.5f : 1.0f);
        }
        if (std::fmod(time, 1.2) < 0.25) {
            glColor4f(1.0f, 0.15f, 0.08f, 0.8f * env.lampLevel);
            billboard({L.tower.x, L.towerHeight, L.tower.y}, 3.0f);
        }
        glEnd();
    }
    glPopAttrib();
}

std::vector<std::string> CityScene::statsLines() const {
    const Census& c = census_;
    const FrameStats& f = stats_;
    long all = 0;
    static const char* const names[kLayerCount] = {"base", "paint", "plaza",  "detail", "shadow",
                                                  "pool", "head",  "globe", "lights"};
    std::string layers = "chunks       ";
    for (int l = 0; l < kLayerCount; ++l) {
        const Layer layer = static_cast<Layer>(l);
        all += chunks_.vertices(layer);
        layers += std::string(l ? ", " : "") + names[l] + " " + std::to_string(visible_[l].size()) + "/" +
                  std::to_string(chunks_.layer(layer).size());
    }
    layers += "  (in view / in all)";
    return {"lamps        street " + std::to_string(c.streetLamps) + ", park " + std::to_string(c.parkLamps) +
                ";  benches " + std::to_string(c.benches) + ", flower beds " + std::to_string(c.flowerBeds),
            layers,
            "scenery      " + std::to_string(f.chunksDrawn) + " of " + std::to_string(f.chunks) + " chunks drawn (" +
                std::to_string(all) + " vertices in all the chunks), " + std::to_string(f.vertices) + " vertices"};
}

void CityScene::drawPlotBorders(const glm::vec3& highlightColor, const std::string& highlightPlot) const {
    const Layout& L = *layout_;
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const float h = L.plotHeight;
    for (const auto& p : L.plots) {
        const float hw = p.width * 0.5f, hd = p.depth * 0.5f;
        const bool hi = p.id == highlightPlot;
        glLineWidth(hi ? 2.5f : 1.2f);
        if (hi) glColor4f(highlightColor.r, highlightColor.g, highlightColor.b, 1.0f);
        else glColor4f(1.0f, 0.85f, 0.3f, 0.7f);
        glPushMatrix();
        glTranslatef(p.center.x, 0.0f, p.center.y);
        glRotatef(p.yawDegrees, 0, 1, 0);
        for (const float y : {0.05f, h}) {
            glBegin(GL_LINE_LOOP);
            glVertex3f(-hw, y, -hd); glVertex3f(hw, y, -hd); glVertex3f(hw, y, hd); glVertex3f(-hw, y, hd);
            glEnd();
        }
        glBegin(GL_LINES);
        for (const float x : {-hw, hw})
            for (const float z : {-hd, hd}) { glVertex3f(x, 0, z); glVertex3f(x, h, z); }
        // Gate marker.
        glVertex3f(-L.gateWidth * 0.5f, 0.1f, hd); glVertex3f(-L.gateWidth * 0.5f, 3.0f, hd);
        glVertex3f(L.gateWidth * 0.5f, 0.1f, hd); glVertex3f(L.gateWidth * 0.5f, 3.0f, hd);
        glEnd();
        glPopMatrix();
    }
    glPopAttrib();
}

}  // namespace citylab
