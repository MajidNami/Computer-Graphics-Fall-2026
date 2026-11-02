#include "city/Chunks.h"

#include "engine/Log.h"
#include "engine/VertexCount.h"

namespace citylab {
namespace {

// A drawing whose size is known: a sanity check that this driver's feedback
// mode counts. If it does not, the chunks keep a box as big as the world and
// are always drawn, as before.
bool feedbackCounts() {
    const GLuint probe = glGenLists(1);
    glNewList(probe, GL_COMPILE);
    glBegin(GL_QUADS);
    glVertex3f(-1.0f, 0.0f, -1.0f);
    glVertex3f(1.0f, 0.0f, -1.0f);
    glVertex3f(1.0f, 0.0f, 1.0f);
    glVertex3f(-1.0f, 0.0f, 1.0f);
    glEnd();
    glEndList();
    const DrawMeasure m = measureDrawing([&] { glCallList(probe); });
    glDeleteLists(probe, 1);
    return !m.overflow && m.vertices == 6 && m.max.x > 0.9f && m.min.x < -0.9f;
}

}  // namespace

void Chunks::open() {
    list_ = glGenLists(1);
    glNewList(list_, GL_COMPILE);
    if (stateless_) return;
    if (texture_) glBindTexture(GL_TEXTURE_2D, texture_);
    glColor3f(r_, g_, b_);
}

void Chunks::close() {
    if (!list_) return;
    glEndList();
    const GLuint list = list_;
    list_ = 0;

    static bool checked = false;
    if (!checked) {
        checked = true;
        measured_ = feedbackCounts();
        if (!measured_) log::warn("city", "this driver cannot measure display lists; the city is drawn whole");
    }
    Chunk c;
    c.list = list;
    // A chunk whose size is not known keeps a box as big as the world, so it is
    // drawn whatever the view; it is never dropped.
    c.min = glm::vec3(-1e5f);
    c.max = glm::vec3(1e5f);
    if (measured_) {
        const DrawMeasure m = measureDrawing([&] { glCallList(list); });
        if (!m.overflow && m.vertices == 0) {  // nothing in it, only state
            glDeleteLists(list, 1);
            return;
        }
        if (!m.overflow) {
            c.vertices = m.vertices;
            c.min = m.min;
            c.max = m.max;
        }
    }
    chunks_[static_cast<size_t>(layer_)].push_back(c);
}

void Chunks::begin(Layer layer, bool stateless) {
    close();
    layer_ = layer;
    stateless_ = stateless;
    open();
}

void Chunks::next() {
    if (!list_) return;
    close();
    open();
}

void Chunks::end() { close(); }

void Chunks::texture(GLuint id) {
    texture_ = id;
    glBindTexture(GL_TEXTURE_2D, id);
}

void Chunks::color(float r, float g, float b) {
    r_ = r;
    g_ = g;
    b_ = b;
    glColor3f(r, g, b);
}

long Chunks::vertices(Layer l) const {
    long sum = 0;
    for (const Chunk& c : layer(l)) sum += c.vertices;
    return sum;
}

size_t Chunks::count() const {
    size_t n = 0;
    for (const auto& v : chunks_) n += v.size();
    return n;
}

}  // namespace citylab
