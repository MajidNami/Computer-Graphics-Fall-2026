#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "citylab/OpenGL.h"
#include "city/Layout.h"
#include "engine/Clock.h"
#include "engine/Frustum.h"

namespace citylab {

// What moves in the city around the plots: trams on the tram boulevard that
// stop at the traffic lights, for a walker on the track and at their
// shelters, and boats on the park lake. Trams keep to the right.
//
// Trams run along straight tracks from far outside the city to far outside
// on the other side, and start again at the beginning; the fog hides the
// ends. The traffic lights at every junction run whether or not anything
// waits at them.
class CityLife {
public:
    void build(const Layout& layout);
    // dt in real seconds. The walker on foot is someone the trams wait for.
    void update(float dt, const glm::vec3& walker, bool onFoot);
    void draw(const Environment& env, const Viewer& viewer) const;
    // Trams drawn in the last frame.
    int tramsDrawn() const { return tramsDrawn_; }
    // Staff measurements (--bench, F3): how many trams and boats there are.
    std::vector<std::string> statsLines() const;

private:
    enum class Light { Green, Amber, Red };

    struct Tram {
        float s = 0.0f;      // metres along the track: the front of the tram
        float speed = 0.0f;
        float topSpeed = 11.0f;
        float dwell = 0.0f;  // seconds left at a stop
        int nextHalt = 0;    // the next stop to serve
    };
    struct Track {
        glm::vec2 start{0.0f}, dir{1.0f, 0.0f};
        float length = 0.0f;
        bool alongX = true;
        std::vector<float> stopLines;  // before each junction, sorted
        std::vector<float> halts;      // where the front stops at a shelter, sorted
        std::vector<Tram> trams;       // front of the queue first
    };
    struct Boat {
        glm::vec2 radius{0.0f};
        float angle = 0.0f, speed = 0.0f, phase = 0.0f;
        bool pedalo = false;
    };

    Light light(bool alongX) const;
    void advance(Track& track, float dt, const glm::vec3& walker, bool onFoot);
    void buildModels();
    void drawSignals(const Environment& env, const Viewer& viewer) const;

    const Layout* layout_ = nullptr;
    std::vector<Track> tracks_;
    std::vector<Rect> junctions_;
    std::vector<Boat> boats_;
    float signalClock_ = 0.0f, time_ = 0.0f;

    GLuint tram_ = 0, tramLights_ = 0, rowingBoat_ = 0, pedalo_ = 0;
    std::vector<GLuint> signalPoles_;  // one list for every junction
    mutable int tramsDrawn_ = 0;
};

}  // namespace citylab
