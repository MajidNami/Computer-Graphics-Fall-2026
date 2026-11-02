#pragma once

#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>

#include <glm/glm.hpp>

#include "citylab/Context.h"
#include "citylab/Zone.h"
#include "city/Layout.h"
#include "city/Registry.h"

namespace citylab {

class CollisionWorld;

// What the city knows about "now", in world coordinates. Each ZoneHost turns
// it into the plot-local Frame its own zone sees.
struct CityFrame {
    float dt = 0.0f;
    float hours = 12.0f;
    float daylight = 1.0f;
    int day = 0;
    glm::vec3 sunDirection{0.0f, 1.0f, 0.0f};
    glm::vec3 eye{0.0f};
};

// C++ zones linked into this executable, by team id (CITYLAB_REGISTER_ZONE).
const std::map<std::string, detail::ZoneFactory>& linkedZones();

// Creates the zone object for a team (its linked C++ class). Returns nullptr
// and fills `error` when it cannot.
using ZoneMaker = std::function<std::unique_ptr<Zone>(std::string& error)>;

// Per-zone resource limits. The checker enforces the same numbers.
struct ZoneBudget {
    static constexpr int kTextures = 16;
    static constexpr int kColliders = 400;
    static constexpr int kLights = 4;  // GL_LIGHT1 .. GL_LIGHT4
};

// Runs one team's zone and keeps it in its box.
//
//  * Space: the zone draws in plot-local coordinates; five clip planes cut
//    everything outside its plot (80 x 100 m, or 160 x 100, and 40 m high), so
//    a zone can never paint over the street or a neighbour.
//  * State: all OpenGL state is saved before draw() and restored after it,
//    and the zone starts every frame from the same documented defaults.
//  * Mistakes: unbalanced glBegin/glEnd, glPushMatrix/glPopMatrix,
//    glPushAttrib or glNewList are repaired and reported once, by name.
//  * Faults: an exception stops only that zone; the plot shows as empty and
//    F8 restarts it.
class ZoneHost {
public:
    enum class State { Stopped, Running, Failed };

    // What the checker learns from one draw() call.
    struct Probe {
        int extraLights = 0;     // GL_LIGHT1..7 left enabled by the zone
        int highestLight = 0;    // e.g. 5 when GL_LIGHT5 was used
        bool sunChanged = false; // GL_LIGHT0 was modified
    };

    ZoneHost(const TeamInfo& team, const PlotLayout& plot, const Layout& layout, CollisionWorld& collisions,
             ZoneMaker maker);
    ~ZoneHost();
    ZoneHost(const ZoneHost&) = delete;
    ZoneHost& operator=(const ZoneHost&) = delete;

    void start();
    void update(const CityFrame& city, bool focused);
    // In the city: plot transform and clip planes. With `local` the caller
    // has set up the matrices itself (the checker draws in plot-local space).
    void draw(bool local = false, Probe* probe = nullptr);
    void reset();     // F8: reset() the zone, or restart it after a failure
    void key(int key);

    State state() const { return state_; }
    bool running() const { return state_ == State::Running; }
    const std::string& failure() const { return failure_; }
    const TeamInfo& team() const { return team_; }
    const PlotLayout& plot() const { return plot_; }
    const Frame& frame() const { return frame_; }
    double drawMilliseconds() const { return drawMs_; }
    double updateMilliseconds() const { return updateMs_; }
    int textureCount() const;
    int colliderCount() const;
    const std::set<std::string>& warnings() const { return warned_; }

private:
    class ZoneContext;
    friend class ZoneContext;

    void fail(const std::string& where, const std::string& what);
    void fail(const std::string& where, const std::exception& e);
    void warnOnce(const std::string& message);
    void repairAfter(const char* where, int modelviewDepth, int attribDepth);

    const TeamInfo& team_;
    const PlotLayout& plot_;
    const Layout& layout_;
    CollisionWorld& collisions_;
    ZoneMaker maker_;
    std::unique_ptr<ZoneContext> context_;
    std::unique_ptr<Zone> zone_;
    State state_ = State::Stopped;
    std::string failure_;
    std::set<std::string> warned_;
    Frame frame_;
    double drawMs_ = 0.0, updateMs_ = 0.0;
};

}  // namespace citylab
