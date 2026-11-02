#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "citylab/Context.h"
#include "citylab/OpenGL.h"

namespace citylab {

// An RGBA8 image in memory, row 0 at the top.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;  // width * height * 4

    Image() = default;
    Image(int w, int h) : width(w), height(h), pixels(static_cast<size_t>(w) * h * 4, 0) {}
    unsigned char* at(int x, int y) { return &pixels[(static_cast<size_t>(y) * width + x) * 4]; }
    const unsigned char* at(int x, int y) const {
        return &pixels[(static_cast<size_t>(y) * width + x) * 4];
    }
};

// Mirrors the rows, turning an image (row 0 at the top) into OpenGL's
// texture layout (row 0 at the bottom).
Image flipVertically(const Image& image);

// Halves the image (box filter) until neither side exceeds `maxSide`.
Image fitWithin(const Image& image, int maxSide);

// Uploads an image as a 2D texture. Mipmaps are built on the CPU with a box
// filter so this works on every OpenGL 1.1 driver.
GLuint uploadTexture(const Image& image, const TextureOptions& options = {});

// Loads PNG/JPG/BMP/TGA with stb_image. On failure returns 0 and sets *error.
bool loadImage(const std::filesystem::path& path, Image& out, std::string* error = nullptr);
GLuint loadTexture(const std::filesystem::path& path, const TextureOptions& options = {},
                   std::string* error = nullptr);

// Largest anisotropic filtering level the driver offers (1 if unsupported).
float maxAnisotropy();

// Largest texture side the city accepts from a team (budget).
constexpr int kMaxTeamTextureSize = 1024;

}  // namespace citylab
