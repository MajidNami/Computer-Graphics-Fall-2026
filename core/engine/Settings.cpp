#include "engine/Settings.h"

#include <algorithm>
#include <fstream>

#include "engine/Json.h"
#include "engine/Log.h"

namespace citylab {

Settings Settings::load(const std::filesystem::path& file, bool create) {
    Settings s;
    std::error_code ec;
    if (std::filesystem::exists(file, ec)) {
        try {
            const Json j = Json::parseFile(file);
            s.mouseSensitivity = std::clamp(j["mouse_sensitivity"].f(s.mouseSensitivity), 0.1f, 5.0f);
            s.invertMouse = j["invert_mouse"].boolean(s.invertMouse);
            s.fieldOfView = std::clamp(j["field_of_view"].f(s.fieldOfView), 40.0f, 100.0f);
            s.walkSpeed = std::clamp(j["walk_speed"].f(s.walkSpeed), 1.0f, 30.0f);
            s.runSpeed = std::clamp(j["run_speed"].f(s.runSpeed), 1.0f, 60.0f);
            s.flySpeed = std::clamp(j["fly_speed"].f(s.flySpeed), 1.0f, 300.0f);
            s.headBob = j["head_bob"].boolean(s.headBob);
            s.vsync = j["vsync"].boolean(s.vsync);
            s.walkThroughObjects = j["walk_through_objects"].boolean(s.walkThroughObjects);
            const int samples = static_cast<int>(j["msaa"].f(static_cast<float>(s.msaa)));
            s.msaa = samples >= 8 ? 8 : samples >= 4 ? 4 : samples >= 2 ? 2 : 0;
        } catch (const std::exception& e) {
            log::warn("settings", std::string(e.what()) + "; using the defaults");
        }
        return s;
    }
    if (create) {
        Json j = Json::object();
        j.set("mouse_sensitivity", s.mouseSensitivity);
        j.set("invert_mouse", s.invertMouse);
        j.set("field_of_view", s.fieldOfView);
        j.set("walk_speed", s.walkSpeed);
        j.set("run_speed", s.runSpeed);
        j.set("fly_speed", s.flySpeed);
        j.set("head_bob", s.headBob);
        j.set("vsync", s.vsync);
        j.set("msaa", s.msaa);
        j.set("walk_through_objects", s.walkThroughObjects);
        std::ofstream out(file, std::ios::binary);
        if (out) out << j.dump(2) << "\n";
    }
    return s;
}

}  // namespace citylab
