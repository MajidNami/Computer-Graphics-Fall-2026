#pragma once

#include <string>

#include <glm/glm.hpp>

#include "citylab/OpenGL.h"

namespace citylab {

// Bitmap font for the HUD (Latin text, digits and a few symbols). The atlas
// is embedded in the executable (FontData.inc), so error messages can be shown
// even when the repository files cannot be found.
//
// Coordinates are window pixels with the origin at the top-left corner;
// call inside Hud::begin()/end().
class Font {
public:
    bool init();
    float width(const std::string& utf8, float scale = 1.0f) const;
    float lineHeight(float scale = 1.0f) const;
    // Draws with a soft dark shadow so text reads on any background.
    void draw(float x, float y, const std::string& utf8, const glm::vec4& color, float scale = 1.0f) const;

private:
    void drawRun(float x, float y, const std::string& utf8, float scale) const;
    GLuint texture_ = 0;
    int atlasWidth_ = 1, atlasHeight_ = 1;
};

}  // namespace citylab
