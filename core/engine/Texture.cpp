#include "engine/Texture.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

#include "stb/stb_image.h"

namespace citylab {

float maxAnisotropy() {
    static float cached = -1.0f;
    if (cached < 0.0f) {
        cached = 1.0f;
        const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if (extensions && std::strstr(extensions, "GL_EXT_texture_filter_anisotropic")) {
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &cached);
            if (cached < 1.0f) cached = 1.0f;
        }
    }
    return cached;
}

namespace {

Image halve(const Image& src) {
    const int w = src.width > 1 ? src.width / 2 : 1;
    const int h = src.height > 1 ? src.height / 2 : 1;
    Image dst(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int x0 = std::min(2 * x, src.width - 1), x1 = std::min(2 * x + 1, src.width - 1);
            const int y0 = std::min(2 * y, src.height - 1), y1 = std::min(2 * y + 1, src.height - 1);
            for (int c = 0; c < 4; ++c) {
                const int sum = src.at(x0, y0)[c] + src.at(x1, y0)[c] + src.at(x0, y1)[c] +
                                src.at(x1, y1)[c];
                dst.at(x, y)[c] = static_cast<unsigned char>((sum + 2) / 4);
            }
        }
    }
    return dst;
}

}  // namespace

GLuint uploadTexture(const Image& image, const TextureOptions& options) {
    if (image.width <= 0 || image.height <= 0) return 0;
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    const GLint wrap = options.repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, options.nearest ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    options.mipmaps ? GL_LINEAR_MIPMAP_LINEAR : (options.nearest ? GL_NEAREST : GL_LINEAR));

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, image.pixels.data());
    if (options.mipmaps) {
        Image level = image;
        int index = 0;
        while (level.width > 1 || level.height > 1) {
            level = halve(level);
            ++index;
            glTexImage2D(GL_TEXTURE_2D, index, GL_RGBA, level.width, level.height, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, level.pixels.data());
        }
        if (maxAnisotropy() > 1.0f)
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(8.0f, maxAnisotropy()));
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

bool loadImage(const std::filesystem::path& path, Image& out, std::string* error) {
    // Read the bytes ourselves so non-ASCII Windows paths work.
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "cannot open " + path.string();
        return false;
    }
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),
                                           std::istreambuf_iterator<char>());
    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h,
                                                &channels, 4);
    if (!data) {
        if (error) *error = path.string() + ": " + stbi_failure_reason();
        return false;
    }
    out = Image(w, h);
    std::memcpy(out.pixels.data(), data, out.pixels.size());
    stbi_image_free(data);
    return true;
}

Image flipVertically(const Image& image) {
    Image flipped(image.width, image.height);
    const size_t row = static_cast<size_t>(image.width) * 4;
    for (int y = 0; y < image.height; ++y)
        std::memcpy(flipped.at(0, y), image.at(0, image.height - 1 - y), row);
    return flipped;
}

Image fitWithin(const Image& image, int maxSide) {
    Image out = image;
    while (out.width > maxSide || out.height > maxSide) out = halve(out);
    return out;
}

GLuint loadTexture(const std::filesystem::path& path, const TextureOptions& options, std::string* error) {
    Image image;
    if (!loadImage(path, image, error)) return 0;
    // OpenGL's texture origin is the bottom-left corner; images start at the top.
    return uploadTexture(flipVertically(image), options);
}

}  // namespace citylab
