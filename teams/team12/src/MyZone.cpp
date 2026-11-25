#include <citylab/CityLab.h>
#include <cmath>

#include "Architecture.h"
#include "Facilities.h"
#include "Geometry.h"
#include "Landscape.h"
#include "Pitch.h"
#include "Roof.h"
#include "Seating.h"

namespace team12 {
namespace {

void drawFloodlight(glm::vec3 position, float nightLight) {
    drawBox(position + glm::vec3{0.0f, 1.30f, 0.0f}, {0.18f, 2.6f, 0.20f}, palette::steel);
    const ScopedMatrix fixture;
    glTranslatef(position.x, position.y, position.z);
    glRotatef(glm::degrees(std::atan2(position.z, position.y)), 1.0f, 0.0f, 0.0f);
    drawBox({}, {1.8f, 0.28f, 0.85f}, palette::steel);
    glPushAttrib(GL_LIGHTING_BIT);
    const GLfloat emission[] = {nightLight * 0.88f, nightLight * 0.94f, nightLight, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emission);
    drawBox({0.0f, -0.15f, 0.0f}, {1.62f, 0.04f, 0.68f}, {0.77f, 0.82f, 0.86f});
    glPopAttrib();
}

void enablePitchSpotlight(glm::vec3 position, float nightLight) {
    const GLfloat location[] = {position.x, position.y, position.z, 1.0f};
    const auto aim = glm::normalize(-position);
    const GLfloat direction[] = {aim.x, aim.y, aim.z};
    const GLfloat color[] = {1.55f * nightLight, 1.55f * nightLight, 1.48f * nightLight, 1.0f};
    const GLfloat ambient[] = {0.09f * nightLight, 0.09f * nightLight, 0.09f * nightLight, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, location);
    glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, direction);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, color);
    glLightfv(GL_LIGHT1, GL_AMBIENT, ambient);
    glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 70.0f);
    glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, 0.65f);
    glEnable(GL_LIGHT1);
}

}  // namespace

class StadiumZone final : public citylab::Zone {
public:
    void init(citylab::Context& ctx) override {
        layout_ = makeStadiumLayout(ctx.plot());
        pitch_ = makePitch(layout_);
        shell_ = makeStadiumShell(layout_, ctx);
        roof_ = makeStadiumRoof(layout_);
        bowl_ = makeSeatingBowl(layout_, ctx);
        grounds_ = makeGrounds(layout_, ctx);
        floodlight_ = {0.0f, layout_.roofHeight - 3.0f, layout_.innerSize.y * 0.45f};
    }

    void draw(const citylab::Context& ctx) const override {
        drawLandscape(layout_, grounds_);
        drawFacilities(layout_);

        const ScopedMatrix stadiumFrame;
        const auto& center = layout_.stadiumCenter;
        glTranslatef(center.x, center.y, center.z);
        glRotatef(layout_.stadiumYawDegrees, 0.0f, 1.0f, 0.0f);
        const float nightLight = 1.0f - glm::smoothstep(0.04f, 0.26f, ctx.frame().daylight);
        drawArchitecture(shell_, nightLight);
        drawStadiumRoof(roof_, nightLight);
        drawSeatingBowl(bowl_);
        drawFloodlight(floodlight_, nightLight);
        // Pitch is last, so the floodlight affects the field and goals only.
        // CityLab restores the light state when it exits the zone.
        if (nightLight > 0.0f) {
            enablePitchSpotlight(floodlight_, nightLight);
        }
        drawPitch(layout_, pitch_);
    }

private:
    StadiumLayout layout_{};
    PitchScene pitch_;
    StadiumShell shell_;
    StadiumRoof roof_;
    SeatingBowl bowl_;
    StadiumGrounds grounds_;
    glm::vec3 floodlight_{};
};

}  // namespace team12

CITYLAB_REGISTER_ZONE(team12::StadiumZone);
