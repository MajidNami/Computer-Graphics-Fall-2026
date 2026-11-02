#include "engine/Font.h"

#include <vector>

#include "stb/stb_image.h"

namespace citylab {
namespace {

#include "engine/FontData.inc"

// Decodes one UTF-8 code point and advances `i`.
unsigned nextCodePoint(const std::string& s, size_t& i) {
    const auto c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) { ++i; return c; }
    int extra = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : 0;
    unsigned code = c & (0x3F >> extra);
    ++i;
    while (extra-- > 0 && i < s.size()) code = (code << 6) | (static_cast<unsigned char>(s[i++]) & 0x3F);
    return code;
}

const FontGlyph* findGlyph(unsigned code) {
    for (const FontGlyph& g : kFontGlyphs)
        if (g.code == code) return &g;
    return nullptr;
}

const FontGlyph* glyphFor(unsigned code) {
    if (code >= 32 && code < 127) return &kFontGlyphs[code - 32];
    const FontGlyph* g = findGlyph(code);
    return g ? g : &kFontGlyphs['?' - 32];
}

}  // namespace

bool Font::init() {
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(kFontPng, static_cast<int>(kFontPngSize), &w, &h, &channels, 1);
    if (!pixels) return false;
    // Store as luminance-alpha white so glColor tints the text.
    std::vector<unsigned char> la(static_cast<size_t>(w) * h * 2);
    for (int k = 0; k < w * h; ++k) {
        la[static_cast<size_t>(k) * 2] = 255;
        la[static_cast<size_t>(k) * 2 + 1] = pixels[k];
    }
    stbi_image_free(pixels);
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, w, h, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, la.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    atlasWidth_ = w;
    atlasHeight_ = h;
    return true;
}

float Font::width(const std::string& utf8, float scale) const {
    float w = 0.0f;
    for (size_t i = 0; i < utf8.size();) w += glyphFor(nextCodePoint(utf8, i))->advance;
    return w * scale;
}

float Font::lineHeight(float scale) const { return static_cast<float>(kFontLineHeight) * scale; }

void Font::drawRun(float x, float y, const std::string& utf8, float scale) const {
    float pen = x;
    const float iw = 1.0f / static_cast<float>(atlasWidth_), ih = 1.0f / static_cast<float>(atlasHeight_);
    glBegin(GL_QUADS);
    for (size_t i = 0; i < utf8.size();) {
        const FontGlyph* g = glyphFor(nextCodePoint(utf8, i));
        if (g->w > 0 && g->h > 0) {
            const float x0 = pen + g->left * scale, y0 = y + g->top * scale;
            const float x1 = x0 + g->w * scale, y1 = y0 + g->h * scale;
            const float u0 = g->x * iw, v0 = g->y * ih, u1 = (g->x + g->w) * iw, v1 = (g->y + g->h) * ih;
            glTexCoord2f(u0, v0); glVertex2f(x0, y0);
            glTexCoord2f(u0, v1); glVertex2f(x0, y1);
            glTexCoord2f(u1, v1); glVertex2f(x1, y1);
            glTexCoord2f(u1, v0); glVertex2f(x1, y0);
        }
        pen += g->advance * scale;
    }
    glEnd();
}

void Font::draw(float x, float y, const std::string& utf8, const glm::vec4& color, float scale) const {
    if (!texture_) return;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(0.0f, 0.0f, 0.0f, color.a * 0.55f);
    drawRun(x + 1.0f, y + 1.5f, utf8, scale);
    glColor4f(color.r, color.g, color.b, color.a);
    drawRun(x, y, utf8, scale);
    glDisable(GL_TEXTURE_2D);
}

}  // namespace citylab
