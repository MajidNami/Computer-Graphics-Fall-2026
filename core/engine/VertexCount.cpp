#include "engine/VertexCount.h"

#include <algorithm>
#include <vector>

#include "citylab/OpenGL.h"

namespace citylab {
namespace {

constexpr float kRange = 8192.0f;  // the box is +-kRange m: the whole city and its surroundings fit
constexpr int kViewport = 1024;    // only the mapping back to metres depends on it

}  // namespace

DrawMeasure measureDrawing(const std::function<void()>& draw) {
    static std::vector<GLfloat> buffer;
    if (buffer.empty()) buffer.resize(1u << 20);

    glPushAttrib(GL_VIEWPORT_BIT | GL_TRANSFORM_BIT | GL_ENABLE_BIT | GL_POLYGON_BIT);
    glDisable(GL_CULL_FACE);
    for (int plane = 0; plane < 6; ++plane) glDisable(GL_CLIP_PLANE0 + plane);
    glViewport(0, 0, kViewport, kViewport);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-kRange, kRange, -kRange, kRange, -kRange, kRange);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    DrawMeasure m;
    GLint count = -1;
    for (int attempt = 0; attempt < 8; ++attempt) {
        glFeedbackBuffer(static_cast<GLsizei>(buffer.size()), GL_3D, buffer.data());
        glRenderMode(GL_FEEDBACK);
        draw();
        count = glRenderMode(GL_RENDER);
        if (count >= 0) break;
        buffer.resize(buffer.size() * 2);  // overflowed: run it again with room for more
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();

    if (count < 0) {
        m.overflow = true;
        return m;
    }
    auto vertex = [&](GLint& i) {
        const glm::vec3 p{(2.0f * buffer[static_cast<size_t>(i)] / kViewport - 1.0f) * kRange,
                          (2.0f * buffer[static_cast<size_t>(i) + 1] / kViewport - 1.0f) * kRange,
                          -(2.0f * buffer[static_cast<size_t>(i) + 2] - 1.0f) * kRange};
        i += 3;
        m.min = glm::min(m.min, p);
        m.max = glm::max(m.max, p);
    };
    for (GLint i = 0; i < count;) {
        const GLint token = static_cast<GLint>(buffer[static_cast<size_t>(i++)]);
        switch (token) {
        case GL_POINT_TOKEN:
        case GL_BITMAP_TOKEN:
        case GL_DRAW_PIXEL_TOKEN:
        case GL_COPY_PIXEL_TOKEN:
            vertex(i);
            m.vertices += 1;
            break;
        case GL_LINE_TOKEN:
        case GL_LINE_RESET_TOKEN:
            vertex(i);
            vertex(i);
            m.vertices += 2;
            break;
        case GL_POLYGON_TOKEN: {
            const GLint n = static_cast<GLint>(buffer[static_cast<size_t>(i++)]);
            for (GLint k = 0; k < n; ++k) vertex(i);
            m.vertices += 3L * std::max<GLint>(1, n - 2);  // a polygon is sent as n - 2 triangles
            break;
        }
        case GL_PASS_THROUGH_TOKEN: ++i; break;
        default: i = count; break;  // unknown token: stop rather than misread
        }
    }
    return m;
}

}  // namespace citylab
