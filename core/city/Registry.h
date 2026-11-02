#pragma once

#include <deque>
#include <filesystem>
#include <string>
#include <vector>

#include "city/Layout.h"

namespace citylab {

struct TeamInfo {
    std::string id;                 // "team07"
    std::string plotId;             // "CU-3", decided by the topic (or named in the registry)
    std::string place;              // topic id from city/places.json
    bool registered = false;        // listed in city/registry.json
    std::vector<std::string> github;

    // From teams/<id>/team.json (owned by the team).
    std::string title, titleEn;     // shown on the plot's sign
    std::filesystem::path folder;   // absolute path of teams/<id>
};

// city/registry.json (staff) joined with every teams/*/team.json (teams).
class Registry {
public:
    static Registry load(const std::filesystem::path& root, const Layout& layout);

    const std::deque<TeamInfo>& teams() const { return teams_; }  // deque: pointers stay valid
    const TeamInfo* findTeam(const std::string& id) const;
    const TeamInfo* teamOnPlot(const std::string& plotId) const;

    // A team folder that is not registered yet still runs locally: it is
    // previewed on the plot of the topic in its team.json ("place") if that
    // plot is free, otherwise on a free plot, so students can start before
    // the staff register them. Returns nullptr if the folder does not exist.
    const TeamInfo* ensurePreview(const std::string& id, const std::filesystem::path& root,
                                  const Layout& layout);

private:
    std::deque<TeamInfo> teams_;
};

// Reads teams/<id>/team.json into `team` (titles, topic).
// Returns false and fills `error` when the file is missing or invalid.
bool readTeamJson(const std::filesystem::path& folder, TeamInfo& team, std::string& error);

}  // namespace citylab
