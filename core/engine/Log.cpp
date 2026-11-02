#include "engine/Log.h"

#include <cstdio>

namespace citylab::log {
namespace {

std::mutex& mutex() {
    static std::mutex m;
    return m;
}

struct State {
    std::deque<Entry> history;
    double now = 0.0;
    int warnings = 0;
    int errors = 0;
};

State& state() {
    static State s;
    return s;
}

}  // namespace

void write(Level level, const std::string& tag, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex());
    State& s = state();
    const char* prefix = level == Level::Error ? "error: " : level == Level::Warning ? "warning: " : "";
    std::fprintf(level == Level::Info ? stdout : stderr, "[%s] %s%s\n", tag.c_str(), prefix,
                 message.c_str());
    std::fflush(level == Level::Info ? stdout : stderr);
    if (level == Level::Info) return;
    if (level == Level::Warning) ++s.warnings;
    if (level == Level::Error) ++s.errors;
    s.history.push_back({level, tag, message, s.now});
    while (s.history.size() > 32) s.history.pop_front();
}

void info(const std::string& tag, const std::string& message) { write(Level::Info, tag, message); }
void warn(const std::string& tag, const std::string& message) { write(Level::Warning, tag, message); }
void error(const std::string& tag, const std::string& message) { write(Level::Error, tag, message); }

std::deque<Entry> recent(double now, double seconds) {
    std::lock_guard<std::mutex> lock(mutex());
    std::deque<Entry> out;
    for (const Entry& e : state().history)
        if (now - e.time <= seconds) out.push_back(e);
    return out;
}

void setClock(double now) {
    std::lock_guard<std::mutex> lock(mutex());
    state().now = now;
}

int warningCount() {
    std::lock_guard<std::mutex> lock(mutex());
    return state().warnings;
}

int errorCount() {
    std::lock_guard<std::mutex> lock(mutex());
    return state().errors;
}

}  // namespace citylab::log
