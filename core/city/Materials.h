#pragma once

#include <glm/glm.hpp>

#include "citylab/OpenGL.h"

namespace citylab {

// The city's own textures. All of them are generated procedurally at start-up,
// so the repository stays small and nothing depends on image files. The
// sizes are those of one repeat of the texture with world-space UVs.
struct Materials {
    GLuint grass = 0;       // park lawn, 4 m
    GLuint meadow = 0;      // open land outside the city, 20 m: grey-green detail only, the colour comes from meadowColour()
    GLuint gravel = 0;      // prepared plots, 4 m
    GLuint asphalt = 0;     // roads, 4 m
    GLuint pavers = 0;      // sidewalks: concrete pavers, 4 m
    GLuint plaza = 0;       // granite slabs, 4 m (tower and fountain squares)
    GLuint parkPath = 0;    // sand-coloured park paths, 4 m
    GLuint concrete = 0;    // smooth concrete with panel joints, 4 m
    GLuint glass = 0;       // curtain wall, 12 m (4 x 4 storeys of 3 m)
    GLuint plain = 0;       // near white: painted metal, plastic, markings
    GLuint water = 0;       // ripples, 8 m; scrolled with the texture matrix
    GLuint soil = 0;        // building sites, 4 m
    GLuint wood = 0;        // benches, decks
    GLuint foliage = 0;     // hedges and the leaves of flower beds and shrubs
    GLuint glow = 0;        // soft radial gradient (lamp halos, sun, moon)
    GLuint blob = 0;        // soft dark disc (contact shadows)

    void build();

    // The colour of the meadow at a point of the world: green, straw and bare
    // earth in fields of 50 to 300 m. The texture repeats every 20 m, so
    // anything it painted of that size would show as a grid from above; the
    // fields are painted by the colour of the vertices instead.
    static glm::vec3 meadowColour(float x, float z);
};

}  // namespace citylab
