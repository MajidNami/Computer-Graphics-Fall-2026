#include "citylab/Props.h"

#include <initializer_list>

#include "citylab/Shapes.h"

namespace citylab::props {
namespace {

struct Rgb {
    float r, g, b;
};

// Saves the state the objects change, and puts it back.
class Saved {
public:
    Saved() {
        glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_TEXTURE_BIT | GL_LIGHTING_BIT);
        glPushMatrix();
    }
    ~Saved() {
        glPopMatrix();
        glPopAttrib();
    }
    Saved(const Saved&) = delete;
    Saved& operator=(const Saved&) = delete;
};

// A part of an object: with a texture the colour only tints it, without one
// the plain colour is used.
void surface(GLuint texture, Rgb plain, Rgb tint = {1.0f, 1.0f, 1.0f}) {
    if (texture) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
        glColor3f(tint.r, tint.g, tint.b);
    } else {
        glDisable(GL_TEXTURE_2D);
        glColor3f(plain.r, plain.g, plain.b);
    }
}

void glowing(float glow, Rgb light) {
    const GLfloat e[4] = {light.r * glow, light.g * glow, light.b * glow, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, e);
}

// A box with its base centred on (x, y, z).
void boxAt(float x, float y, float z, float w, float h, float d, float uv = 0.0f) {
    glPushMatrix();
    glTranslatef(x, y + h * 0.5f, z);
    shapes::box(w, h, d, uv);
    glPopMatrix();
}

// An ellipsoid of leaves; `shade` makes it a little lighter or darker.
void canopy(const Textures& t, float x, float y, float z, float rx, float ry, float rz, float shade) {
    surface(t.leaves, {0.30f * shade, 0.55f * shade, 0.22f * shade}, {0.92f * shade, 1.0f * shade, 0.86f * shade});
    glPushMatrix();
    glTranslatef(x, y, z);
    glScalef(rx, ry, rz);
    shapes::sphere(1.0f, 10, 7);
    glPopMatrix();
}

// Objects are modelled at their natural height and scaled to the one asked for.
void scaleTo(float height, float natural) {
    const float s = height / natural;
    glScalef(s, s, s);
}

const Rgb kAnthracite{0.24f, 0.26f, 0.29f};
const Rgb kLampLight{1.0f, 0.94f, 0.82f};
const Rgb kCraneYellow{1.0f, 0.78f, 0.10f};

}  // namespace

void shrub(float height, const Textures& t) {
    const Saved saved;
    scaleTo(height, 1.2f);
    canopy(t, 0.0f, 0.55f, 0.0f, 0.9f, 0.65f, 0.9f, 0.95f);
}

void flowerBed(const Textures& t) {
    const Saved saved;
    surface(t.concrete, {0.88f, 0.88f, 0.86f}, {0.95f, 0.95f, 0.93f});
    boxAt(0.0f, 0.0f, 0.0f, 3.0f, 0.4f, 1.2f, 0.5f);
    surface(t.leaves, {0.28f, 0.50f, 0.20f}, {0.9f, 1.0f, 0.85f});
    boxAt(0.0f, 0.4f, 0.0f, 2.8f, 0.12f, 1.0f, 1.0f);
    // Flowers in the colours of the six districts, always in the same places.
    glDisable(GL_TEXTURE_2D);
    const float palette[6][3] = {{0.95f, 0.30f, 0.35f}, {1.0f, 0.78f, 0.20f}, {0.65f, 0.40f, 0.90f},
                                 {1.0f, 1.0f, 0.96f},   {1.0f, 0.55f, 0.20f}, {0.40f, 0.60f, 1.0f}};
    unsigned seed = 17u;
    auto next = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>((seed >> 8) & 0xFFFFu) / 65535.0f;
    };
    for (int i = 0; i < 26; ++i) {
        const float* c = palette[i % 6];
        glColor3f(c[0], c[1], c[2]);
        glPushMatrix();
        glTranslatef(-1.3f + 2.6f * next(), 0.6f, -0.45f + 0.9f * next());
        glRotatef(360.0f * next(), 0.0f, 1.0f, 0.0f);
        shapes::box(0.2f, 0.14f, 0.2f);  // a box is far cheaper than a sphere
        glPopMatrix();
    }
}

void bench(const Textures& t) {
    // Two concrete blocks and a slatted wooden seat with a back rest.
    const Saved saved;
    surface(t.concrete, {0.82f, 0.82f, 0.80f}, {0.9f, 0.9f, 0.88f});
    for (const float x : {-0.8f, 0.8f}) boxAt(x, 0.0f, 0.0f, 0.18f, 0.42f, 0.55f, 1.0f);
    surface(t.wood, {0.62f, 0.44f, 0.27f}, {1.0f, 0.92f, 0.82f});
    for (int k = 0; k < 4; ++k) boxAt(0.0f, 0.42f, -0.2f + 0.13f * static_cast<float>(k), 2.0f, 0.05f, 0.1f);
    glPushMatrix();
    glTranslatef(0.0f, 0.75f, -0.27f);
    glRotatef(-12.0f, 1, 0, 0);
    shapes::box(2.0f, 0.36f, 0.05f);
    glPopMatrix();
}

void streetLampPost(const Textures& t) {
    // Slim anthracite pole, a gentle arm reaching over the road (+z), the
    // luminaire housing at its end.
    const Saved saved;
    surface(t.metal, kAnthracite, kAnthracite);
    shapes::cylinder(0.22f, 0.6f, 10, true);
    shapes::cylinder(0.09f, kStreetLampHeight + 0.15f, 10, false);
    glPushMatrix();
    glTranslatef(0.0f, kStreetLampHeight + 0.05f, kStreetLampReach * 0.5f);
    shapes::box(0.12f, 0.1f, kStreetLampReach + 0.1f);
    glPopMatrix();
    boxAt(0.0f, kStreetLampHeight - 0.05f, kStreetLampReach, 0.42f, 0.14f, 0.9f);
}

void streetLampHead() {
    glPushMatrix();
    glTranslatef(0.0f, kStreetLampHeight - 0.09f, kStreetLampReach);
    shapes::box(0.34f, 0.04f, 0.78f);
    glPopMatrix();
}

void streetLamp(float glow, const Textures& t) {
    streetLampPost(t);
    const Saved saved;
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.85f, 0.85f, 0.82f);
    glowing(glow, kLampLight);
    streetLampHead();
}

void parkLampPost(const Textures& t) {
    const Saved saved;
    surface(t.metal, kAnthracite, kAnthracite);
    shapes::cylinder(0.16f, 0.4f, 10, true);
    shapes::cylinder(0.06f, kParkLampHeight - 0.25f, 8, false);
}

void parkLampGlobe() {
    glPushMatrix();
    glTranslatef(0.0f, kParkLampHeight, 0.0f);
    shapes::sphere(0.28f, 10, 6);
    glPopMatrix();
}

void parkLamp(float glow, const Textures& t) {
    parkLampPost(t);
    const Saved saved;
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.92f, 0.92f, 0.88f);
    glowing(glow, kLampLight);
    parkLampGlobe();
}

void busShelter(const Textures& t) {
    const Saved saved;
    surface(t.metal, {0.30f, 0.33f, 0.38f}, {0.30f, 0.33f, 0.38f});
    for (const float x : {-2.9f, 2.9f}) boxAt(x, 0.0f, -0.85f, 0.12f, 2.7f, 0.12f);
    boxAt(0.0f, 2.7f, 0.0f, 6.4f, 0.14f, 2.4f);  // roof
    glDisable(GL_TEXTURE_2D);
    glColor3f(1.0f, 0.55f, 0.15f);
    boxAt(0.0f, 2.84f, 0.0f, 6.4f, 0.06f, 2.4f);  // orange roof edge, the colour of the city's transit
    surface(t.wood, {0.62f, 0.44f, 0.27f}, {1.0f, 0.92f, 0.82f});
    boxAt(0.0f, 0.45f, -0.6f, 4.0f, 0.06f, 0.4f);  // seat
    surface(t.glass, {0.62f, 0.78f, 0.86f}, {0.85f, 0.95f, 1.0f});
    boxAt(0.0f, 0.15f, -0.95f, 5.8f, 2.4f, 0.04f, 0.08f);  // back and side panes
    for (const float x : {-2.95f, 2.95f}) boxAt(x, 0.15f, -0.35f, 0.04f, 2.4f, 1.1f, 0.08f);
}

void craneMast(const Textures& t) {
    // A lattice mast: four chords and diagonal bracing every 2.5 m.
    const Saved saved;
    surface(t.metal, kCraneYellow, kCraneYellow);
    const float h = kCraneHeight, s = 0.8f;
    boxAt(0.0f, 0.0f, 0.0f, 3.0f, 0.6f, 3.0f);  // footing
    for (const float x : {-s, s})
        for (const float z : {-s, s}) boxAt(x, 0.6f, z, 0.14f, h - 0.6f, 0.14f);
    for (float y = 1.6f; y < h - 1.0f; y += 2.5f) {
        for (int face = 0; face < 4; ++face) {
            glPushMatrix();
            glRotatef(90.0f * static_cast<float>(face), 0, 1, 0);
            glTranslatef(0.0f, y + 1.25f, s);
            glRotatef(45.0f, 0, 0, 1);
            shapes::box(0.08f, 2.3f, 0.08f);
            glPopMatrix();
        }
    }
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.25f, 0.27f, 0.30f);
    boxAt(0.0f, h, 0.0f, 2.2f, 0.5f, 2.2f);  // slewing ring
}

void craneJib(const Textures& t) {
    // In the jib's own frame: the mast axis at x = z = 0, the jib along +x.
    const Saved saved;
    const float h = kCraneHeight + 0.5f;
    surface(t.metal, kCraneYellow, kCraneYellow);
    boxAt(0.0f, h, 0.0f, 1.6f, 4.0f, 1.6f);           // tower head
    boxAt(14.0f, h + 0.6f, 0.0f, 28.0f, 0.9f, 1.0f);  // jib
    boxAt(-5.5f, h + 0.6f, 0.0f, 11.0f, 1.0f, 1.4f);  // counter-jib
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.85f, 0.85f, 0.85f);
    boxAt(-9.0f, h - 1.4f, 0.0f, 2.4f, 2.0f, 1.8f);   // counterweight
    glColor3f(0.25f, 0.27f, 0.30f);
    boxAt(-1.2f, h + 0.2f, 1.2f, 1.8f, 1.6f, 1.6f);   // cab
    glBegin(GL_LINES);  // ties from the tower head to both ends, and the hook rope
    glVertex3f(0.0f, h + 4.0f, 0.0f); glVertex3f(26.0f, h + 1.5f, 0.0f);
    glVertex3f(0.0f, h + 4.0f, 0.0f); glVertex3f(-10.0f, h + 1.6f, 0.0f);
    glVertex3f(18.0f, h + 0.6f, 0.0f); glVertex3f(18.0f, 6.0f, 0.0f);
    glEnd();
    glColor3f(0.95f, 0.30f, 0.20f);
    boxAt(18.0f, 5.2f, 0.0f, 0.5f, 0.8f, 0.5f);       // hook block
}

void crane(float jibDegrees, const Textures& t) {
    craneMast(t);
    const Saved saved;
    glRotatef(jibDegrees, 0, 1, 0);  // T(mast top) is inside the jib's frame
    craneJib(t);
}

}  // namespace citylab::props
