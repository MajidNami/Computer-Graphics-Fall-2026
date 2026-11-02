// The built parts of the city: the tower in the park, the gate and frontage
// of every plot, district signs and bus shelters. All of it is compiled once
// into display lists by CityScene::build().
//
// Every piece is modelled in its own local frame (GroundFrame), so one piece
// of code serves all four directions: the gate of a north-facing plot is the
// same code as the gate of a south-facing one.

#include <algorithm>
#include <cmath>

#include "citylab/Shapes.h"
#include "city/CityScene.h"
#include "city/Frame.h"
#include "city/Geometry.h"
#include "engine/Collision.h"

namespace citylab {
namespace {

constexpr float kFacadeUv = 1.0f / 12.0f;  // the facade textures repeat every 12 m

// Architecture drawn in a local ground frame. Each call both draws a part
// and registers its world-space collider, so geometry and collisions can
// never drift apart.
class Piece {
public:
    Piece(const GroundFrame& frame, CollisionWorld& collisions) : frame_(frame), collisions_(collisions) {
        glPushMatrix();
        frame_.apply();
    }
    ~Piece() { glPopMatrix(); }
    Piece(const Piece&) = delete;
    Piece& operator=(const Piece&) = delete;

    void solid(const glm::vec3& min, const glm::vec3& max) const { collisions_.add(frame_.toWorld(min, max)); }

    void block(const glm::vec3& min, const glm::vec3& max, float uv, bool collide = true) const {
        geom::block(min, max, uv);
        if (collide) solid(min, max);
    }

private:
    GroundFrame frame_;
    CollisionWorld& collisions_;
};

}  // namespace

// --------------------------------------------------------------------------
// Signs
// --------------------------------------------------------------------------

void CityScene::signBoard(GLuint texture, float width, float height) {
    // A slim board in the local x-y plane, centred on the origin, with the
    // sign image on its +z face. Signs are pre-rendered PNGs (Persian text
    // needs proper shaping), so a missing file just leaves a plain board.
    bind(materials_.plain);
    color(0.20f, 0.22f, 0.26f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -0.06f);
    shapes::box(width + 0.16f, height + 0.16f, 0.12f);
    glPopMatrix();
    if (texture) {
        bind(texture);
        white();
    } else {
        color(0.95f, 0.95f, 0.93f);
    }
    // A little light of its own keeps a sign readable on the shady side.
    const GLfloat glow[4] = {0.35f, 0.35f, 0.35f, 1.0f}, none[4] = {0, 0, 0, 1};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, glow);
    const float hw = width * 0.5f, hh = height * 0.5f;
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glTexCoord2f(0, 0); glVertex3f(-hw, -hh, 0.005f);
    glTexCoord2f(1, 0); glVertex3f(hw, -hh, 0.005f);
    glTexCoord2f(1, 1); glVertex3f(hw, hh, 0.005f);
    glTexCoord2f(0, 1); glVertex3f(-hw, hh, 0.005f);
    glEnd();
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, none);
}

void CityScene::buildSigns() {
    const Layout& L = *layout_;
    // A sign on two posts, standing at (x, z) and facing `yaw`.
    auto standing = [&](const std::string& sign, float x, float z, float yaw, const glm::vec3& accent) {
        chunks_.next();
        Piece p({{x, z}, yaw}, *collisions_);
        bind(materials_.plain);
        color(accent);
        for (const float s : {-2.7f, 2.7f}) p.block({s - 0.1f, 0.0f, -0.1f}, {s + 0.1f, 3.6f, 0.1f}, 1.0f);
        glPushMatrix();
        glTranslatef(0.0f, 2.6f, 0.0f);
        signBoard(signs_.at(sign), 5.2f, 1.3f);
        glPopMatrix();
        keepClear_.push_back({x - 4.0f, x + 4.0f, z - 4.0f, z + 4.0f});
    };
    // District signs in the middle of each block's two side sidewalks,
    // facing the avenue.
    for (const auto& d : L.districts) {
        const float z = d.rect.center().y, off = 2.9f;
        standing("district-" + d.id, d.rect.x0 - off, z, -90.0f, d.color);
        standing("district-" + d.id, d.rect.x1 + off, z, 90.0f, d.color);
    }

    // Shelters at the stops of the tram boulevard, on both sides.
    for (const glm::vec3& stop : L.tramStops()) {
        chunks_.next();
        Piece p({{stop.x, stop.y}, stop.z}, *collisions_);
        glCallList(props_.busShelter);
        p.solid({-3.0f, 0.0f, -1.0f}, {3.0f, 2.6f, -0.9f});
        for (const float x : {-3.0f, 2.9f}) p.solid({x, 0.0f, -1.0f}, {x + 0.1f, 2.6f, 0.2f});
        keepClear_.push_back({stop.x - 5.0f, stop.x + 5.0f, stop.y - 3.0f, stop.y + 3.0f});
    }
}

// --------------------------------------------------------------------------
// The tower
// --------------------------------------------------------------------------

void CityScene::buildTower() {
    // A modern TV and observation tower in the middle of the park: a glass
    // lobby, a tapering concrete shaft, a glazed observation pod and a mast.
    // Heights are fractions of the tower height from layout.json.
    const Layout& L = *layout_;
    const float H = L.towerHeight;
    const float shaftTop = 0.78f * H, podBottom = 0.70f * H, podGlass = 0.75f * H, podTop = 0.79f * H;
    auto stage = [&](float y, auto&& draw) {
        glPushMatrix();
        glTranslatef(L.tower.x, y, L.tower.y);
        draw();
        glPopMatrix();
    };
    chunks_.next();  // the tower is a chunk of its own
    bind(materials_.glass);
    color(0.9f, 0.97f, 1.0f);
    stage(0.0f, [] { geom::frustum(11.0f, 11.0f, 6.0f, 40, kFacadeUv, false); });
    bind(materials_.concrete);
    color(0.96f, 0.96f, 0.95f);
    stage(6.0f, [] { geom::frustum(12.5f, 12.5f, 0.7f, 40, 0.25f, true); });
    stage(6.7f, [&] { geom::frustum(4.8f, 3.0f, shaftTop - 6.7f, 24, 0.25f, false); });
    stage(podBottom, [&] { geom::frustum(3.1f, 11.0f, podGlass - podBottom, 32, 0.25f, false); });
    bind(materials_.glass);
    color(0.85f, 0.95f, 1.0f);
    stage(podGlass, [&] { geom::frustum(11.0f, 12.0f, podTop - podGlass, 40, kFacadeUv, false); });
    bind(materials_.concrete);
    color(0.96f, 0.96f, 0.95f);
    stage(podTop, [] { geom::frustum(12.1f, 8.5f, 2.2f, 40, 0.25f, true); });
    stage(podTop + 2.2f, [] { geom::frustum(6.0f, 6.0f, 3.0f, 32, 0.25f, true); });
    bind(materials_.plain);
    color(0.92f, 0.30f, 0.25f);  // red and white bands on the mast
    const float mast0 = podTop + 5.2f;
    for (int k = 0; k < 4; ++k) {
        const float y0 = mast0 + (H - mast0) * k / 4.0f, y1 = mast0 + (H - mast0) * (k + 1) / 4.0f;
        const float r0 = 1.1f - 0.8f * k / 4.0f, r1 = 1.1f - 0.8f * (k + 1) / 4.0f;
        if (k % 2) color(0.95f, 0.95f, 0.95f);
        else color(0.92f, 0.30f, 0.25f);
        stage(y0, [&] { geom::frustum(r0, r1, y1 - y0, 12, 0.25f, k == 3); });
    }
    const float s = 11.0f * 0.75f;
    solid({L.tower.x - s, 0.0f, L.tower.y - s}, {L.tower.x + s, 7.0f, L.tower.y + s});
    solid({L.tower.x - 11.0f, 0.0f, L.tower.y - 4.0f}, {L.tower.x + 11.0f, 7.0f, L.tower.y + 4.0f});
    solid({L.tower.x - 4.0f, 0.0f, L.tower.y - 11.0f}, {L.tower.x + 4.0f, 7.0f, L.tower.y + 11.0f});
    chunks_.next();
}

void CityScene::buildTowerLights() {
    // Rings of light around the shaft and the pod, drawn emissive at night.
    const Layout& L = *layout_;
    const float H = L.towerHeight;
    const float shaftTop = 0.78f * H, podBottom = 0.70f * H, podTop = 0.79f * H;
    auto stage = [&](float y, auto&& draw) {
        glPushMatrix();
        glTranslatef(L.tower.x, y, L.tower.y);
        draw();
        glPopMatrix();
    };
    glBindTexture(GL_TEXTURE_2D, materials_.plain);
    for (float y = 14.0f; y < podBottom - 4.0f; y += 12.0f) {
        const float t = (y - 6.7f) / (shaftTop - 6.7f), r = 4.8f + (3.0f - 4.8f) * t + 0.06f;
        stage(y, [&] { geom::frustum(r, r - 0.01f, 0.5f, 24, 0.25f, false); });
    }
    stage(podTop - 0.1f, [] { geom::frustum(12.15f, 12.15f, 0.35f, 40, 0.25f, false); });
}

// --------------------------------------------------------------------------
// Plot frontages and gates
// --------------------------------------------------------------------------

void CityScene::buildPlotFrames(const Registry& registry) {
    (void)registry;  // the sign images already carry the team's title
    const Layout& L = *layout_;
    const float gate = L.gateWidth * 0.5f;
    // Everything stays just outside the plot volume, which belongs to the team.
    const float o = 0.15f, planter = 0.9f, z0 = o, z1 = o + planter;

    for (const PlotLayout& p : L.plots) {
        const DistrictLayout* d = L.findDistrict(p.district);
        const glm::vec3 accent = d ? d->color : glm::vec3(0.92f, 0.92f, 0.90f);
        const float hw = p.width * 0.5f, hd = p.depth * 0.5f;
        chunks_.next();  // each plot's frontage is a chunk
        Piece frame({p.center, p.yawDegrees}, *collisions_);

        // Low concrete planters with a clipped hedge along the frontage, and
        // along any side of the plot that is not next to another plot.
        auto planterRun = [&](const glm::vec3& a, const glm::vec3& b) {
            bind(materials_.concrete);
            color(0.93f, 0.93f, 0.91f);
            frame.block(a, {b.x, 0.5f, b.z}, 0.5f);
            bind(materials_.foliage);
            color(0.62f, 0.78f, 0.58f);  // darker than the lawns, so the frontage reads
            geom::block({a.x + 0.08f, 0.5f, a.z + 0.08f}, {b.x - 0.08f, 0.95f, b.z - 0.08f}, 0.5f);
        };
        planterRun({-hw, 0.0f, hd + z0}, {-gate - 1.0f, 0.0f, hd + z1});
        planterRun({gate + 1.0f, 0.0f, hd + z0}, {hw, 0.0f, hd + z1});
        auto outside = [&](const glm::vec3& local) {
            const glm::vec3 w = p.localToWorld(local);
            return L.plotAt(w.x, w.z) == nullptr;
        };
        if (outside({-hw - 1.0f, 0.0f, 0.0f})) planterRun({-hw - z1, 0.0f, -hd}, {-hw - z0, 0.0f, hd + z1});
        if (outside({hw + 1.0f, 0.0f, 0.0f})) planterRun({hw + z0, 0.0f, -hd}, {hw + z1, 0.0f, hd + z1});
        if (outside({0.0f, 0.0f, -hd - 1.0f})) planterRun({-hw - z1, 0.0f, -hd - z1}, {hw + z1, 0.0f, -hd - z0});

        // The gate: two slim pylons in the district colour, a beam, and the
        // plot's sign hanging under it, facing the street.
        const float zc = hd + (z0 + z1) * 0.5f;
        bind(materials_.plain);
        color(accent);
        for (const float s : {-1.0f, 1.0f}) {
            const float x = s * (gate + 0.5f);
            frame.block({x - 0.25f, 0.0f, zc - 0.25f}, {x + 0.25f, 5.6f, zc + 0.25f}, 1.0f);
        }
        color(0.95f, 0.95f, 0.95f);
        geom::block({-gate - 0.85f, 5.6f, zc - 0.3f}, {gate + 0.85f, 6.1f, zc + 0.3f}, 1.0f);
        glPushMatrix();
        glTranslatef(0.0f, 4.45f, zc + 0.12f);
        signBoard(signs_["plot-" + p.id], 7.2f, 1.8f);
        glPopMatrix();
    }
}

}  // namespace citylab
