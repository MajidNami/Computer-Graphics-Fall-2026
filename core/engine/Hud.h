#pragma once

#include <string>

#include <glm/glm.hpp>

#include "engine/Font.h"

namespace citylab {

// 2D overlay helpers: pixel coordinates, origin at the top-left corner.
class Hud {
public:
    bool init() { return font_.init(); }

    void begin(int width, int height) const;  // saves GL state, sets an orthographic projection
    void end() const;                         // restores everything

    void panel(float x, float y, float w, float h, const glm::vec4& color) const;
    void line(const glm::vec2& a, const glm::vec2& b, const glm::vec4& color, float width = 1.0f) const;
    void text(float x, float y, const std::string& s, const glm::vec4& color = glm::vec4(1.0f), float scale = 1.0f) const {
        font_.draw(x, y, s, color, scale);
    }
    float textWidth(const std::string& s, float scale = 1.0f) const { return font_.width(s, scale); }
    float lineHeight(float scale = 1.0f) const { return font_.lineHeight(scale); }

private:
    Font font_;
};

}  // namespace citylab
