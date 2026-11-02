#pragma once

#include <filesystem>

#include "engine/Options.h"

namespace citylab {

// citylab --check team07
//
// Runs one zone alone and measures it the way the graders will: it starts,
// stays inside its plot, keeps to the budgets, uses its lights correctly,
// and is fast enough. Writes build/check/team07.json and four screenshots
// to build/check/team07/, prints a summary, and returns 0 when nothing
// failed (warnings are allowed).
int runChecker(const Options& options, const std::filesystem::path& root);

}  // namespace citylab
