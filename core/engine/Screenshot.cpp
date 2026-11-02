#include "engine/Screenshot.h"

#include <cstring>
#include <ctime>
#include <fstream>
#include <vector>

#include "citylab/OpenGL.h"
#include "stb/stb_image_write.h"

namespace citylab {
namespace {

void appendToVector(void* context, void* data, int size) {
    auto* out = static_cast<std::vector<unsigned char>*>(context);
    const auto* bytes = static_cast<const unsigned char*>(data);
    out->insert(out->end(), bytes, bytes + size);
}

}  // namespace

bool saveScreenshot(const std::filesystem::path& file, int width, int height, std::string* error) {
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // OpenGL rows start at the bottom; PNG rows start at the top.
    const size_t row = static_cast<size_t>(width) * 3;
    std::vector<unsigned char> flipped(pixels.size());
    for (int y = 0; y < height; ++y)
        std::memcpy(&flipped[static_cast<size_t>(y) * row], &pixels[static_cast<size_t>(height - 1 - y) * row], row);

    std::vector<unsigned char> png;
    if (!stbi_write_png_to_func(appendToVector, &png, width, height, 3, flipped.data(), static_cast<int>(row))) {
        if (error) *error = "PNG encoding failed";
        return false;
    }
    std::error_code ec;
    if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary);
    if (!out) {
        if (error) *error = "cannot write " + file.string();
        return false;
    }
    out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    return static_cast<bool>(out);
}

std::filesystem::path timestampedScreenshotPath(const std::filesystem::path& root, const std::string& prefix) {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char name[64];
    std::strftime(name, sizeof(name), "_%Y-%m-%d_%H-%M-%S.png", &local);
    return root / "screenshots" / (prefix + name);
}

}  // namespace citylab
