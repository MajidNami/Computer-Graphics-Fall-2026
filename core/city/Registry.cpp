#include "city/Registry.h"

#include <regex>
#include <set>

#include "engine/Json.h"
#include "engine/Log.h"

namespace citylab {

bool readTeamJson(const std::filesystem::path& folder, TeamInfo& team, std::string& error) {
    const auto file = folder / "team.json";
    if (!std::filesystem::exists(file)) {
        error = file.string() + " is missing";
        return false;
    }
    try {
        const Json j = Json::parseFile(file);
        // Zones are written in C++. "language" is optional; any other value is a mistake.
        if (const std::string lang = j["language"].str("cpp"); lang != "cpp") {
            error = file.string() + ": \"language\" is \"" + lang + "\"; zones are written in C++ (\"cpp\")";
            return false;
        }
        team.title = j["title"].str();
        team.titleEn = j["title_en"].str();
        if (team.place.empty()) team.place = j["place"].str();  // used until the team is registered
        const std::string declared = j["id"].str();
        if (!declared.empty() && declared != team.id) {
            error = file.string() + ": \"id\" is \"" + declared + "\" but the folder is " + team.id;
            return false;
        }
        team.folder = std::filesystem::absolute(folder);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

Registry Registry::load(const std::filesystem::path& root, const Layout& layout) {
    Registry registry;
    const Json j = Json::parseFile(root / "city" / "registry.json");
    const std::regex teamPattern("team[0-9]{2}");
    std::set<std::string> usedPlots;

    for (const auto& t : j["teams"].items()) {
        TeamInfo team;
        team.id = t["id"].str();
        team.plotId = t["plot"].str();
        team.place = t["place"].str();
        team.registered = true;
        for (const auto& g : t["github"].items()) team.github.push_back(g.str());

        if (!std::regex_match(team.id, teamPattern)) {
            log::error("registry", "team id '" + team.id + "' must look like team07");
            continue;
        }
        // A team picks a topic; the topic decides the plot. The registry may
        // also name a plot directly (a team given a free plot).
        if (team.plotId.empty()) {
            const PlotLayout* plot = layout.plotOfPlace(team.place);
            if (!plot) {
                log::error("registry", team.id + ": no plot has the topic '" + team.place +
                                           "' (see city/places.json)");
                continue;
            }
            team.plotId = plot->id;
        } else if (const PlotLayout* plot = layout.findPlot(team.plotId)) {
            if (team.place.empty()) team.place = plot->place;
        } else {
            log::error("registry", team.id + ": unknown plot '" + team.plotId + "'");
            continue;
        }
        if (!usedPlots.insert(team.plotId).second) {
            log::error("registry", team.id + ": plot " + team.plotId + " is already taken");
            continue;
        }
        std::string error;
        if (!readTeamJson(root / "teams" / team.id, team, error)) {
            log::warn("registry", team.id + ": " + error + " (the plot stays empty)");
            team.folder.clear();
        }
        registry.teams_.push_back(std::move(team));
    }
    return registry;
}

const TeamInfo* Registry::findTeam(const std::string& id) const {
    for (const auto& t : teams_)
        if (t.id == id) return &t;
    return nullptr;
}

const TeamInfo* Registry::teamOnPlot(const std::string& plotId) const {
    for (const auto& t : teams_)
        if (t.plotId == plotId) return &t;
    return nullptr;
}

const TeamInfo* Registry::ensurePreview(const std::string& id, const std::filesystem::path& root,
                                        const Layout& layout) {
    if (const TeamInfo* existing = findTeam(id)) return existing;
    const auto folder = root / "teams" / id;
    if (!std::filesystem::is_directory(folder)) return nullptr;

    TeamInfo team;
    team.id = id;
    std::string error;
    if (!readTeamJson(folder, team, error)) {
        log::error(id, error);
        return nullptr;
    }
    // The topic named in team.json, if its plot is still free; otherwise the
    // first free plot that has no topic, then any free plot.
    if (const PlotLayout* plot = layout.plotOfPlace(team.place); plot && !teamOnPlot(plot->id))
        team.plotId = plot->id;
    for (const bool topicless : {true, false}) {
        for (const auto& plot : layout.plots) {
            if (!team.plotId.empty()) break;
            if ((topicless && !plot.place.empty()) || teamOnPlot(plot.id)) continue;
            team.plotId = plot.id;
        }
    }
    if (team.plotId.empty()) {
        log::error(id, "no free plot to preview on");
        return nullptr;
    }
    log::warn(id, "not in city/registry.json yet; previewing on free plot " + team.plotId);
    teams_.push_back(std::move(team));
    return &teams_.back();
}

}  // namespace citylab
