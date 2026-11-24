// team02 - Science Museum
//
// Your zone. Start here and make it yours; the comments in sdk/cpp/citylab/
// (Zone.h, Context.h, Shapes.h, Props.h) explain every part of the API.
// Coordinates are plot-local metres:
// x from -40 to 40 across the frontage (-80 to 80 on a double plot), z from
// -50 (back) to +50 (the gate on the street), y up from 0 to 40.
//
//     python3 tools/citylab.py run team02      build and walk straight to your plot
//     python3 tools/citylab.py check team02    the same checks the graders run

#include <citylab/CityLab.h>

namespace team02 {

using namespace citylab;

class MyZone : public Zone {
public:
    void init(Context& ctx) override {
        // Everything that never moves goes into a display list, built once.
        scenery_ = glGenLists(1);
        glNewList(scenery_, GL_COMPILE);
        drawScenery(ctx.plot());
        glEndList();

        // What the visitor cannot walk through.
        // Collision boundary around the atom sculpture.
        ctx.addCollider(
            {-16.0f, 0.0f, 27.0f},
            {-8.0f, 8.0f, 35.0f}
        );
        ctx.log("hello from " + ctx.teamId() + " on plot " + ctx.plot().id);
    }

    void update(const Frame&) override {}

    void reset() override {}

    void onKey(int) override {}

    void draw(const Context&) const override {
        glCallList(scenery_);
    }


private:
    // Draw one rectangular paved area.
    void drawPaving(float x, float z,
                    float width, float depth) const {
        glPushMatrix();
        glTranslatef(x, 0.03f, z);
        shapes::plane(width, depth);
        glPopMatrix();
    }

    void drawScenery(const PlotInfo& plot) const {

        // ---------------------------------
        // 1. Grass covering the entire plot
        // ---------------------------------

        glColor3f(0.36f, 0.52f, 0.29f);

        glPushMatrix();
        glTranslatef(0.0f, 0.01f, 0.0f);
        shapes::plane(plot.width, plot.depth);
        glPopMatrix();


        // ---------------------------------
        // 2. Main pedestrian walkway
        // Entrance -> Science Museum
        // ---------------------------------

        glColor3f(0.72f, 0.70f, 0.66f);

        drawPaving(0.0f, 16.0f, 5.0f, 60.0f);


        // ---------------------------------
        // 3. Entrance plaza
        // ---------------------------------

        glColor3f(0.80f, 0.78f, 0.73f);

        drawPaving(0.0f, 45.0f, 12.0f, 8.0f);


        // ---------------------------------
        // 4. Central cross walkway
        // Connects both side buildings
        // ---------------------------------

        glColor3f(0.72f, 0.70f, 0.66f);

        drawPaving(0.0f, 15.0f, 50.0f, 4.0f);


        // ---------------------------------
        // 5. Planetarium connector
        // ---------------------------------

        drawPaving(-25.0f, 11.0f, 4.0f, 7.0f);


        // ---------------------------------
        // 6. Cafe connector
        // ---------------------------------

        drawPaving(24.0f, 11.0f, 4.0f, 7.0f);


        // ---------------------------------
        // 7. Museum entrance plaza
        // ---------------------------------

        glColor3f(0.80f, 0.78f, 0.73f);

        drawPaving(0.0f, -10.5f, 18.0f, 7.0f);


        // ---------------------------------
        // 8. Atom sculpture plaza
        // ---------------------------------

        drawPaving(-12.0f, 31.0f, 14.0f, 14.0f);
        // Build the Science Museum atom landmark.
        drawAtomLandmark();
    }


    // =========================================
    // SCIENCE MUSEUM - ATOM LANDMARK
    // =========================================

    void drawAtomLandmark() const {

        glPushMatrix();

        // Position the sculpture in the entrance plaza.
        glTranslatef(-12.0f, 0.0f, 31.0f);


        // -------------------------------------
        // 1. Square pedestal
        // -------------------------------------

        glColor3f(0.30f, 0.34f, 0.40f);

        glPushMatrix();
        glTranslatef(0.0f, 0.25f, 0.0f);
        shapes::box(2.2f, 0.5f, 2.2f);
        glPopMatrix();


        // -------------------------------------
        // 2. Central support column
        // -------------------------------------

        glColor3f(0.70f, 0.74f, 0.80f);

        glPushMatrix();
        glTranslatef(0.0f, 0.5f, 0.0f);
        shapes::cylinder(0.18f, 3.8f, 20);
        glPopMatrix();


        // All orbital components share this height.
        const float centerY = 4.4f;
        const float orbitRadius = 3.3f;


        // -------------------------------------
        // 3. Central nucleus
        // -------------------------------------

        glColor3f(0.10f, 0.38f, 0.85f);

        glPushMatrix();
        glTranslatef(0.0f, centerY, 0.0f);
        shapes::sphere(0.9f, 32, 20);
        glPopMatrix();


        // -------------------------------------
        // 4. First orbital ring
        // -------------------------------------

        glPushMatrix();

        glTranslatef(0.0f, centerY, 0.0f);
        glRotatef(55.0f, 1.0f, 0.0f, 0.0f);

        glColor3f(0.85f, 0.88f, 0.93f);
        shapes::torus(orbitRadius, 0.13f, 48, 12);

        // Electron on the first orbit.
        glPushMatrix();
        glTranslatef(orbitRadius, 0.0f, 0.0f);
        glColor3f(0.95f, 0.65f, 0.12f);
        shapes::sphere(0.38f, 20, 14);
        glPopMatrix();

        glPopMatrix();


        // -------------------------------------
        // 5. Second orbital ring
        // -------------------------------------

        glPushMatrix();

        glTranslatef(0.0f, centerY, 0.0f);
        glRotatef(55.0f, 0.0f, 0.0f, 1.0f);
        glRotatef(35.0f, 0.0f, 1.0f, 0.0f);

        glColor3f(0.20f, 0.72f, 0.85f);
        shapes::torus(orbitRadius, 0.13f, 48, 12);

        // Electron on the second orbit.
        glPushMatrix();
        glTranslatef(orbitRadius, 0.0f, 0.0f);
        glColor3f(0.95f, 0.65f, 0.12f);
        shapes::sphere(0.38f, 20, 14);
        glPopMatrix();

        glPopMatrix();


        // -------------------------------------
        // 6. Third orbital ring
        // -------------------------------------

        glPushMatrix();

        glTranslatef(0.0f, centerY, 0.0f);
        glRotatef(65.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(65.0f, 0.0f, 1.0f, 0.0f);

        glColor3f(0.55f, 0.68f, 0.95f);
        shapes::torus(orbitRadius, 0.13f, 48, 12);

        // Electron on the third orbit.
        glPushMatrix();
        glTranslatef(orbitRadius, 0.0f, 0.0f);
        glColor3f(0.95f, 0.65f, 0.12f);
        shapes::sphere(0.38f, 20, 14);
        glPopMatrix();

        glPopMatrix();


        glPopMatrix();
    }



    GLuint scenery_ = 0;
};

}  // namespace team02

CITYLAB_REGISTER_ZONE(team02::MyZone);
