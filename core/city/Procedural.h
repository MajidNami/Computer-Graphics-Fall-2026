#pragma once

#include <cstdint>

namespace citylab::procedural {

// Deterministic hash in [0, 1).
inline float hash(int x, int y, int seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u +
                 static_cast<uint32_t>(seed) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h & 0x00FFFFFFu) / 16777216.0f;
}

// Smooth value noise that repeats every `period` units (for seamless textures).
float noise(float x, float y, int period, int seed);

// Fractal sum of `octaves` noise layers, roughly in [0, 1].
float fbm(float x, float y, int octaves, int period, int seed);

// A small deterministic random generator for placing things in the city.
class Random {
public:
    explicit Random(uint32_t seed) : state_(seed ? seed : 1u) {}
    uint32_t next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }
    float uniform() { return static_cast<float>(next() & 0x00FFFFFFu) / 16777216.0f; }
    float range(float lo, float hi) { return lo + (hi - lo) * uniform(); }

private:
    uint32_t state_;
};

}  // namespace citylab::procedural
