#include "city/Life.h"

#include <algorithm>
#include <cmath>

#include "citylab/Shapes.h"
#include "city/Geometry.h"

namespace citylab {
namespace {

constexpr float kPi = 3.14159265f;
constexpr float kTramLength = 27.0f;
constexpr float kSignalCycle = 36.0f;    // seconds: along x green, amber, all red, then along z

void color(float r, float g, float b) { glColor3f(r, g, b); }

void emission(float r, float g, float b) {
    const GLfloat e[4] = {r, g, b, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, e);
}

}  // namespace

// --------------------------------------------------------------------------
// Tram tracks and trams
// --------------------------------------------------------------------------

void CityLife::build(const Layout& L) {
    layout_ = &L;
    junctions_ = L.junctions();
    tracks_.clear();

    const Rect& c = L.city;
    const float reach = Layout::kExitLength;
    for (const StreetLayout& street : L.streets) {
        const Rect& b = street.rect;
        const bool ax = street.alongX();
        // The tram boulevard crosses the whole city; it has two tracks.
        const bool through = ax ? b.x0 <= c.x0 + 0.1f && b.x1 >= c.x1 - 0.1f : b.z0 <= c.z0 + 0.1f && b.z1 >= c.z1 - 0.1f;
        if (!through || street.kind != "tram") continue;
        const float mid = ax ? b.center().y : b.center().x;
        const float offset = L.tramWidth * 0.25f;  // of each track from the middle of the boulevard
        for (const float dir : {1.0f, -1.0f}) {
            Track track;
            track.alongX = ax;
            // Keep right: heading +x the right-hand side is +z, heading +z it is -x.
            const float across = ax ? mid + dir * offset : mid - dir * offset;
            const float lo = (ax ? c.x0 : c.z0) - reach, hi = (ax ? c.x1 : c.z1) + reach;
            const float from = dir > 0.0f ? lo : hi;
            track.start = ax ? glm::vec2(from, across) : glm::vec2(across, from);
            track.dir = ax ? glm::vec2(dir, 0.0f) : glm::vec2(0.0f, dir);
            track.length = hi - lo;
            const auto along = [&](float coordinate) { return (coordinate - from) * dir; };

            // A stop line 6 m before every junction, behind the zebra crossing.
            for (const Rect& j : junctions_) {
                const float a0 = ax ? j.z0 : j.x0, a1 = ax ? j.z1 : j.x1;
                if (across < a0 || across > a1) continue;
                const float entry = dir > 0.0f ? (ax ? j.x0 : j.z0) - 6.0f : (ax ? j.x1 : j.z1) + 6.0f;
                track.stopLines.push_back(along(entry));
            }
            std::sort(track.stopLines.begin(), track.stopLines.end());

            // Each track serves the shelters on its own side of the boulevard.
            for (const glm::vec3& stop : L.tramStops())
                if ((stop.y < L.tramZ) == (across < L.tramZ)) track.halts.push_back(along(stop.x) + kTramLength * 0.5f);
            std::sort(track.halts.begin(), track.halts.end());
            for (int k = 2; k >= 0; --k) {
                Tram t;
                t.s = track.length * (static_cast<float>(k) + 0.3f) / 3.0f;
                t.nextHalt = static_cast<int>(
                    std::upper_bound(track.halts.begin(), track.halts.end(), t.s) - track.halts.begin());
                track.trams.push_back(t);
            }
            tracks_.push_back(std::move(track));
        }
    }

    buildModels();
}

CityLife::Light CityLife::light(bool alongX) const {
    const float t = std::fmod(signalClock_, kSignalCycle) - (alongX ? 0.0f : kSignalCycle * 0.5f);
    const float u = t < 0.0f ? t + kSignalCycle : t;
    return u < 15.0f ? Light::Green : u < 17.0f ? Light::Amber : Light::Red;
}

void CityLife::update(float dt, const glm::vec3& walker, bool onFoot) {
    if (!layout_) return;
    signalClock_ += dt;
    for (Track& track : tracks_) advance(track, dt, walker, onFoot);
}

// Every tram drives as fast as it may: its top speed, but never so fast that
// it could not stop with its usual braking (v = sqrt(2 a d)) before the tram
// ahead, a red light, the walker or its next stop.
void CityLife::advance(Track& track, float dt, const glm::vec3& walker, bool onFoot) {
    const size_t n = track.trams.size();
    if (n == 0) return;
    const float accel = 1.2f, brake = 1.4f;
    const Light signal = light(track.alongX);

    const glm::vec2 rel = glm::vec2(walker.x, walker.z) - track.start;
    const float walkerAlong = glm::dot(rel, track.dir);
    const bool walkerOnTrack = onFoot && walker.y < 2.0f && glm::length(rel - track.dir * walkerAlong) < 2.3f;

    for (size_t i = 0; i < n; ++i) {
        Tram& v = track.trams[i];
        float limit = v.topSpeed;
        const auto stopWithin = [&](float distance) {
            limit = std::min(limit, std::sqrt(2.0f * brake * std::max(0.0f, distance)));
        };
        if (n > 1) {
            const Tram& ahead = track.trams[(i + n - 1) % n];
            stopWithin(ahead.s - v.s - kTramLength - 3.0f + (i == 0 ? track.length : 0.0f));
        }
        for (const float line : track.stopLines) {
            const float d = line - v.s;
            if (d < -0.5f) continue;
            if (d > 90.0f) break;
            const bool canStop = v.speed * v.speed / (2.0f * brake) < d + 0.5f;
            if (signal == Light::Red || (signal == Light::Amber && canStop)) stopWithin(d - 0.3f);
            break;
        }
        if (walkerOnTrack) {
            const float d = walkerAlong - v.s;
            if (d > -1.0f && d < 45.0f) stopWithin(d - 3.0f);
        }
        if (v.dwell > 0.0f) {
            v.dwell -= dt;
            limit = 0.0f;
        } else if (v.nextHalt < static_cast<int>(track.halts.size())) {
            const float d = track.halts[static_cast<size_t>(v.nextHalt)] - v.s;
            if (d < 0.4f && v.speed < 0.3f) {
                v.dwell = 15.0f;  // doors open, people get on and off
                ++v.nextHalt;
                limit = 0.0f;
            } else {
                stopWithin(d);
            }
        }
        if (limit > v.speed) v.speed = std::min(limit, v.speed + accel * dt);
        else v.speed = std::max(limit, v.speed - 2.0f * brake * dt);
        v.s += v.speed * dt;
    }
    // The front tram has left the far end: it starts again at the back.
    if (track.trams.front().s >= track.length) {
        track.trams.front().s -= track.length;
        track.trams.front().nextHalt = 0;
        std::rotate(track.trams.begin(), track.trams.begin() + 1, track.trams.end());
    }
}

// --------------------------------------------------------------------------
// Models: each has its front at z = 0 and its body towards -z
// --------------------------------------------------------------------------

void CityLife::buildModels() {
    if (!tram_) {
        tram_ = glGenLists(1);
        tramLights_ = glGenLists(1);
        for (size_t j = 0; j < junctions_.size(); ++j) signalPoles_.push_back(glGenLists(1));
    }

    // The city's tram: three sections in white with the transit orange.
    const float sectionL = (kTramLength - 0.9f) / 3.0f, w = 1.25f;
    glNewList(tram_, GL_COMPILE);
    for (int s = 0; s < 3; ++s) {
        const float z1 = -s * (sectionL + 0.45f), z0 = z1 - sectionL;
        color(0.95f, 0.95f, 0.94f);
        geom::block({-w, 0.4f, z0}, {w, 1.3f, z1}, 0.0f);
        geom::block({-w, 2.8f, z0}, {w, 3.3f, z1}, 0.0f);
        color(1.0f, 0.55f, 0.15f);
        geom::block({-w - 0.01f, 1.3f, z0}, {w + 0.01f, 1.5f, z1}, 0.0f);
        color(0.17f, 0.22f, 0.26f);
        geom::block({-w + 0.02f, 1.5f, z0 + 0.1f}, {w - 0.02f, 2.8f, z1 - 0.1f}, 0.0f);
        color(0.85f, 0.86f, 0.87f);
        geom::block({-0.8f, 3.3f, z0 + 2.0f}, {0.8f, 3.6f, z1 - 2.0f}, 0.0f);
        color(0.18f, 0.18f, 0.20f);
        for (const float z : {z0 + 1.8f, z1 - 1.8f}) geom::block({-1.0f, 0.05f, z - 1.0f}, {1.0f, 0.45f, z + 1.0f}, 0.0f);
        if (s < 2) geom::block({-1.0f, 0.5f, z0 - 0.45f}, {1.0f, 3.1f, z0}, 0.0f);  // the bellows
    }
    color(0.20f, 0.20f, 0.22f);  // the pantograph on the middle section, raised to the wire
    const float zp = -(sectionL + 0.45f) - sectionL * 0.5f;
    geom::block({-0.05f, 3.6f, zp - 0.6f}, {0.05f, 5.7f, zp - 0.5f}, 0.0f);
    geom::block({-0.8f, 5.7f, zp - 0.65f}, {0.8f, 5.76f, zp - 0.45f}, 0.0f);
    glEndList();

    glNewList(tramLights_, GL_COMPILE);
    emission(0.55f, 0.50f, 0.38f);  // lit from inside
    color(0.9f, 0.85f, 0.65f);
    for (int s = 0; s < 3; ++s) {
        const float z1 = -s * (sectionL + 0.45f), z0 = z1 - sectionL;
        geom::block({-w - 0.005f, 1.6f, z0 + 0.4f}, {w + 0.005f, 2.7f, z1 - 0.4f}, 0.0f);
    }
    emission(1.0f, 0.95f, 0.8f);
    color(1.0f, 0.95f, 0.8f);
    for (const float x : {-0.8f, 0.8f}) geom::block({x - 0.15f, 0.7f, 0.0f}, {x + 0.15f, 0.9f, 0.02f}, 0.0f);
    emission(0.9f, 0.08f, 0.05f);
    color(0.9f, 0.1f, 0.1f);
    for (const float x : {-0.8f, 0.8f})
        geom::block({x - 0.15f, 0.7f, -kTramLength - 0.02f}, {x + 0.15f, 0.9f, -kTramLength}, 0.0f);
    emission(0.0f, 0.0f, 0.0f);
    glEndList();

    // One signal for each way into every junction, on the right-hand corner
    // before it, facing the traffic that comes.
    for (size_t n = 0; n < junctions_.size(); ++n) {
        const Rect& j = junctions_[n];
        glNewList(signalPoles_[n], GL_COMPILE);
        color(0.24f, 0.26f, 0.29f);
        for (const glm::vec2& p : {glm::vec2(j.x0 - 1.2f, j.z1 + 1.2f), glm::vec2(j.x1 + 1.2f, j.z0 - 1.2f),
                                   glm::vec2(j.x0 - 1.2f, j.z0 - 1.2f), glm::vec2(j.x1 + 1.2f, j.z1 + 1.2f)}) {
            glPushMatrix();
            glTranslatef(p.x, 0.0f, p.y);
            shapes::cylinder(0.07f, 2.5f, 8, false);
            glPopMatrix();
            geom::block({p.x - 0.2f, 2.5f, p.y - 0.2f}, {p.x + 0.2f, 3.5f, p.y + 0.2f}, 0.0f);
        }
        glEndList();
    }
}

// --------------------------------------------------------------------------
// Drawing
// --------------------------------------------------------------------------

void CityLife::drawSignals(const Environment& env, const Viewer& viewer) const {
    const float glow = 0.55f + 0.45f * env.lampLevel;
    struct Head {
        float dx, dz;  // which way the lamps face
        bool alongX;
    };
    const Head heads[4] = {{-1.0f, 0.0f, true}, {1.0f, 0.0f, true}, {0.0f, -1.0f, false}, {0.0f, 1.0f, false}};
    for (size_t n = 0; n < junctions_.size(); ++n) {
        const Rect& j = junctions_[n];
        // A signal can be told from far off only by its colour: leave out the junctions
        // that are behind the viewer or a long way away.
        const glm::vec3 middle{j.center().x, 3.0f, j.center().y};
        if (glm::length(middle - viewer.eye) > 320.0f || !viewer.frustum.sphere(middle, 24.0f)) continue;
        glCallList(signalPoles_[n]);
        const glm::vec2 corners[4] = {{j.x0 - 1.2f, j.z1 + 1.2f}, {j.x1 + 1.2f, j.z0 - 1.2f},
                                      {j.x0 - 1.2f, j.z0 - 1.2f}, {j.x1 + 1.2f, j.z1 + 1.2f}};
        for (int k = 0; k < 4; ++k) {
            const Light state = light(heads[k].alongX);
            const glm::vec2 face = corners[k] + glm::vec2(heads[k].dx, heads[k].dz) * 0.21f;
            const glm::vec3 lamps[3] = {{0.9f, 0.12f, 0.1f}, {1.0f, 0.65f, 0.1f}, {0.15f, 0.9f, 0.35f}};
            for (int l = 0; l < 3; ++l) {
                const bool on = (l == 0 && state == Light::Red) || (l == 1 && state == Light::Amber) ||
                                (l == 2 && state == Light::Green);
                const glm::vec3 c = on ? lamps[l] : lamps[l] * 0.18f;
                color(c.r, c.g, c.b);
                if (on) emission(c.r * glow, c.g * glow, c.b * glow);
                const float y = 3.3f - 0.3f * static_cast<float>(l);
                const float hx = heads[k].dx != 0.0f ? 0.02f : 0.1f, hz = heads[k].dz != 0.0f ? 0.02f : 0.1f;
                geom::block({face.x - hx, y - 0.1f, face.y - hz}, {face.x + hx, y + 0.1f, face.y + hz}, 0.0f);
                if (on) emission(0.0f, 0.0f, 0.0f);
            }
        }
    }
}

std::vector<std::string> CityLife::statsLines() const {
    int trams = 0;
    for (const Track& track : tracks_) trams += static_cast<int>(track.trams.size());
    return {"life         " + std::to_string(trams) + " trams;  drawn last frame: " + std::to_string(tramsDrawn_) +
            " trams"};
}

void CityLife::draw(const Environment& env, const Viewer& view) const {
    if (!layout_) return;
    tramsDrawn_ = 0;
    drawSignals(env, view);
    const bool lightsOn = env.lampLevel > 0.3f;
    const Rect area = layout_->city.grown(260.0f);
    const glm::vec2 viewer{view.eye.x, view.eye.z};
    const float reach = std::min(900.0f, view.distance);
    for (const Track& track : tracks_) {
        const float yaw = glm::degrees(std::atan2(track.dir.x, track.dir.y));
        for (const Tram& t : track.trams) {
            const glm::vec2 front = track.start + track.dir * t.s;
            if (!area.contains(front.x, front.y)) continue;
            if (glm::length(front - viewer) > reach) continue;
            // A sphere round the middle of the tram, which is 27 m long.
            const glm::vec2 middle = front - track.dir * (kTramLength * 0.5f);
            if (!view.frustum.sphere({middle.x, 1.5f, middle.y}, 15.0f)) continue;
            glPushMatrix();
            glTranslatef(front.x, 0.0f, front.y);
            glRotatef(yaw, 0.0f, 1.0f, 0.0f);
            glCallList(tram_);
            if (lightsOn) glCallList(tramLights_);
            glPopMatrix();
            ++tramsDrawn_;
        }
    }
}

}  // namespace citylab
