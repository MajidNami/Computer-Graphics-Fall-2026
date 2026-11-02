#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "city/Chunks.h"
#include "city/Layout.h"
#include "city/Materials.h"
#include "city/Props.h"
#include "city/Registry.h"
#include "engine/Clock.h"
#include "engine/Frustum.h"

namespace citylab {

class CollisionWorld;

// Everything the city staff builds: the streets with their sidewalks,
// markings and the tram track, the central park with its lake, fountain and
// tower, the street lamps, the gate and sign of every plot, and the skyline
// around the city.
//
// The static parts are compiled once into display lists, cut into chunks
// (city/Chunks.h) that each know their bounding box. A frame starts with
// beginFrame(), which says which chunks are in view and near enough to see;
// only those are drawn. A few parts (water,
// fountain jets, tower lights, cranes, lamp glow, lit windows) are animated
// or switched per frame.
class CityScene {
public:
    void build(const Layout& layout, const Registry& registry, const std::filesystem::path& root,
               CollisionWorld& collisions);

    // Decides what this frame draws. Call it before drawOpaque().
    void beginFrame(const Viewer& viewer);
    // Opaque city geometry, of what beginFrame() found in view.
    void drawOpaque(const Environment& env, double time) const;
    // A plot whose zone is not running: a lawn for a free plot, a building
    // site with a turning crane for a plot whose team is not running.
    void drawEmptyPlot(const PlotLayout& plot, bool assigned, double time) const;
    // Water surfaces and translucent effects, after all opaque geometry.
    void drawTransparent(const glm::mat4& view, const Environment& env, double time) const;
    // F2: wire boxes around every plot volume.
    void drawPlotBorders(const glm::vec3& highlightColor, const std::string& highlightPlot) const;

    const Materials& materials() const { return materials_; }

    // What the last frame drew (F3, --bench).
    struct FrameStats {
        int chunks = 0, chunksDrawn = 0;      // pieces of scenery in the city, and in the frame
        long vertices = 0;                    // the scenery sent, in triangle corners
    };
    const FrameStats& frameStats() const { return stats_; }
    // The lines of the staff report: what the city is made of, and what it cost last frame.
    std::vector<std::string> statsLines() const;
    // Off while OpenGL counts the vertices the city submits (--bench).
    void setFaceCulling(bool on) const { faceCulling_ = on; }

private:
    struct Fountain {
        glm::vec3 base;
        float height;
        float spread;
    };
    struct Lamp {
        glm::vec3 light;  // world position of the light
        float pool;       // radius of its pool of light on the ground
    };
    struct Prop {  // a bench or a flower bed
        float x, z, yaw;
    };

    // Each part is compiled into chunks of its own.
    void planStreets();
    void buildGround();
    void buildStreets();
    void buildSidewalks();
    void buildPark();
    void buildTower();
    void buildTowerLights();
    void buildPlotFrames(const Registry& registry);
    void buildSigns();
    void buildSkyline();
    void buildLampsAndBenches();
    void buildScatter();
    void buildEmptyPlots();

    // Placement helpers that register colliders and remember what stands where.
    // They place props in world coordinates: call them outside any Piece.
    void streetLamp(float x, float z, float yawDegrees);
    void parkLamp(float x, float z);
    void benchAt(float x, float z, float yawDegrees);
    void flowerBedAt(float x, float z, float yawDegrees);
    void solid(const glm::vec3& min, const glm::vec3& max);
    bool occupied(float x, float z, float margin) const;
    bool onPath(float x, float z, float margin) const;  // park paths and squares

    // State and flat surfaces for the builders. They go into the chunk that is
    // open; a seam is where one chunk ends and the next begins.
    void bind(GLuint texture) { chunks_.texture(texture); }
    void color(float r, float g, float b) { chunks_.color(r, g, b); }
    void color(const glm::vec3& c) { chunks_.color(c.r, c.g, c.b); }
    void white() { chunks_.color(1.0f, 1.0f, 1.0f); }
    // geom::ground in pieces of at most kPiece metres, each its own chunk when it is long.
    void ground(const Rect& r, float y, float uv);

    GLuint loadSign(const std::string& name);
    void signBoard(GLuint texture, float width, float height);

    const Layout* layout_ = nullptr;
    CollisionWorld* collisions_ = nullptr;
    std::filesystem::path root_;
    Materials materials_;
    Props props_;
    Chunks chunks_;

    // The skyline walls come in five looks; the lists hold geometry only, so
    // the same ones are drawn again with the lit-window mask at night.
    struct SkylineLook {
        bool glass = true;
        glm::vec3 tint{1.0f};
    };
    std::vector<SkylineLook> looks_;
    std::map<std::string, GLuint> lawnLists_;  // the meadow of every plot, by plot id
    std::map<int, GLuint> detailLists_, siteLists_;  // by plot width

    std::vector<Rect> carriageways_;   // asphalt, cut at the junctions
    std::vector<Rect> junctions_;
    std::vector<Rect> islands_;        // raised areas with a kerb: blocks and the park
    std::vector<Rect> exits_;          // the avenues out of the city: whole bands
    std::vector<Rect> exitRoads_, exitWalks_;
    std::vector<Rect> parkPaths_;      // straight park paths
    std::vector<Lamp> lamps_;
    std::vector<glm::vec3> streetLampFrames_;  // x, z, yaw of every street lamp
    std::vector<glm::vec2> parkLamps_;
    std::vector<Prop> benches_, flowerBeds_;
    std::vector<glm::vec3> shadows_;  // x, z, radius
    std::vector<Fountain> fountains_;
    std::vector<Rect> occupied_;   // no lamp or bench here
    std::vector<Rect> keepClear_;  // signs and shelters: no street lamps either
    std::map<std::string, GLuint> signs_;

    // How many of everything the city has: numbers for tuning, not for the game.
    struct Census {
        int streetLamps = 0, parkLamps = 0, benches = 0, flowerBeds = 0, skyline = 0;
    } census_;

    // Per frame.
    Viewer viewer_;
    std::vector<unsigned> visible_[kLayerCount];  // indices into the chunks of each layer
    mutable FrameStats stats_;
    mutable bool faceCulling_ = true;
};

}  // namespace citylab
