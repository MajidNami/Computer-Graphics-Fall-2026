// __TEAM__ - __TITLE_EN__
//
// Your zone. Start here and make it yours; the comments in sdk/cpp/citylab/
// (Zone.h, Context.h, Shapes.h, Props.h) explain every part of the API.
// Coordinates are plot-local metres:
// x from -40 to 40 across the frontage (-80 to 80 on a double plot), z from
// -50 (back) to +50 (the gate on the street), y up from 0 to 40.
//
//     python3 tools/citylab.py run __TEAM__      build and walk straight to your plot
//     python3 tools/citylab.py check __TEAM__    the same checks the graders run

#include <citylab/CityLab.h>

namespace __TEAM__ {

using namespace citylab;

class MyZone : public Zone {
public:
    void init(Context& ctx) override {
        // Phase 1: build everything that never moves into a display list.
        scenery_ = glGenLists(1);
        glNewList(scenery_, GL_COMPILE);
        drawScenery(ctx.plot());
        glEndList();

        // Phase 3: what the visitor cannot walk through.
        ctx.addCollider({-5.0f, 0.0f, -14.0f}, {5.0f, 4.0f, -6.0f});
        ctx.log("hello from " + ctx.teamId() + " on plot " + ctx.plot().id);
    }

    void update(const Frame& frame) override {
        // Phase 2: move things with frame.dt so the speed is the same on every computer.
        if (spinning_) angle_ += 45.0f * frame.dt;  // degrees per second
    }

    void draw(const Context&) const override {
        glCallList(scenery_);

        // A weather vane on the roof that turns.
        glPushMatrix();
        glTranslatef(0.0f, 6.6f, -10.0f);
        glRotatef(angle_, 0.0f, 1.0f, 0.0f);
        glColor3f(0.85f, 0.70f, 0.25f);
        shapes::box(0.1f, 0.1f, 1.6f);
        glTranslatef(0.0f, 0.0f, 0.8f);
        shapes::cone(0.2f, 0.5f, 12);
        glPopMatrix();
    }

    void reset() override { angle_ = 0.0f; }

    void onKey(int key) override {
        if (key == 'v') spinning_ = !spinning_;  // V stops and starts the vane
    }

private:
    void drawScenery(const PlotInfo& plot) const {
        glColor3f(0.36f, 0.50f, 0.24f);  // lawn over the whole plot
        glPushMatrix();
        glTranslatef(0.0f, 0.01f, 0.0f);
        shapes::plane(plot.width, plot.depth);
        glPopMatrix();

        glColor3f(0.70f, 0.64f, 0.55f);  // a path from the house to the gate
        glPushMatrix();
        glTranslatef(0.0f, 0.02f, 17.0f);
        shapes::plane(3.0f, 46.0f);
        glPopMatrix();

        glColor3f(0.92f, 0.85f, 0.70f);  // a small house: walls and a roof
        glPushMatrix();
        glTranslatef(0.0f, 2.0f, -10.0f);
        shapes::box(10.0f, 4.0f, 8.0f);
        glPopMatrix();
        glColor3f(0.60f, 0.25f, 0.18f);
        glPushMatrix();
        glTranslatef(0.0f, 4.0f, -10.0f);
        glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
        shapes::gableRoof(8.6f, 2.4f, 10.6f);
        glPopMatrix();
    }

    GLuint scenery_ = 0;
    float angle_ = 0.0f;
    bool spinning_ = true;
};

}  // namespace __TEAM__

CITYLAB_REGISTER_ZONE(__TEAM__::MyZone);
