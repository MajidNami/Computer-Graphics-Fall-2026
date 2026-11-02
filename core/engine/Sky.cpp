#include "engine/Sky.h"

#include <algorithm>
#include <cmath>

#include "city/Procedural.h"

namespace citylab {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDomeRadius = 1800.0f;

glm::vec3 mix3(const glm::vec3& a, const glm::vec3& b, float t) { return a + (b - a) * std::clamp(t, 0.0f, 1.0f); }

}  // namespace

void Sky::init(GLuint glowTexture) {
    glow_ = glowTexture;
    procedural::Random rng(1405);
    for (int i = 0; i < 1600; ++i) {
        // Uniform on the upper hemisphere.
        const float y = rng.range(0.03f, 1.0f);
        const float a = rng.range(0.0f, 2.0f * kPi);
        const float r = std::sqrt(1.0f - y * y);
        const float brightness = std::pow(rng.uniform(), 3.0f) * 0.85f + 0.15f;
        stars_.push_back({r * std::cos(a), y, r * std::sin(a), brightness});
    }
    // Mountain silhouette: two octaves of smooth noise around the horizon,
    // taller in the north like the Alborz behind Tehran.
    const int samples = 256;
    for (int i = 0; i <= samples; ++i) {
        const float t = static_cast<float>(i) / samples;
        const float a = t * 2.0f * kPi;
        const float n = procedural::fbm(t * 24.0f, 0.5f, 4, 24, 7);
        const float north = 0.5f - 0.5f * std::sin(a);  // z = sin(a); north is -z
        const float height = 30.0f + 170.0f * n * (0.45f + 0.55f * north);
        mountains_.push_back({a, height});
    }
    // Fair-weather clouds: clusters of overlapping puffs, flatter and smaller
    // towards the horizon.
    for (int c = 0; c < 46; ++c) {
        const float azimuth = rng.range(0.0f, 2.0f * kPi);
        const float elevation = 0.10f + 0.55f * std::pow(rng.uniform(), 1.6f);
        const float size = rng.range(70.0f, 150.0f) * (0.55f + elevation);
        const int count = 4 + static_cast<int>(rng.uniform() * 5.0f);
        for (int k = 0; k < count; ++k) {
            const float spread = static_cast<float>(k) - static_cast<float>(count - 1) * 0.5f;
            puffs_.push_back({azimuth + spread * size * 0.55f / (kDomeRadius * std::cos(elevation)) + rng.range(-0.01f, 0.01f),
                              elevation + rng.range(-0.012f, 0.02f) * (1.0f - std::fabs(spread) * 0.2f),
                              size * rng.range(0.7f, 1.15f) * (1.0f - std::fabs(spread) * 0.08f), rng.range(0.85f, 1.0f)});
        }
    }
}

void Sky::draw(const glm::vec3& eye, const Environment& env, double time) const {
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT | GL_POINT_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(eye.x, eye.y, eye.z);  // the sky moves with the viewer
    GLfloat m[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, m);
    glm::mat4 view;
    for (int k = 0; k < 16; ++k) view[k / 4][k % 4] = m[k];

    drawDome(env);
    drawStars(env);
    drawDiscs(view, env);
    drawClouds(view, env, time);
    drawMountains(env);

    glPopMatrix();
    glPopAttrib();
}

void Sky::drawDome(const Environment& env) const {
    // Colour depends on elevation (zenith -> horizon) plus a warm glow in the
    // direction of a low sun.
    const glm::vec3 sunFlat = glm::normalize(glm::vec3(env.sunDirection.x, 0.0f, env.sunDirection.z) + glm::vec3(1e-4f));
    const float lowSun = std::clamp(1.0f - std::fabs(env.sunDirection.y) * 2.2f, 0.0f, 1.0f);
    const int slices = 48, stacks = 14;
    for (int j = 0; j < stacks; ++j) {
        const float e0 = -0.12f + (static_cast<float>(j) / stacks) * (kPi * 0.5f + 0.12f);
        const float e1 = -0.12f + (static_cast<float>(j + 1) / stacks) * (kPi * 0.5f + 0.12f);
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= slices; ++i) {
            const float a = 2.0f * kPi * static_cast<float>(i) / slices;
            for (const float e : {e1, e0}) {
                const glm::vec3 d{std::cos(e) * std::cos(a), std::sin(e), std::cos(e) * std::sin(a)};
                const float up = std::clamp(d.y, 0.0f, 1.0f);
                glm::vec3 c = mix3(env.horizonColor, env.zenithColor, std::pow(up, 0.55f));
                const float toward = std::max(0.0f, glm::dot(glm::normalize(glm::vec3(d.x, 0, d.z)), sunFlat));
                const float glow = std::pow(toward, 6.0f) * lowSun * (1.0f - up) * 0.85f;
                c = mix3(c, env.glowColor, glow);
                if (d.y < 0.0f) c *= 0.82f;  // below the horizon (seen past the mountains)
                glColor3f(c.r, c.g, c.b);
                glVertex3f(d.x * kDomeRadius, d.y * kDomeRadius, d.z * kDomeRadius);
            }
        }
        glEnd();
    }
}

void Sky::drawStars(const Environment& env) const {
    if (env.starAlpha <= 0.01f) return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (const glm::vec4& s : stars_) {
        glColor4f(0.92f, 0.95f, 1.0f, s.w * env.starAlpha);
        glVertex3f(s.x * kDomeRadius * 0.95f, s.y * kDomeRadius * 0.95f, s.z * kDomeRadius * 0.95f);
    }
    glEnd();
    glDisable(GL_BLEND);
}

void Sky::drawDiscs(const glm::mat4& view, const Environment& env) const {
    // Camera-facing quads: take right/up from the view matrix rows.
    const glm::vec3 right{view[0][0], view[1][0], view[2][0]};
    const glm::vec3 up{view[0][1], view[1][1], view[2][1]};
    auto disc = [&](const glm::vec3& dir, float size, const glm::vec4& color) {
        const glm::vec3 c = dir * (kDomeRadius * 0.9f);
        const glm::vec3 r = right * size, u = up * size;
        glColor4f(color.r, color.g, color.b, color.a);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex3f(c.x - r.x - u.x, c.y - r.y - u.y, c.z - r.z - u.z);
        glTexCoord2f(1, 0); glVertex3f(c.x + r.x - u.x, c.y + r.y - u.y, c.z + r.z - u.z);
        glTexCoord2f(1, 1); glVertex3f(c.x + r.x + u.x, c.y + r.y + u.y, c.z + r.z + u.z);
        glTexCoord2f(0, 1); glVertex3f(c.x - r.x + u.x, c.y - r.y + u.y, c.z - r.z + u.z);
        glEnd();
    };
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, glow_);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    if (env.sunDirection.y > -0.1f) {
        const glm::vec3 warm = mix3(glm::vec3(1.0f, 0.55f, 0.25f), glm::vec3(1.0f, 0.95f, 0.85f),
                                    env.sunDirection.y * 3.0f);
        disc(env.sunDirection, 260.0f, glm::vec4(warm, 0.35f));  // halo
        disc(env.sunDirection, 60.0f, glm::vec4(1.0f, 0.98f, 0.92f, 1.0f));
        disc(env.sunDirection, 44.0f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    const glm::vec3 moon = glm::normalize(-env.sunDirection + glm::vec3(0.0f, 0.35f, 0.0f));
    if (moon.y > 0.0f && env.daylight < 0.6f) {
        const float a = 1.0f - env.daylight / 0.6f;
        disc(moon, 120.0f, glm::vec4(0.6f, 0.7f, 0.9f, 0.18f * a));
        disc(moon, 34.0f, glm::vec4(0.95f, 0.95f, 0.88f, a));
    }
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void Sky::drawClouds(const glm::mat4& view, const Environment& env, double time) const {
    // Camera-facing soft discs on the dome. The wind turns the whole layer
    // slowly about the vertical axis.
    const glm::vec3 right{view[0][0], view[1][0], view[2][0]};
    const glm::vec3 up{view[0][1], view[1][1], view[2][1]};
    const float drift = static_cast<float>(std::fmod(time * 0.0025, 2.0 * kPi));
    // White by day, tinted by the sunset glow, blue-grey and faint at night.
    const glm::vec3 lit = mix3(mix3(glm::vec3(0.16f, 0.18f, 0.26f), env.glowColor, env.daylight * 1.6f),
                               glm::vec3(1.0f, 1.0f, 1.0f), (env.daylight - 0.45f) * 2.0f);
    const float alpha = 0.25f + 0.55f * env.daylight;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, glow_);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    for (const Puff& p : puffs_) {
        const float a = p.azimuth + drift;
        const glm::vec3 c = glm::vec3(std::cos(p.elevation) * std::cos(a), std::sin(p.elevation),
                                      std::cos(p.elevation) * std::sin(a)) * (kDomeRadius * 0.85f);
        // Flatten towards the horizon: a cloud seen from below is wide and thin.
        const glm::vec3 r = right * p.size, u = up * (p.size * (0.45f + 0.55f * std::sin(p.elevation)));
        const glm::vec3 shade = lit * (0.82f + 0.18f * p.shade);
        // The underside is a little darker than the top.
        glColor4f(shade.r * 0.9f, shade.g * 0.9f, shade.b * 0.93f, alpha);
        glTexCoord2f(0, 0); glVertex3f(c.x - r.x - u.x, c.y - r.y - u.y, c.z - r.z - u.z);
        glTexCoord2f(1, 0); glVertex3f(c.x + r.x - u.x, c.y + r.y - u.y, c.z + r.z - u.z);
        glColor4f(shade.r, shade.g, shade.b, alpha);
        glTexCoord2f(1, 1); glVertex3f(c.x + r.x + u.x, c.y + r.y + u.y, c.z + r.z + u.z);
        glTexCoord2f(0, 1); glVertex3f(c.x - r.x + u.x, c.y - r.y + u.y, c.z - r.z + u.z);
    }
    glEnd();
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void Sky::drawMountains(const Environment& env) const {
    // Far mountains take on the horizon colour (aerial perspective); the
    // nearer, lower ridge is a little darker.
    const glm::vec3 rock = mix3(glm::vec3(0.30f, 0.34f, 0.40f), glm::vec3(0.06f, 0.07f, 0.10f), 1.0f - env.daylight);
    for (int layer = 0; layer < 2; ++layer) {
        const float radius = layer == 0 ? 1600.0f : 1300.0f;
        const float heightScale = layer == 0 ? 1.0f : 0.55f;
        const float haze = layer == 0 ? 0.62f : 0.42f;
        const glm::vec3 top = mix3(rock, env.horizonColor, haze);
        const glm::vec3 base = mix3(rock, env.horizonColor, haze + 0.25f);
        glBegin(GL_QUAD_STRIP);
        for (size_t i = 0; i < mountains_.size(); ++i) {
            const glm::vec2& m = mountains_[(i + static_cast<size_t>(layer) * 70) % mountains_.size()];
            const float a = mountains_[i].x;
            const float x = std::cos(a) * radius, z = std::sin(a) * radius;
            glColor3f(top.r, top.g, top.b);
            glVertex3f(x, m.y * heightScale - 10.0f, z);
            glColor3f(base.r, base.g, base.b);
            glVertex3f(x, -60.0f, z);
        }
        glEnd();
    }
}

}  // namespace citylab
