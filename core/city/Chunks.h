#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "citylab/OpenGL.h"
#include "engine/Frustum.h"

namespace citylab {

// What a chunk of scenery is, which decides how it is drawn and how far.
enum class Layer {
    Base,    // lit, textured and static: ground, streets, sidewalks, the park, architecture, props
    // Flat surfaces that lie on another surface are drawn with a polygon offset
    // that pulls them towards the viewer, so they never flicker against it. The
    // offset is set when the layer is drawn, never inside a list: OpenGL's
    // feedback mode, which measures the lists, cannot cope with it.
    Paint,   // park paths and the promenade; offset 1
    Plaza,   // the squares round the tower and the fountain, over the paths; offset 2
    Detail,  // lane lines, crossings, wires: thinner than a pixel from far away, so only near; offset 1
    Shadow,  // soft contact shadows under the lamps (blended)
    Pool,    // pools of lamplight on the ground (additive, at night)
    Head,    // the glowing heads of the street lamps
    Globe,   // the glowing globes of the park lamps
    Lights,  // the rings of light on the tower
    Wall0, Wall1, Wall2, Wall3, Wall4,  // skyline walls, by look
    Roof,
    Count
};
constexpr int kLayerCount = static_cast<int>(Layer::Count);

// A display list with the box it fits in and what it costs to draw. The
// city is cut into chunks so that what is behind the camera, or too far to
// see through the fog, is not sent to the graphics card at all.
struct Chunk {
    GLuint list = 0;
    long vertices = 0;           // triangle corners
    glm::vec3 min{0.0f}, max{0.0f};
};

// Compiles the city's display lists in pieces. The builders keep their
// shape (set a texture and a colour, draw, set the next); they only say where
// one piece ends with next(). Each piece is measured when it is closed.
class Chunks {
public:
    // Opens a list for `layer` (and closes the open one). A stateless list
    // leaves the texture and colour to whoever draws the layer.
    void begin(Layer layer, bool stateless = false);
    // Closes the open list and opens another one of the same layer, with the
    // texture and colour the builder had set.
    void next();
    void end();

    // The state a builder sets. Chunks remember it so that every new list
    // starts the way the builder left things.
    void texture(GLuint id);
    void color(float r, float g, float b);

    const std::vector<Chunk>& layer(Layer l) const { return chunks_[static_cast<size_t>(l)]; }
    long vertices(Layer l) const;
    size_t count() const;
    bool measured() const { return measured_; }

private:
    void open();
    void close();

    std::vector<Chunk> chunks_[kLayerCount];
    Layer layer_ = Layer::Base;
    bool stateless_ = false;
    GLuint list_ = 0;
    bool measured_ = true;  // false if the driver cannot count what a list draws

    GLuint texture_ = 0;
    float r_ = 1.0f, g_ = 1.0f, b_ = 1.0f;
};

}  // namespace citylab
