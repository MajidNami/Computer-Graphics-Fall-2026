#include "city/Materials.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include "city/Procedural.h"
#include "engine/Texture.h"

namespace citylab {
namespace procedural {

float noise(float x, float y, int period, int seed) {
    const int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(xi), fy = y - static_cast<float>(yi);
    const auto wrap = [period](int v) { return period > 0 ? ((v % period) + period) % period : v; };
    const float a = hash(wrap(xi), wrap(yi), seed), b = hash(wrap(xi + 1), wrap(yi), seed);
    const float c = hash(wrap(xi), wrap(yi + 1), seed), d = hash(wrap(xi + 1), wrap(yi + 1), seed);
    const float u = fx * fx * (3.0f - 2.0f * fx), v = fy * fy * (3.0f - 2.0f * fy);
    return (a + (b - a) * u) + ((c + (d - c) * u) - (a + (b - a) * u)) * v;
}

float fbm(float x, float y, int octaves, int period, int seed) {
    float sum = 0.0f, amplitude = 0.5f, total = 0.0f;
    for (int o = 0; o < octaves; ++o) {
        sum += amplitude * noise(x, y, period, seed + o * 31);
        total += amplitude;
        x *= 2.0f;
        y *= 2.0f;
        period *= 2;
        amplitude *= 0.5f;
    }
    return sum / total;
}

}  // namespace procedural

namespace {

using procedural::fbm;
using procedural::hash;

struct Rgb {
    float r, g, b;
};

Rgb mix(Rgb a, Rgb b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

Rgb scale(Rgb c, float k) { return {c.r * k, c.g * k, c.b * k}; }

unsigned char byte(float v) { return static_cast<unsigned char>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); }

Image paint(int size, const std::function<Rgb(float u, float v, float& alpha)>& shader) {
    Image image(size, size);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float alpha = 1.0f;
            const Rgb c = shader((static_cast<float>(x) + 0.5f) / static_cast<float>(size),
                                 (static_cast<float>(y) + 0.5f) / static_cast<float>(size), alpha);
            unsigned char* p = image.at(x, y);
            p[0] = byte(c.r);
            p[1] = byte(c.g);
            p[2] = byte(c.b);
            p[3] = byte(alpha);
        }
    }
    return image;
}

float smooth(float edge0, float edge1, float x) {
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Distance (in cell units, 0 at a joint, 0.5 in the middle) to the nearest
// joint of a grid of `cols` x `rows` cells; every other row is shifted by
// `shift` of a cell (running bond). Also returns the cell.
float gridJoint(float u, float v, int cols, int rows, float shift, int& cellX, int& cellY) {
    const float fy = v * static_cast<float>(rows);
    cellY = static_cast<int>(std::floor(fy));
    const float fx = u * static_cast<float>(cols) + ((cellY % 2) ? shift : 0.0f);
    cellX = static_cast<int>(std::floor(fx)) % cols;  // wrap so the tile repeats seamlessly
    const float dx = std::min(fx - std::floor(fx), 1.0f - (fx - std::floor(fx)));
    const float dy = std::min(fy - std::floor(fy), 1.0f - (fy - std::floor(fy)));
    return std::min(dx, dy);
}

Image grassImage() {
    // Park lawn: varied greens with clover and a few worn, drier patches, so
    // a large lawn does not look like a sports pitch.
    const Rgb deep{0.25f, 0.45f, 0.17f}, light{0.42f, 0.60f, 0.24f}, dry{0.58f, 0.60f, 0.34f};
    return paint(512, [&](float u, float v, float&) {
        const float n = fbm(u * 8, v * 8, 5, 8, 11);
        const float patches = fbm(u * 2, v * 2, 3, 2, 23);
        const float blades = hash(static_cast<int>(u * 512), static_cast<int>(v * 512), 5);
        Rgb c = mix(deep, light, n * 1.3f - 0.15f);
        c = mix(c, dry, smooth(0.55f, 0.78f, patches) * 0.55f);
        if (hash(static_cast<int>(u * 160), static_cast<int>(v * 160), 41) > 0.985f) c = Rgb{0.95f, 0.95f, 0.85f};  // daisies
        return scale(c, 0.86f + 0.22f * blades);
    });
}

Image meadowImage() {
    // Open land outside the city: only the fine detail of long grass, little
    // clumps and a few straw flecks, as a grey-green texture that is nearly
    // neutral in colour. Nothing in it is bigger than a metre or two, so that
    // repeating it every 20 m cannot show as a pattern; the fields of green,
    // straw and bare earth are the colour of the vertices (meadowColour).
    return paint(512, [&](float u, float v, float&) {
        const float clump = fbm(u * 40, v * 40, 3, 40, 61);      // 0.5 m
        const float tuft = fbm(u * 120, v * 120, 2, 120, 17);    // 0.17 m
        const float grain = hash(static_cast<int>(u * 512), static_cast<int>(v * 512), 8);
        float k = 0.80f + 0.26f * clump + 0.10f * tuft + 0.10f * grain;
        Rgb c{0.96f, 1.0f, 0.90f};  // a trace of green: the vertices bring the rest
        if (hash(static_cast<int>(u * 256), static_cast<int>(v * 256), 29) > 0.93f) c = Rgb{1.0f, 0.97f, 0.80f};  // straw fleck
        return scale(c, k);
    });
}

Image gravelImage() {
    // Light crushed stone of a prepared building plot.
    const Rgb a{0.80f, 0.77f, 0.71f}, b{0.66f, 0.63f, 0.58f}, dark{0.48f, 0.46f, 0.43f};
    return paint(256, [&](float u, float v, float&) {
        const float n = fbm(u * 16, v * 16, 4, 16, 27);
        const float grain = hash(static_cast<int>(u * 256), static_cast<int>(v * 256), 3);
        Rgb c = mix(a, b, n);
        if (grain > 0.9f) c = dark;
        return scale(c, 0.9f + 0.16f * grain);
    });
}

Image asphaltImage() {
    // Fine dark aggregate with light speckles and soft patches of wear.
    const Rgb dark{0.20f, 0.21f, 0.23f}, worn{0.30f, 0.31f, 0.33f}, stone{0.55f, 0.55f, 0.56f};
    return paint(512, [&](float u, float v, float&) {
        const float n = fbm(u * 6, v * 6, 5, 6, 71);
        const float grain = hash(static_cast<int>(u * 512), static_cast<int>(v * 512), 13);
        Rgb c = mix(dark, worn, n * 1.1f - 0.2f);
        c = mix(c, stone, smooth(0.93f, 1.0f, grain) * 0.6f);
        return scale(c, 0.94f + 0.1f * grain);
    });
}

Image paversImage() {
    // Light concrete pavers, 50 x 25 cm in running bond, three greys mixed:
    // 8 x 16 stones on a 4 m tile.
    const Rgb a{0.80f, 0.79f, 0.76f}, b{0.70f, 0.70f, 0.69f}, c3{0.86f, 0.83f, 0.78f}, joint{0.50f, 0.50f, 0.49f};
    return paint(512, [&](float u, float v, float&) {
        int x = 0, y = 0;
        const float edge = gridJoint(u, v, 8, 16, 0.5f, x, y);
        const float pick = hash(x, y, 31);
        Rgb c = pick < 0.45f ? a : pick < 0.8f ? b : c3;
        c = scale(c, 0.94f + 0.1f * fbm(u * 24, v * 24, 3, 24, 4));
        return mix(joint, c, smooth(0.03f, 0.09f, edge));
    });
}

Image plazaImage() {
    // Large warm-grey granite slabs, 1 x 1 m with a few darker accents.
    const Rgb a{0.86f, 0.83f, 0.78f}, b{0.76f, 0.73f, 0.69f}, accent{0.55f, 0.56f, 0.60f}, joint{0.58f, 0.56f, 0.52f};
    return paint(512, [&](float u, float v, float&) {
        int x = 0, y = 0;
        const float edge = gridJoint(u, v, 4, 4, 0.0f, x, y);
        Rgb c = mix(a, b, hash(x, y, 8));
        if (hash(x, y, 21) > 0.86f) c = accent;
        c = scale(c, 0.93f + 0.12f * fbm(u * 20, v * 20, 4, 20, 14));
        return mix(joint, c, smooth(0.01f, 0.03f, edge));
    });
}

Image parkPathImage() {
    // Warm sand-coloured stabilised gravel.
    const Rgb a{0.86f, 0.76f, 0.58f}, b{0.74f, 0.64f, 0.48f};
    return paint(256, [&](float u, float v, float&) {
        const float n = fbm(u * 24, v * 24, 3, 24, 77);
        const float grains = hash(static_cast<int>(u * 256), static_cast<int>(v * 256), 19);
        return scale(mix(a, b, n), 0.88f + 0.22f * grains);
    });
}

Image concreteImage() {
    // Smooth light concrete with formwork panel joints every 2 m (4 m tile)
    // and tie holes.
    const Rgb a{0.84f, 0.84f, 0.82f}, b{0.74f, 0.74f, 0.73f}, joint{0.56f, 0.56f, 0.55f};
    return paint(256, [&](float u, float v, float&) {
        int x = 0, y = 0;
        const float edge = gridJoint(u, v, 2, 2, 0.0f, x, y);
        Rgb c = mix(a, b, fbm(u * 8, v * 8, 5, 8, 52) * 0.9f);
        const float hx = std::fmod(u * 8.0f, 1.0f) - 0.5f, hy = std::fmod(v * 8.0f, 1.0f) - 0.5f;
        if (hx * hx + hy * hy < 0.006f) c = scale(c, 0.8f);
        return mix(joint, c, smooth(0.004f, 0.012f, edge));
    });
}

// Building facades share one grid so the night windows line up with both:
// a 12 x 12 m tile of 4 x 4 cells, each cell 3 m wide and one 3 m storey.
constexpr int kFacadeCells = 4;

// Position inside a facade cell: x, y in 0..1, and the cell itself.
void facadeCell(float u, float v, float& x, float& y, int& cx, int& cy) {
    const float fx = u * kFacadeCells, fy = v * kFacadeCells;
    cx = static_cast<int>(std::floor(fx));
    cy = static_cast<int>(std::floor(fy));
    x = fx - static_cast<float>(cx);
    y = fy - static_cast<float>(cy);
}

Image glassImage() {
    // Curtain wall: blue-green glass reflecting a bright sky, thin aluminium
    // mullions, a spandrel band at every floor slab.
    const Rgb skyHigh{0.62f, 0.78f, 0.90f}, skyLow{0.30f, 0.48f, 0.62f}, mullion{0.82f, 0.85f, 0.88f},
        spandrel{0.26f, 0.36f, 0.44f};
    return paint(256, [&](float u, float v, float&) {
        float x = 0, y = 0;
        int cx = 0, cy = 0;
        facadeCell(u, v, x, y, cx, cy);
        // Each pane reflects a slightly different part of the sky.
        Rgb c = mix(skyLow, skyHigh, y * 0.7f + 0.3f * hash(cx, cy, 3));
        c = scale(c, 0.92f + 0.12f * fbm(u * 3, v * 3, 2, 3, 6));
        if (y < 0.16f) c = spandrel;
        if (x < 0.03f || x > 0.97f || (y > 0.16f && y < 0.19f)) c = mullion;
        return c;
    });
}

Image plainImage() {
    // Almost white with a trace of noise: painted metal, plastic, signs.
    return paint(64, [&](float u, float v, float&) {
        const float k = 0.96f + 0.04f * fbm(u * 4, v * 4, 2, 4, 1);
        return Rgb{k, k, k};
    });
}

Image waterImage() {
    const Rgb deep{0.12f, 0.42f, 0.62f}, shallow{0.25f, 0.62f, 0.78f}, sparkle{0.85f, 0.95f, 1.0f};
    return paint(256, [&](float u, float v, float& alpha) {
        const float n = fbm(u * 8, v * 8, 5, 8, 90);
        const float ripple = 0.5f + 0.5f * std::sin((v * 16.0f + n * 5.0f) * 2.0f * 3.14159f);
        Rgb c = mix(deep, shallow, n * 1.2f - 0.1f);
        c = mix(c, sparkle, smooth(0.95f, 1.0f, ripple) * smooth(0.45f, 0.7f, n) * 0.35f);
        alpha = 0.86f;
        return c;
    });
}

Image soilImage() {
    const Rgb a{0.56f, 0.44f, 0.31f}, b{0.44f, 0.34f, 0.24f}, gravel{0.66f, 0.62f, 0.56f};
    return paint(256, [&](float u, float v, float&) {
        const float n = fbm(u * 10, v * 10, 4, 10, 33);
        const Rgb c = mix(a, b, n);
        return mix(c, gravel, smooth(0.6f, 0.8f, fbm(u * 4, v * 4, 3, 4, 7)) * 0.5f);
    });
}

Image woodImage() {
    const Rgb a{0.72f, 0.52f, 0.32f}, b{0.55f, 0.38f, 0.22f};
    return paint(256, [&](float u, float v, float&) {
        const float grain = 0.5f + 0.5f * std::sin((u * 40.0f + fbm(u * 4, v * 16, 3, 4, 2) * 8.0f));
        return mix(a, b, grain * 0.7f);
    });
}

Image foliageImage() {
    const Rgb dark{0.20f, 0.42f, 0.14f}, mid{0.34f, 0.60f, 0.20f}, sun{0.60f, 0.76f, 0.28f};
    return paint(256, [&](float u, float v, float&) {
        const float n = fbm(u * 12, v * 12, 4, 12, 55);
        const float leaves = hash(static_cast<int>(u * 128), static_cast<int>(v * 128), 6);
        const Rgb c = mix(dark, mid, n * 1.3f);
        return mix(c, sun, smooth(0.72f, 1.0f, leaves) * 0.55f);
    });
}

Image radial(int size, bool dark) {
    Image image(size, size);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) / size * 2 - 1;
            const float dy = (static_cast<float>(y) + 0.5f) / size * 2 - 1;
            const float r = std::sqrt(dx * dx + dy * dy);
            float a = std::clamp(1.0f - r, 0.0f, 1.0f);
            a = dark ? smooth(0.0f, 0.7f, a) * 0.55f : a * a * (0.6f + 0.4f * a);
            unsigned char* p = image.at(x, y);
            p[0] = p[1] = p[2] = dark ? 0 : 255;
            p[3] = byte(a);
        }
    }
    return image;
}

}  // namespace

glm::vec3 Materials::meadowColour(float x, float z) {
    const Rgb grass{0.33f, 0.43f, 0.21f}, straw{0.56f, 0.52f, 0.35f}, earth{0.50f, 0.43f, 0.33f};
    const float dry = smooth(0.42f, 0.66f, fbm(x * 0.0075f, z * 0.0075f, 3, 0, 17));   // fields of 130 m
    const float bare = smooth(0.62f, 0.78f, fbm(x * 0.02f, z * 0.02f, 2, 0, 29));      // patches of 50 m
    const float light = 0.72f + 0.16f * fbm(x * 0.0035f, z * 0.0035f, 3, 0, 61);        // 300 m
    Rgb c = mix(grass, straw, 0.25f + 0.5f * dry);
    c = mix(c, earth, bare * 0.45f);
    return {c.r * light, c.g * light, c.b * light};
}

void Materials::build() {
    grass = uploadTexture(grassImage());
    meadow = uploadTexture(meadowImage());
    gravel = uploadTexture(gravelImage());
    asphalt = uploadTexture(asphaltImage());
    pavers = uploadTexture(paversImage());
    plaza = uploadTexture(plazaImage());
    parkPath = uploadTexture(parkPathImage());
    concrete = uploadTexture(concreteImage());
    glass = uploadTexture(glassImage());
    plain = uploadTexture(plainImage());
    water = uploadTexture(waterImage());
    soil = uploadTexture(soilImage());
    wood = uploadTexture(woodImage());
    foliage = uploadTexture(foliageImage());
    TextureOptions clamp;
    clamp.repeat = false;
    glow = uploadTexture(radial(64, false), clamp);
    blob = uploadTexture(radial(64, true), clamp);
}

}  // namespace citylab
