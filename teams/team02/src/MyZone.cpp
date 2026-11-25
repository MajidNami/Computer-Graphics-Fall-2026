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
#include <algorithm>
#include <cmath>

namespace team02 {

using namespace citylab;

namespace {

// ---------------------------------------------------------------------------
// Planetarium constants (plot-local metres)
// ---------------------------------------------------------------------------
constexpr float kPi        = 3.14159265f;
constexpr float kDegToRad  = kPi / 180.0f;
constexpr float kGoldenAngle = 2.39996f;

// Placement of the planetarium centre on the plot.
constexpr float kPlanCx = -25.0f;
constexpr float kPlanCz = 0.0f;

// Wall ring: 12 bays of 30 degrees, bay 0 (facing +z) is the entrance.
constexpr int   kBayCount      = 12;
constexpr float kBayStepDeg    = 360.0f / kBayCount;
constexpr float kBayWidth      = 4.1f;    // = 2 * R * tan(15 deg) for R = 7.65
constexpr float kWallRadius    = 7.65f;   // centre line of the wall ring
constexpr float kWallThickness = 0.65f;
constexpr float kWallHeight    = 3.15f;
constexpr float kFloorY        = 0.35f;   // top of the foundation slab
constexpr float kWallTop       = kFloorY + kWallHeight;  // 3.5 m

// Dome: a hemisphere sitting on top of the wall ring.
constexpr float kDomeRadius      = 8.0f;
constexpr int   kDomeRings       = 12;
constexpr int   kDomeSegments    = 48;
constexpr int   kGlassEvery      = 6;     // every 6th meridian is a glass band
constexpr float kInnerDomeRadius = 7.58f; // = inner wall corner radius
constexpr int   kInnerRings      = 10;
constexpr int   kInnerSegments   = 36;

// Interior.
constexpr float kInteriorFloorRadius = 7.55f;
constexpr float kInteriorFloorLift   = 0.02f;  // above the slab (no z-fighting)

// Entrance.
constexpr float kDoorHalfWidth = 1.8f;
constexpr float kDoorLeafWidth = 1.8f;
constexpr float kDoorLeafHeight = 2.8f;
constexpr float kDoorOpenDeg   = 75.0f;

// Seating: concentric arcs around the projector, aisle left open at +z.
struct SeatRow {
    float radius;
    int   count;
};
constexpr SeatRow kSeatRows[] = {{3.4f, 12}, {4.8f, 17}, {6.2f, 22}};
constexpr float kSeatArcStartDeg = 40.0f;
constexpr float kSeatArcEndDeg   = 320.0f;

// Heights of the chrome rings that hug the dome (metres above the dome base).
constexpr float kDomeRingHeights[] = {2.0f, 4.0f, 6.0f};

// A point on a hemisphere, in the hemisphere's own frame.
struct DomePoint {
    float x, y, z;
};

}  // namespace

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

        addPlanetariumColliders(ctx);

        ctx.log("hello from " + ctx.teamId() + " on plot " + ctx.plot().id);
    }

    void update(const Frame&) override {}

    void reset() override {}

    void onKey(int) override {}

    void draw(const Context&) const override {
        glCallList(scenery_);
    }


private:
    // =========================================
    // COLLIDERS
    // =========================================

    // Build colliders from the same data the planetarium is drawn with, so
    // the walls you see are the walls you bump into. Each bay is split into
    // three short pieces; the doorway stays open.
    static void addPlanetariumColliders(Context& ctx) {
        for (int bay = 1; bay < kBayCount; ++bay) {
            const float angle = bay * kBayStepDeg * kDegToRad;
            const float s = std::sin(angle);
            const float c = std::cos(angle);

            for (int piece = -1; piece <= 1; ++piece) {
                // Offset along the wall tangent (c, -s).
                const float along = piece * kBayWidth / 3.0f;
                const float cx = kPlanCx + kWallRadius * s + along * c;
                const float cz = kPlanCz + kWallRadius * c - along * s;

                // Half extents of the axis-aligned box around the piece.
                const float hx = std::fabs(c) * kBayWidth / 6.0f
                               + std::fabs(s) * kWallThickness * 0.5f;
                const float hz = std::fabs(s) * kBayWidth / 6.0f
                               + std::fabs(c) * kWallThickness * 0.5f;

                ctx.addCollider({cx - hx, 0.0f, cz - hz},
                                {cx + hx, kWallTop, cz + hz});
            }
        }

        // Door pillars.
        for (int side = -1; side <= 1; side += 2) {
            const float px = kPlanCx + side * (kDoorHalfWidth + 0.2f);
            ctx.addCollider({px - 0.2f, 0.0f, kPlanCz + kWallRadius - 0.4f},
                            {px + 0.2f, kWallTop, kPlanCz + kWallRadius + 0.4f});
        }

        // Star projector in the middle of the room.
        ctx.addCollider({kPlanCx - 0.9f, 0.0f, kPlanCz - 0.9f},
                        {kPlanCx + 0.9f, 2.7f, kPlanCz + 0.9f});
    }


    // =========================================
    // SCENERY (plot level)
    // =========================================

    // Draw one rectangular paved area.
    void drawPaving(float x, float z,
                    float width, float depth) const {
        glPushMatrix();
        glTranslatef(x, 0.03f, z);
        shapes::plane(width, depth);
        glPopMatrix();
    }

    void drawScenery(const PlotInfo& plot) const {

        // 1. Grass covering the entire plot
        glColor3f(0.36f, 0.52f, 0.29f);

        glPushMatrix();
        glTranslatef(0.0f, 0.01f, 0.0f);
        shapes::plane(plot.width, plot.depth);
        glPopMatrix();

        // 2. Main pedestrian walkway: entrance -> Science Museum
        glColor3f(0.72f, 0.70f, 0.66f);
        drawPaving(0.0f, 16.0f, 5.0f, 60.0f);

        // 3. Entrance plaza
        glColor3f(0.80f, 0.78f, 0.73f);
        drawPaving(0.0f, 45.0f, 12.0f, 8.0f);

        // 4. Central cross walkway: connects both side buildings
        glColor3f(0.72f, 0.70f, 0.66f);
        drawPaving(0.0f, 15.0f, 50.0f, 4.0f);

        // 5. Planetarium connector
        drawPaving(-25.0f, 11.0f, 4.0f, 7.0f);

        // 6. Cafe connector
        drawPaving(24.0f, 11.0f, 4.0f, 7.0f);

        // 7. Museum entrance plaza
        glColor3f(0.80f, 0.78f, 0.73f);
        drawPaving(0.0f, -10.5f, 18.0f, 7.0f);

        // 8. Atom sculpture plaza
        drawPaving(-12.0f, 31.0f, 14.0f, 14.0f);

        drawAtomLandmark();
        drawPlanetarium();
    }


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

        // Electron at a different position on each ring.
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

        // 1. Metallic pedestal
        glColor3f(0.32f, 0.35f, 0.39f);
        glTranslatef(0.0f, 0.25f, 0.0f);
        shapes::box(3.2f, 0.5f, 3.2f);

        // 2. Central support
        glColor3f(0.76f, 0.79f, 0.83f);
        glTranslatef(0.0f, 0.25f, 0.0f);
        shapes::cylinder(0.22f, 7.0f, 32);

        // 3. Central nucleus
        glColor3f(0.83f, 0.85f, 0.88f);
        glTranslatef(0.0f, 7.0f, 0.0f);
        shapes::sphere(1.15f, 32, 24);

        // 4. Three orbital rings, 120 degrees apart.
        drawOrbit(0.0f,   0.0f);
        drawOrbit(120.0f, 120.0f);
        drawOrbit(240.0f, 240.0f);

        glPopMatrix();
    }


    // =========================================
    // SCIENCE MUSEUM - PLANETARIUM
    //
    // Hierarchy (part 6):
    //   level 1  planetarium frame        T(plot position)
    //   level 2  wall bay                 Ry(bay angle) * T(0, y, R)
    //   level 3  panel, 2 windows, fin, trim  (5 parts x 11 bays)
    // Move or rotate only the level-1 transform and everything follows.
    // =========================================

    void drawPlanetarium() const {

        // Non-uniformly scaled boxes are used below; keep normals unit length.
        glEnable(GL_NORMALIZE);

        glPushMatrix();
        glTranslatef(kPlanCx, 0.0f, kPlanCz);

        drawFoundation();
        drawWallRing();
        drawEntrance();
        drawDome();
        drawInterior();
        drawGlowAccents();

        glPopMatrix();
    }


    // ---------------------------------------------------------
    // Foundation: three stacked discs form a stepped plinth.
    // ---------------------------------------------------------
    void drawFoundation() const {
        glColor3f(0.42f, 0.44f, 0.48f);
        shapes::cylinder(9.4f, 0.10f, 64);

        glColor3f(0.36f, 0.38f, 0.42f);
        shapes::cylinder(8.9f, 0.22f, 64);

        glColor3f(0.30f, 0.33f, 0.37f);
        shapes::cylinder(8.4f, kFloorY, 64);
    }


    // ---------------------------------------------------------
    // Wall ring: one bay per 30 degrees, entrance bay skipped.
    // ---------------------------------------------------------
    void drawWallRing() const {
        for (int bay = 1; bay < kBayCount; ++bay) {
            glPushMatrix();
            glRotatef(bay * kBayStepDeg, 0.0f, 1.0f, 0.0f);
            glTranslatef(0.0f, kFloorY + kWallHeight * 0.5f, kWallRadius);
            drawWallBay(bay);
            glPopMatrix();
        }
    }

    // One wall bay in its own frame: origin at the centre of the panel,
    // +z pointing out of the building, +x along the wall.
    void drawWallBay(int bay) const {

        // Chrome panel (unit box, non-uniform scale).
        const float tone = (bay % 2 == 0) ? 0.72f : 0.66f;
        glColor3f(tone, tone + 0.03f, tone + 0.07f);
        glPushMatrix();
        glScalef(kBayWidth, kWallHeight, kWallThickness);
        shapes::box(1.0f, 1.0f, 1.0f);
        glPopMatrix();

        // Two tall blue glass windows on the outer face.
        glColor3f(0.08f, 0.26f, 0.52f);
        for (int side = -1; side <= 1; side += 2) {
            glPushMatrix();
            glTranslatef(side * 0.85f, 0.0f, kWallThickness * 0.5f + 0.01f);
            glScalef(0.8f, 2.4f, 0.04f);
            shapes::box(1.0f, 1.0f, 1.0f);
            glPopMatrix();
        }

        // Bright vertical fin on the +x edge; hides the notch between bays.
        glColor3f(0.88f, 0.90f, 0.94f);
        glPushMatrix();
        glTranslatef(kBayWidth * 0.5f, 0.0f, 0.0f);
        shapes::box(0.26f, kWallHeight, kWallThickness + 0.16f);
        glPopMatrix();

        // Horizontal trim band under the dome.
        glColor3f(0.20f, 0.23f, 0.29f);
        glPushMatrix();
        glTranslatef(0.0f, kWallHeight * 0.5f - 0.08f,
                     kWallThickness * 0.5f + 0.04f);
        shapes::box(kBayWidth, 0.14f, 0.08f);
        glPopMatrix();
    }


    // ---------------------------------------------------------
    // Entrance: pillars, lintel, open glass doors, wedge canopy.
    // ---------------------------------------------------------
    void drawEntrance() const {

        const float pillarX = kDoorHalfWidth + 0.2f;

        // Pillars and lintel.
        glColor3f(0.88f, 0.90f, 0.94f);
        for (int side = -1; side <= 1; side += 2) {
            glPushMatrix();
            glTranslatef(side * pillarX, kFloorY + kWallHeight * 0.5f, kWallRadius);
            shapes::box(0.4f, kWallHeight, 0.8f);
            glPopMatrix();
        }

        glPushMatrix();
        glTranslatef(0.0f, kWallTop - 0.175f, kWallRadius);
        shapes::box(2.0f * pillarX + 0.4f, 0.35f, 0.8f);
        glPopMatrix();

        // Two glass door leaves, both swung open outwards.
        drawDoorLeaf(-1.0f);
        drawDoorLeaf(+1.0f);

        // Cantilevered canopy: hand-made wedge, stretched by a
        // non-uniform scale (width 4.8, thickness 0.5, depth 2.6).
        const float canopyDepth = 2.6f;
        const float canopyFrontZ = kWallRadius + 0.4f + canopyDepth;
        glColor3f(0.78f, 0.82f, 0.88f);
        glPushMatrix();
        glTranslatef(0.0f, kWallTop - 0.40f, kWallRadius + 0.4f + canopyDepth * 0.5f);
        glScalef(4.8f, 0.5f, canopyDepth);
        drawWedge();
        glPopMatrix();

        // Two slim supports under the front corners (they reach the ground).
        glColor3f(0.82f, 0.85f, 0.90f);
        for (int side = -1; side <= 1; side += 2) {
            glPushMatrix();
            glTranslatef(side * 2.3f, 0.03f, canopyFrontZ - 0.1f);
            shapes::cylinder(0.07f, kWallTop - 0.40f - 0.03f, 12);
            glPopMatrix();
        }
    }

    // One door leaf. side = -1 for the left leaf, +1 for the right leaf.
    // The leaf is modelled closed and rotated about its hinge edge.
    void drawDoorLeaf(float side) const {

        // Hinge point p in the leaf's own frame: its outer vertical edge.
        const float hingeX = side * kDoorLeafWidth * 0.5f;

        glPushMatrix();

        // Place the closed leaf in the doorway.
        glTranslatef(side * (kDoorHalfWidth - kDoorLeafWidth * 0.5f),
                     kFloorY + kDoorLeafHeight * 0.5f,
                     kWallRadius);

        glTranslatef(hingeX, 0.0f, 0.0f);                  // T(p)
        glRotatef(side * kDoorOpenDeg, 0.0f, 1.0f, 0.0f);  // R
        glTranslatef(-hingeX, 0.0f, 0.0f);                 // T(-p)

        // Glass pane.
        glColor3f(0.45f, 0.72f, 0.92f);
        shapes::box(kDoorLeafWidth, kDoorLeafHeight, 0.06f);

        // Chrome bars on the hinge edge and on the free edge.
        glColor3f(0.80f, 0.83f, 0.88f);
        glPushMatrix();
        glTranslatef(hingeX, 0.0f, 0.0f);
        shapes::box(0.08f, kDoorLeafHeight, 0.10f);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(-hingeX, 0.0f, 0.0f);
        shapes::box(0.08f, kDoorLeafHeight, 0.10f);
        glPopMatrix();

        glPopMatrix();
    }


    // ---------------------------------------------------------
    // HAND-MADE SHAPE 1: wedge (right triangular prism, 5 faces).
    // Unit size, centred on x/z, base at y = 0, sloping down towards +z.
    // ---------------------------------------------------------
    void drawWedge() const {
        // Bottom corners a b c d, top-back edge e f.
        const float a[3] = {-0.5f, 0.0f, -0.5f};
        const float b[3] = { 0.5f, 0.0f, -0.5f};
        const float c[3] = { 0.5f, 0.0f,  0.5f};
        const float d[3] = {-0.5f, 0.0f,  0.5f};
        const float e[3] = {-0.5f, 1.0f, -0.5f};
        const float f[3] = { 0.5f, 1.0f, -0.5f};

        glBegin(GL_QUADS);
        // Bottom (faces down).
        glNormal3f(0.0f, -1.0f, 0.0f);
        glVertex3fv(a); glVertex3fv(b); glVertex3fv(c); glVertex3fv(d);
        // Back (faces -z).
        glNormal3f(0.0f, 0.0f, -1.0f);
        glVertex3fv(a); glVertex3fv(e); glVertex3fv(f); glVertex3fv(b);
        // Slope (faces up and +z).
        glNormal3f(0.0f, 0.7071f, 0.7071f);
        glVertex3fv(e); glVertex3fv(d); glVertex3fv(c); glVertex3fv(f);
        glEnd();

        glBegin(GL_TRIANGLES);
        // Right side (faces +x).
        glNormal3f(1.0f, 0.0f, 0.0f);
        glVertex3fv(b); glVertex3fv(f); glVertex3fv(c);
        // Left side (faces -x).
        glNormal3f(-1.0f, 0.0f, 0.0f);
        glVertex3fv(a); glVertex3fv(d); glVertex3fv(e);
        glEnd();
    }


    // ---------------------------------------------------------
    // HAND-MADE SHAPE 2: hemispherical dome shell.
    // Points come from the rotation of a quarter-circle profile about y.
    // ---------------------------------------------------------

    // Point on the hemisphere: ring 0 is the apex, ring == rings the base.
    static DomePoint domePoint(float radius, int ring, int rings,
                               int seg, int segs) {
        const float phi   = 0.5f * kPi * static_cast<float>(ring) / rings;
        const float theta = 2.0f * kPi * static_cast<float>(seg) / segs;
        return {radius * std::sin(phi) * std::cos(theta),
                radius * std::cos(phi),
                radius * std::sin(phi) * std::sin(theta)};
    }

    // Fake-chrome colour: a sky/horizon gradient baked into the vertices,
    // a checker pattern for panels, and dark blue glass bands.
    static void setDomeColor(float h, bool alt, bool glass) {
        if (glass) {
            glColor3f(0.08f + 0.10f * h, 0.24f + 0.20f * h, 0.50f + 0.25f * h);
            return;
        }
        const float horizon = (h - 0.30f) / 0.09f;
        float v = 0.62f + 0.30f * h + 0.20f * std::exp(-horizon * horizon);
        if (alt) v *= 0.93f;
        v = std::min(v, 1.0f);
        glColor3f(v * 0.94f, v * 0.98f, std::min(1.0f, v * 1.04f));
    }

    static void emitChromeVertex(const DomePoint& p, bool alt, bool glass) {
        const float nx = p.x / kDomeRadius;
        const float ny = p.y / kDomeRadius;
        const float nz = p.z / kDomeRadius;
        setDomeColor(ny, alt, glass);
        glNormal3f(nx, ny, nz);
        glVertex3f(p.x, p.y, p.z);
    }

    // Inside of the dome: unlit dark screen with a faint horizon glow.
    static void emitInnerVertex(const DomePoint& p) {
        const float h = p.y / kInnerDomeRadius;
        const float glow = std::exp(-(h / 0.20f) * (h / 0.20f));
        glColor3f(0.010f + 0.05f * glow,
                  0.018f + 0.11f * glow,
                  0.055f + 0.22f * glow);
        glVertex3f(p.x, p.y, p.z);
    }

    // Radius of the dome surface at a given height above its base.
    static float domeRadiusAt(float height) {
        return std::sqrt(kDomeRadius * kDomeRadius - height * height);
    }

    void drawDome() const {

        glPushMatrix();
        glTranslatef(0.0f, kWallTop, 0.0f);

        // Outer shell. Corners a b c d run clockwise seen from outside,
        // so the quads are emitted as a d c b (counter-clockwise).
        glBegin(GL_QUADS);
        for (int ring = 0; ring < kDomeRings; ++ring) {
            for (int seg = 0; seg < kDomeSegments; ++seg) {
                const bool glass = (seg % kGlassEvery) == 0;
                const bool alt   = ((ring + seg) % 2) == 0;

                const DomePoint a = domePoint(kDomeRadius, ring,     kDomeRings, seg,     kDomeSegments);
                const DomePoint b = domePoint(kDomeRadius, ring + 1, kDomeRings, seg,     kDomeSegments);
                const DomePoint c = domePoint(kDomeRadius, ring + 1, kDomeRings, seg + 1, kDomeSegments);
                const DomePoint d = domePoint(kDomeRadius, ring,     kDomeRings, seg + 1, kDomeSegments);

                emitChromeVertex(a, alt, glass);
                emitChromeVertex(d, alt, glass);
                emitChromeVertex(c, alt, glass);
                emitChromeVertex(b, alt, glass);
            }
        }
        glEnd();

        // Bright chrome rings hugging the shell.
        glColor3f(0.92f, 0.94f, 0.97f);
        for (float h : kDomeRingHeights) {
            glPushMatrix();
            glTranslatef(0.0f, h, 0.0f);
            shapes::torus(domeRadiusAt(h) + 0.03f, 0.05f, 48, 6);
            glPopMatrix();
        }

        // Crown: mast, tip and a tilted halo ring on the apex.
        glColor3f(0.85f, 0.88f, 0.92f);
        glPushMatrix();
        glTranslatef(0.0f, kDomeRadius - 0.1f, 0.0f);
        shapes::cylinder(0.12f, 1.5f, 16);

        glPushMatrix();
        glTranslatef(0.0f, 1.4f, 0.0f);
        shapes::sphere(0.22f, 12, 8);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(0.0f, 0.8f, 0.0f);
        glRotatef(25.0f, 1.0f, 0.0f, 0.0f);
        shapes::torus(0.55f, 0.035f, 32, 6);
        glPopMatrix();
        glPopMatrix();

        glPopMatrix();
    }


    // ---------------------------------------------------------
    // Interior: dark dome screen with stars, floor, seats, projector.
    // ---------------------------------------------------------
    void drawInterior() const {

        drawInnerDome();

        // Everything below sits on the interior floor (origin on the floor).
        glPushMatrix();
        glTranslatef(0.0f, kFloorY + kInteriorFloorLift, 0.0f);

        // Floor disc.
        glColor3f(0.10f, 0.11f, 0.16f);
        shapes::disk(kInteriorFloorRadius, 64);

        // Aisle carpet from the door to the centre.
        glColor3f(0.06f, 0.22f, 0.30f);
        glPushMatrix();
        glTranslatef(0.0f, 0.012f, 4.1f);
        shapes::plane(1.8f, 6.6f);
        glPopMatrix();

        drawSeatRows();
        drawProjector();

        glPopMatrix();
    }

    // Unlit inner dome, ceiling collar and stars.
    void drawInnerDome() const {

        // The city enables lighting before draw(); switch it off for the
        // emissive screen and switch it back on afterwards.
        glDisable(GL_LIGHTING);

        glPushMatrix();
        glTranslatef(0.0f, kWallTop, 0.0f);

        // Ceiling collar: a down-facing ring that closes the gap between the
        // polygonal wall ring and the round dome.
        glColor3f(0.02f, 0.03f, 0.07f);
        glBegin(GL_QUADS);
        for (int seg = 0; seg < kInnerSegments; ++seg) {
            const float t0 = 2.0f * kPi * seg / kInnerSegments;
            const float t1 = 2.0f * kPi * (seg + 1) / kInnerSegments;
            const float rIn = 7.2f, rOut = 7.7f, y = -0.01f;
            glVertex3f(rIn  * std::cos(t0), y, rIn  * std::sin(t0));
            glVertex3f(rOut * std::cos(t0), y, rOut * std::sin(t0));
            glVertex3f(rOut * std::cos(t1), y, rOut * std::sin(t1));
            glVertex3f(rIn  * std::cos(t1), y, rIn  * std::sin(t1));
        }
        glEnd();

        // Inner shell, wound a b c d so it faces inwards.
        glBegin(GL_QUADS);
        for (int ring = 0; ring < kInnerRings; ++ring) {
            for (int seg = 0; seg < kInnerSegments; ++seg) {
                emitInnerVertex(domePoint(kInnerDomeRadius, ring,     kInnerRings, seg,     kInnerSegments));
                emitInnerVertex(domePoint(kInnerDomeRadius, ring + 1, kInnerRings, seg,     kInnerSegments));
                emitInnerVertex(domePoint(kInnerDomeRadius, ring + 1, kInnerRings, seg + 1, kInnerSegments));
                emitInnerVertex(domePoint(kInnerDomeRadius, ring,     kInnerRings, seg + 1, kInnerSegments));
            }
        }
        glEnd();

        // Stars: evenly spread over the hemisphere (golden-angle spiral).
        // Two passes: dim small stars, then bright big ones.
        const int   starCount  = 150;
        const float starRadius = 7.4f;
        for (int pass = 0; pass < 2; ++pass) {
            const bool bright = (pass == 1);
            glPointSize(bright ? 4.0f : 2.0f);
            if (bright) glColor3f(1.0f, 0.95f, 0.80f);
            else        glColor3f(0.75f, 0.85f, 1.0f);

            glBegin(GL_POINTS);
            for (int i = 0; i < starCount; ++i) {
                if ((i % 9 == 0) != bright) continue;
                const float h = (i + 0.5f) / starCount;
                const float ringR = std::sqrt(1.0f - h * h);
                const float a = i * kGoldenAngle;
                glVertex3f(starRadius * ringR * std::cos(a),
                           starRadius * h,
                           starRadius * ringR * std::sin(a));
            }
            glEnd();
        }
        glPointSize(1.0f);

        glPopMatrix();

        glEnable(GL_LIGHTING);
    }


    // ---------------------------------------------------------
    // Seats: one geometry, many placements on three arcs.
    // ---------------------------------------------------------
    void drawSeatRows() const {
        for (const SeatRow& row : kSeatRows) {
            for (int i = 0; i < row.count; ++i) {
                const float t = (row.count > 1)
                    ? static_cast<float>(i) / (row.count - 1) : 0.5f;
                const float angleDeg =
                    kSeatArcStartDeg + t * (kSeatArcEndDeg - kSeatArcStartDeg);
                const float rad = angleDeg * kDegToRad;

                glPushMatrix();
                glTranslatef(row.radius * std::sin(rad), 0.0f,
                             row.radius * std::cos(rad));
                // A seat's front is +z; turn it to face the room centre.
                glRotatef(angleDeg + 180.0f, 0.0f, 1.0f, 0.0f);
                drawSeat();
                glPopMatrix();
            }
        }
    }

    // One reclining seat. Origin on the floor, front faces +z.
    void drawSeat() const {

        const float pedestalH = 0.30f;
        const float cushionH  = 0.16f;
        const float backW     = 0.90f;
        const float backH     = 0.85f;
        const float backT     = 0.14f;

        // Pedestal: the cylinder stands on the floor.
        glColor3f(0.16f, 0.18f, 0.24f);
        shapes::cylinder(0.16f, pedestalH, 12);

        // Cushion rests on the pedestal.
        glColor3f(0.10f, 0.22f, 0.45f);
        glPushMatrix();
        glTranslatef(0.0f, pedestalH + cushionH * 0.5f, 0.0f);
        shapes::box(0.90f, cushionH, 0.85f);
        glPopMatrix();

        // Armrests, flush against the cushion sides.
        glColor3f(0.20f, 0.24f, 0.32f);
        for (int side = -1; side <= 1; side += 2) {
            glPushMatrix();
            glTranslatef(side * 0.50f, pedestalH + 0.14f, -0.05f);
            shapes::box(0.10f, 0.28f, 0.70f);
            glPopMatrix();
        }

        // Backrest, reclined about the bottom edge it is hinged on.
        const float hingeY = -backH * 0.5f;   // p in the backrest's own frame

        glColor3f(0.12f, 0.30f, 0.58f);
        glPushMatrix();
        glTranslatef(0.0f, pedestalH + cushionH * 0.5f + backH * 0.5f, -0.40f);
        glTranslatef(0.0f, hingeY, 0.0f);                  // T(p)
        glRotatef(-18.0f, 1.0f, 0.0f, 0.0f);               // R (lean back)
        glTranslatef(0.0f, -hingeY, 0.0f);                 // T(-p)
        shapes::box(backW, backH, backT);
        glPopMatrix();
    }


    // ---------------------------------------------------------
    // Star projector: pedestal, column, tilted dumbbell with lenses.
    // ---------------------------------------------------------
    void drawProjector() const {

        const float columnH  = 1.45f;
        const float armHalf  = 0.80f;
        const float ballR    = 0.42f;

        // Base plate and column, both standing on the floor.
        glColor3f(0.20f, 0.23f, 0.29f);
        shapes::cylinder(0.85f, 0.25f, 32);

        glColor3f(0.38f, 0.42f, 0.49f);
        shapes::cylinder(0.30f, columnH, 24);

        // Joint at the top of the column.
        glPushMatrix();
        glTranslatef(0.0f, columnH, 0.0f);
        glColor3f(0.55f, 0.59f, 0.66f);
        shapes::sphere(0.30f, 16, 12);

        // Dumbbell, tilted about z.
        glRotatef(22.0f, 0.0f, 0.0f, 1.0f);

        glColor3f(0.55f, 0.59f, 0.66f);
        glPushMatrix();
        glTranslatef(0.0f, -armHalf, 0.0f);
        shapes::cylinder(0.09f, 2.0f * armHalf, 12);
        glPopMatrix();

        for (int end = -1; end <= 1; end += 2) {
            glPushMatrix();
            glTranslatef(0.0f, end * armHalf, 0.0f);
            drawLensBall(ballR);
            glPopMatrix();
        }

        glPopMatrix();
    }

    // A ball covered with small glowing-blue lenses.
    void drawLensBall(float radius) const {

        glColor3f(0.30f, 0.34f, 0.42f);
        shapes::sphere(radius, 16, 12);

        const float polarDeg[] = {40.0f, 90.0f, 140.0f};
        const int   lensesPerRing = 6;

        glColor3f(0.35f, 0.80f, 1.0f);
        for (float polar : polarDeg) {
            for (int k = 0; k < lensesPerRing; ++k) {
                glPushMatrix();
                glRotatef(k * (360.0f / lensesPerRing), 0.0f, 1.0f, 0.0f);
                glRotatef(polar, 0.0f, 0.0f, 1.0f);
                glTranslatef(0.0f, radius - 0.03f, 0.0f);
                shapes::cylinder(0.05f, 0.10f, 8);
                glPopMatrix();
            }
        }
    }


    // ---------------------------------------------------------
    // Glowing accents (unlit): dome rim, base ring, floor rings, aisle lights.
    // ---------------------------------------------------------
    void drawGlowAccents() const {

        glDisable(GL_LIGHTING);
        glColor3f(0.25f, 0.85f, 1.0f);

        // Dome rim.
        glPushMatrix();
        glTranslatef(0.0f, kWallTop + 0.12f, 0.0f);
        shapes::torus(domeRadiusAt(0.12f) + 0.04f, 0.05f, 48, 5);
        glPopMatrix();

        // Ring on the plinth, just outside the walls.
        glPushMatrix();
        glTranslatef(0.0f, kFloorY + 0.03f, 0.0f);
        shapes::torus(8.32f, 0.04f, 48, 5);
        glPopMatrix();

        // Rings inlaid in the interior floor.
        const float floorY = kFloorY + kInteriorFloorLift + 0.05f;
        const float inlayRadii[] = {2.3f, 7.1f};
        for (float r : inlayRadii) {
            glPushMatrix();
            glTranslatef(0.0f, floorY, 0.0f);
            shapes::torus(r, 0.03f, 48, 5);
            glPopMatrix();
        }

        // Aisle lights on both sides of the carpet.
        for (int i = 0; i < 6; ++i) {
            for (int side = -1; side <= 1; side += 2) {
                glPushMatrix();
                glTranslatef(side * 1.0f, floorY + 0.02f, 1.2f + i * 1.0f);
                shapes::sphere(0.06f, 6, 4);
                glPopMatrix();
            }
        }

        glEnable(GL_LIGHTING);
    }


    GLuint scenery_ = 0;
};

}  // namespace team02

CITYLAB_REGISTER_ZONE(team02::MyZone);