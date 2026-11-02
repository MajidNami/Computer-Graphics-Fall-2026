#pragma once

#include <deque>
#include <mutex>
#include <string>

namespace citylab {

// Console logging with a short on-screen history for the HUD.
//
//     log::info("city", "loaded 36 plots");
//     log::warn("team07", "texture is 4096 px wide");
//
// Warnings and errors also appear for a few seconds at the top of the window,
// because students often do not watch the terminal.
namespace log {

enum class Level { Info, Warning, Error };

struct Entry {
    Level level;
    std::string tag;
    std::string message;
    double time;  // seconds since start, filled in by the logger
};

void info(const std::string& tag, const std::string& message);
void warn(const std::string& tag, const std::string& message);
void error(const std::string& tag, const std::string& message);
void write(Level level, const std::string& tag, const std::string& message);

// Warnings and errors from the last `seconds`, newest last.
std::deque<Entry> recent(double now, double seconds);
void setClock(double now);  // the logger stamps entries with this time

int warningCount();
int errorCount();

}  // namespace log
}  // namespace citylab
