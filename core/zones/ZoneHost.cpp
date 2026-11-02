#include "zones/ZoneHost.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>

#include <glm/gtc/type_ptr.hpp>

#include "city/Frame.h"
#include "engine/Collision.h"
#include "engine/Log.h"
#include "engine/Texture.h"

namespace citylab {

// ---- Registration of linked C++ zones ------------------------------------

namespace {

std::map<std::string, detail::ZoneFactory>& factories() {
    static std::map<std::string, detail::ZoneFactory> map;  // safe during static initialisation
    return map;
}

using Clock = std::chrono::steady_clock;

double millisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

const char* errorName(GLenum e) {
    switch (e) {
    case GL_INVALID_ENUM: return "GL_INVALID_ENUM (a constant that does not fit this function)";
    case GL_INVALID_VALUE: return "GL_INVALID_VALUE (a number out of range)";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION (a call that is not allowed here)";
    case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW (too many glPush... calls)";
    case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW (too many glPop... calls)";
    case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
    default: return "unknown OpenGL error";
    }
}

// Reads and clears all pending OpenGL error flags.
GLenum drainErrors() {
    GLenum first = GL_NO_ERROR;
    for (int i = 0; i < 16; ++i) {
        const GLenum e = glGetError();
        if (e == GL_NO_ERROR) break;
        if (first == GL_NO_ERROR) first = e;
    }
    return first;
}

int integer(GLenum query) {
    GLint value = -1;
    glGetIntegerv(query, &value);
    return value;
}

}  // namespace

namespace detail {

bool registerZone(const char* teamId, ZoneFactory factory) {
    auto& map = factories();
    if (map.count(teamId)) std::fprintf(stderr, "[%s] error: CITYLAB_REGISTER_ZONE is used twice\n", teamId);
    map[teamId] = factory;
    return true;
}

}  // namespace detail

const std::map<std::string, detail::ZoneFactory>& linkedZones() { return factories(); }

// ---- The Context a zone receives -----------------------------------------

class ZoneHost::ZoneContext final : public Context {
public:
    ZoneContext(ZoneHost& host, const Layout& layout) : host_(host) {
        info_.id = host.plot_.id;
        info_.district = host.plot_.district;
        info_.place = host.plot_.place;
        info_.width = host.plot_.width;
        info_.depth = host.plot_.depth;
        info_.height = layout.plotHeight;
        info_.gateWidth = layout.gateWidth;
    }

    bool initializing = false;
    int textures = 0;
    int colliders = 0;

    const std::string& teamId() const override { return host_.team_.id; }
    const PlotInfo& plot() const override { return info_; }
    const Frame& frame() const override { return host_.frame_; }

    std::string assetPath(const std::string& relativePath) const override {
        const std::filesystem::path folder = host_.team_.folder.lexically_normal();
        const std::filesystem::path relative(relativePath);
        if (relative.is_absolute() || relative.has_root_name()) {
            host_.warnOnce("assetPath(\"" + relativePath + "\"): use a path relative to your team folder");
            return {};
        }
        const std::filesystem::path full = (folder / relative).lexically_normal();
        const auto [end, unused] = std::mismatch(folder.begin(), folder.end(), full.begin(), full.end());
        (void)unused;
        if (end != folder.end()) {
            host_.warnOnce("assetPath(\"" + relativePath + "\") leaves the team folder; that is not allowed");
            return {};
        }
        return full.string();
    }

    GLuint loadTexture(const std::string& relativePath, const TextureOptions& options) override {
        if (!admitTexture("loadTexture(\"" + relativePath + "\")")) return 0;
        const std::string path = assetPath(relativePath);
        if (path.empty()) return 0;
        Image image;
        std::string error;
        if (!loadImage(path, image, &error)) {
            host_.warnOnce("loadTexture: " + error);
            return 0;
        }
        return upload(flipVertically(image), relativePath, options);
    }

    GLuint createTexture(int width, int height, const unsigned char* rgba, const TextureOptions& options) override {
        if (!admitTexture("createTexture()")) return 0;
        if (width <= 0 || height <= 0 || !rgba) {
            host_.warnOnce("createTexture(): width and height must be positive and pixels not null");
            return 0;
        }
        Image image(width, height);
        std::copy(rgba, rgba + image.pixels.size(), image.pixels.begin());
        return upload(image, "createTexture()", options);
    }

    void addCollider(const glm::vec3& a, const glm::vec3& b) override {
        if (colliders >= ZoneBudget::kColliders) {
            host_.warnOnce("more than " + std::to_string(ZoneBudget::kColliders) +
                           " colliders; merge small boxes into bigger ones");
            return;
        }
        // Keep colliders inside the plot volume, like the geometry.
        const glm::vec3 half{info_.width * 0.5f, info_.height, info_.depth * 0.5f};
        const glm::vec3 lo = glm::max(glm::min(a, b), glm::vec3(-half.x, 0.0f, -half.z));
        const glm::vec3 hi = glm::min(glm::max(a, b), glm::vec3(half.x, half.y, half.z));
        if (lo.x >= hi.x || lo.y >= hi.y || lo.z >= hi.z) {
            host_.warnOnce("addCollider(): a box lies completely outside the plot and was ignored");
            return;
        }
        const GroundFrame frame{host_.plot_.center, host_.plot_.yawDegrees};
        host_.collisions_.add(frame.toWorld(lo, hi), host_.team_.id);
        ++colliders;
    }

    void log(const std::string& message) const override { log::info(host_.team_.id, message); }

private:
    bool admitTexture(const std::string& call) {
        if (!initializing) {
            host_.warnOnce(call + " was called outside init(); load textures once in init()");
            return false;
        }
        if (textures >= ZoneBudget::kTextures) {
            host_.warnOnce(call + ": more than " + std::to_string(ZoneBudget::kTextures) +
                           " textures; combine small images into one atlas");
            return false;
        }
        return true;
    }

    GLuint upload(const Image& image, const std::string& what, const TextureOptions& options) {
        Image fitted = image;
        if (image.width > kMaxTeamTextureSize || image.height > kMaxTeamTextureSize) {
            fitted = fitWithin(image, kMaxTeamTextureSize);
            host_.warnOnce(what + " is " + std::to_string(image.width) + "x" + std::to_string(image.height) +
                           " px; it was scaled down to " + std::to_string(fitted.width) + "x" +
                           std::to_string(fitted.height) + " (limit " + std::to_string(kMaxTeamTextureSize) + ")");
        }
        const GLuint id = uploadTexture(fitted, options);
        if (id) ++textures;
        return id;
    }

    ZoneHost& host_;
    PlotInfo info_;
};

// ---- ZoneHost -------------------------------------------------------------

ZoneHost::ZoneHost(const TeamInfo& team, const PlotLayout& plot, const Layout& layout, CollisionWorld& collisions,
                   ZoneMaker maker)
    : team_(team), plot_(plot), layout_(layout), collisions_(collisions), maker_(std::move(maker)) {}

ZoneHost::~ZoneHost() = default;

int ZoneHost::textureCount() const { return context_ ? context_->textures : 0; }
int ZoneHost::colliderCount() const { return context_ ? context_->colliders : 0; }

void ZoneHost::warnOnce(const std::string& message) {
    if (warned_.insert(message).second) log::warn(team_.id, message);
}

void ZoneHost::fail(const std::string& where, const std::string& what) {
    state_ = State::Failed;
    failure_ = where + ": " + what;
    log::error(team_.id, failure_ + "  (the plot is shown empty; fix the code and press F8 to restart)");
    collisions_.removeOwner(team_.id);
}

void ZoneHost::fail(const std::string& where, const std::exception& e) { fail(where, std::string(e.what())); }

void ZoneHost::start() {
    collisions_.removeOwner(team_.id);
    warned_.clear();
    zone_.reset();
    context_ = std::make_unique<ZoneContext>(*this, layout_);
    frame_ = Frame{};
    std::string error;
    try {
        zone_ = maker_(error);
    } catch (const std::exception& e) {
        error = e.what();
    }
    if (!zone_) {
        fail("start", error.empty() ? "no zone was created" : error);
        return;
    }
    state_ = State::Running;
    failure_.clear();

    drainErrors();
    const int modelview = integer(GL_MODELVIEW_STACK_DEPTH), attribs = integer(GL_ATTRIB_STACK_DEPTH);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    context_->initializing = true;
    try {
        zone_->init(*context_);
    } catch (const std::exception& e) {
        fail("init()", e);
    } catch (...) {
        fail("init()", "unknown exception");
    }
    context_->initializing = false;
    repairAfter("init()", modelview, attribs + 1);
    glPopAttrib();
    if (state_ == State::Running)
        log::info(team_.id, std::string("running on plot ") + plot_.id);
}

void ZoneHost::update(const CityFrame& city, bool focused) {
    if (state_ != State::Running) return;
    frame_.dt = city.dt;
    frame_.time += city.dt;
    frame_.timeOfDay = city.hours;
    frame_.daylight = city.daylight;
    frame_.day = city.day;
    frame_.sunDirection = glm::normalize(plot_.directionToLocal(city.sunDirection));
    frame_.cameraPosition = plot_.worldToLocal(city.eye);
    frame_.focused = focused;
    const auto start = Clock::now();
    try {
        zone_->update(frame_);
    } catch (const std::exception& e) {
        fail("update()", e);
    } catch (...) {
        fail("update()", "unknown exception");
    }
    updateMs_ = millisecondsSince(start);
}

void ZoneHost::repairAfter(const char* where, int modelviewDepth, int attribDepth) {
    const std::string in = std::string(" in ") + where;
    // A query inside glBegin/glEnd is ignored, so the value stays -1.
    if (integer(GL_MODELVIEW_STACK_DEPTH) < 0) {
        glEnd();
        warnOnce("glBegin() without a matching glEnd()" + in);
    }
    if (integer(GL_LIST_INDEX) != 0) {
        glEndList();
        warnOnce("glNewList() without a matching glEndList()" + in);
    }
    if (const GLenum e = drainErrors(); e != GL_NO_ERROR) warnOnce(std::string("OpenGL reported ") + errorName(e) + in);

    glMatrixMode(GL_MODELVIEW);
    int depth = integer(GL_MODELVIEW_STACK_DEPTH);
    if (depth > modelviewDepth) warnOnce("glPushMatrix() without a matching glPopMatrix()" + in);
    if (depth < modelviewDepth) warnOnce("glPopMatrix() without a matching glPushMatrix()" + in);
    for (; depth > modelviewDepth; --depth) glPopMatrix();
    for (; depth < modelviewDepth; ++depth) glPushMatrix();

    int attribs = integer(GL_ATTRIB_STACK_DEPTH);
    if (attribs > attribDepth) warnOnce("glPushAttrib() without a matching glPopAttrib()" + in);
    for (; attribs > attribDepth; --attribs) glPopAttrib();
    if (attribs < attribDepth) {
        warnOnce("glPopAttrib() without a matching glPushAttrib()" + in);
        for (; attribs < attribDepth; ++attribs) glPushAttrib(GL_ALL_ATTRIB_BITS);
    }
    drainErrors();
}

void ZoneHost::draw(bool local, Probe* probe) {
    if (state_ != State::Running) return;
    drainErrors();  // errors from the city are not the zone's fault

    // Save the matrices; the zone may only change the modelview (and the
    // texture matrix, for scrolling textures).
    GLfloat view[16], projection[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, view);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    const int modelview = integer(GL_MODELVIEW_STACK_DEPTH);
    const int attribs = integer(GL_ATTRIB_STACK_DEPTH);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    for (int k = 0; k < 6; ++k) glDisable(GL_CLIP_PLANE0 + k);  // plane 5 stays free for the zone
    if (!local) {
        glTranslatef(plot_.center.x, 0.0f, plot_.center.y);
        glRotatef(plot_.yawDegrees, 0.0f, 1.0f, 0.0f);
        // Clip planes in plot-local coordinates (glClipPlane uses the current
        // modelview): x in [-w/2, w/2], z in [-d/2, d/2], y <= height.
        const double hw = plot_.width * 0.5 + 0.01, hd = plot_.depth * 0.5 + 0.01;
        const double planes[5][4] = {{1, 0, 0, hw}, {-1, 0, 0, hw}, {0, 0, 1, hd}, {0, 0, -1, hd},
                                     {0, -1, 0, layout_.plotHeight + 0.01}};
        for (int k = 0; k < 5; ++k) {
            glClipPlane(GL_CLIP_PLANE0 + k, planes[k]);
            glEnable(GL_CLIP_PLANE0 + k);
        }
    }
    GLfloat sunBefore[8];
    glGetLightfv(GL_LIGHT0, GL_POSITION, sunBefore);
    glGetLightfv(GL_LIGHT0, GL_DIFFUSE, sunBefore + 4);

    // The documented starting state of every draw() call.
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT7);
    for (int k = 1; k <= 6; ++k) glDisable(GL_LIGHT0 + k);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    const GLfloat black[4] = {0, 0, 0, 1};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, black);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_NORMALIZE);
    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glShadeModel(GL_SMOOTH);
    glLineWidth(1.0f);
    glPointSize(1.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glNormal3f(0.0f, 1.0f, 0.0f);

    const auto start = Clock::now();
    try {
        zone_->draw(*context_);
    } catch (const std::exception& e) {
        fail("draw()", e);
    } catch (...) {
        fail("draw()", "unknown exception");
    }
    drawMs_ = millisecondsSince(start);

    repairAfter("draw()", modelview + 1, attribs + 1);
    if (probe) {
        *probe = Probe{};
        for (int k = 1; k <= 7; ++k) {
            if (!glIsEnabled(GL_LIGHT0 + k)) continue;
            if (k == 7) continue;  // the city's sky fill light is on by default
            ++probe->extraLights;
            probe->highestLight = k;
        }
        GLfloat sunAfter[8];
        glGetLightfv(GL_LIGHT0, GL_POSITION, sunAfter);
        glGetLightfv(GL_LIGHT0, GL_DIFFUSE, sunAfter + 4);
        probe->sunChanged = !std::equal(sunAfter, sunAfter + 8, sunBefore) || !glIsEnabled(GL_LIGHT0);
    }
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glLoadMatrixf(view);
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    GLfloat after[16];
    glGetFloatv(GL_PROJECTION_MATRIX, after);
    if (!std::equal(after, after + 16, projection)) {
        warnOnce("draw() changed the projection matrix; the city owns it");
        glLoadMatrixf(projection);
    }
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
    drainErrors();
}

void ZoneHost::reset() {
    if (state_ != State::Running) {
        log::info(team_.id, "restarting");
        start();
        return;
    }
    frame_.time = 0.0;
    try {
        zone_->reset();
        log::info(team_.id, "reset");
    } catch (const std::exception& e) {
        fail("reset()", e);
    } catch (...) {
        fail("reset()", "unknown exception");
    }
}

void ZoneHost::key(int key) {
    if (state_ != State::Running) return;
    try {
        zone_->onKey(key);
    } catch (const std::exception& e) {
        fail("onKey()", e);
    } catch (...) {
        fail("onKey()", "unknown exception");
    }
}

}  // namespace citylab
