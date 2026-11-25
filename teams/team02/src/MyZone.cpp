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

    
    // =========================================
    // SCIENCE MUSEUM - ATOM LANDMARK
    // =========================================

    // Draw one orbital ring and its electron.
    void drawOrbit(float azimuth, float electronAngle) const {

        const float orbitRadius = 4.5f;

        glPushMatrix();

        // Rotate the orbital plane around the Y axis.
        glRotatef(azimuth, 0.0f, 1.0f, 0.0f);

        // Tilt the orbital plane.
        glRotatef(35.264f, 1.0f, 0.0f, 0.0f);

        // Silver orbital ring.
        glColor3f(0.78f, 0.81f, 0.85f);
        shapes::torus(orbitRadius, 0.07f, 64, 10);

        // Place the electron at a different position
        // on each orbital ring.
        glPushMatrix();

        glRotatef(electronAngle, 0.0f, 1.0f, 0.0f);
        glTranslatef(orbitRadius, 0.0f, 0.0f);

        glColor3f(0.58f, 0.62f, 0.68f);
        shapes::sphere(0.32f, 24, 16);

        glPopMatrix();

        glPopMatrix();
    }


    // Build the complete atom sculpture.
    void drawAtomLandmark() const {

        glPushMatrix();

        // Position on the plot.
        glTranslatef(-12.0f, 0.0f, 31.0f);


        // -------------------------------------
        // 1. Metallic pedestal
        // -------------------------------------

        glColor3f(0.32f, 0.35f, 0.39f);

        // glPushMatrix();
        glTranslatef(0.0f, 0.25f, 0.0f);
        shapes::box(3.2f, 0.5f, 3.2f);


        // -------------------------------------
        // 2. Central support
        // -------------------------------------

        glColor3f(0.76f, 0.79f, 0.83f);

        // glPushMatrix();
        glTranslatef(0.0f, 0.25f, 0.0f);
        shapes::cylinder(0.22f, 7.0f, 32);


        // -------------------------------------
        // 3. Central nucleus
        // -------------------------------------

        glColor3f(0.83f, 0.85f, 0.88f);

        // glPushMatrix();
        glTranslatef(0.0f, 7.0f, 0.0f);
        shapes::sphere(1.15f, 32, 24);


        // -------------------------------------
        // 4. Three symmetrical orbital rings
        // -------------------------------------

        // The three orbital planes are separated
        // by equal 60-degree angles.

        drawOrbit(0.0f,   0.0f);
        drawOrbit(120.0f, 120.0f);
        drawOrbit(240.0f, 240.0f);


        glPopMatrix();
    }




    GLuint scenery_ = 0;
};

}  // namespace team02

CITYLAB_REGISTER_ZONE(team02::MyZone);
