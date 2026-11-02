#pragma once

#include <filesystem>
#include <string>

namespace citylab {

// Saves the current back buffer as a PNG. Returns false and sets `error`
// if the file cannot be written.
bool saveScreenshot(const std::filesystem::path& file, int width, int height, std::string* error = nullptr);

// "screenshots/citylab_2026-10-29_17-30-05.png" under `root`.
std::filesystem::path timestampedScreenshotPath(const std::filesystem::path& root, const std::string& prefix);

}  // namespace citylab
