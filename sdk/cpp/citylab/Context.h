#pragma once

#include <string>

#include <glm/glm.hpp>

#include "citylab/OpenGL.h"
#include "citylab/Zone.h"

namespace citylab {

// Facts about the plot a team was given. Sizes are in metres.
struct PlotInfo {
    std::string id;           // e.g. "TR-4"
    std::string district;     // e.g. "transport"
    std::string place;        // your topic from city/places.json, e.g. "airport"
    float width = 80.0f;      // x from -width/2 to +width/2 (frontage); 160 for a double plot
    float depth = 100.0f;     // z from -depth/2 (back) to +depth/2 (street gate)
    float height = 40.0f;     // y from 0 to height (and down to -5 for a basement or a pool)
    float gateWidth = 10.0f;  // the opening in the frontage, centred on x = 0
};

struct TextureOptions {
    bool repeat = true;    // GL_REPEAT (true) or GL_CLAMP_TO_EDGE (false)
    bool mipmaps = true;   // build mipmaps and use trilinear filtering
    bool nearest = false;  // GL_NEAREST magnification (pixel-art look)
};

// The services the city offers a zone. One Context belongs to one zone.
class Context {
public:
    virtual ~Context() = default;

    virtual const std::string& teamId() const = 0;
    virtual const PlotInfo& plot() const = 0;

    // The latest frame; handy inside draw().
    virtual const Frame& frame() const = 0;

    // Absolute path of a file inside your team folder, e.g. assetPath("assets/brick.png").
    // Paths that leave the team folder are rejected.
    virtual std::string assetPath(const std::string& relativePath) const = 0;

    // Loads a PNG/JPG/BMP/TGA from your team folder into a texture.
    // Only call this from init(). Returns 0 and logs a warning on failure.
    virtual GLuint loadTexture(const std::string& relativePath,
                               const TextureOptions& options = {}) = 0;

    // Uploads your own RGBA pixels (width * height * 4 bytes), e.g. a
    // procedural checkerboard. Only call this from init().
    virtual GLuint createTexture(int width, int height, const unsigned char* rgba,
                                 const TextureOptions& options = {}) = 0;

    // A solid box the walking visitor cannot pass through, in plot-local
    // coordinates. Boxes lower than 0.4 m can be stepped onto (stairs!).
    virtual void addCollider(const glm::vec3& minCorner, const glm::vec3& maxCorner) = 0;

    // Prints "[team07] message" in the terminal running the city.
    virtual void log(const std::string& message) const = 0;
};

}  // namespace citylab
