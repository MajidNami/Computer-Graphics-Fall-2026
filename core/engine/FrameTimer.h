#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

namespace citylab {

// Turns the time a frame really took into the time the world moves by.
//
// The raw time jitters (a driver compiles something, another program wakes
// up, vsync lands a millisecond early or late), and every such wobble would
// go straight into the camera as a small jerk. Two things are done about it:
//   * a frame far longer than the ones before it is a hiccup, not a slow
//     computer: the world moves by the usual step instead of leaping;
//   * what is left is smoothed a little, so the step changes gently.
// A computer that is really slow takes long frames every time, and gets them.
class FrameTimer {
public:
    // `raw` is the seconds the last frame took; the result is what to advance by.
    float advance(float raw) {
        raw = std::clamp(raw, 0.0f, kLongest);
        if (count_ >= 4) {
            const float usual = median();
            if (raw > usual * kHiccup && raw > kHiccupMin) raw = usual;
        }
        history_[static_cast<std::size_t>(next_)] = raw;
        next_ = (next_ + 1) % kHistory;
        count_ = std::min(count_ + 1, kHistory);
        smooth_ = count_ == 1 ? raw : smooth_ + (raw - smooth_) * kFollow;
        return smooth_;
    }

private:
    static constexpr int kHistory = 8;
    static constexpr float kLongest = 0.1f;     // a longer frame is cut to this, as before
    static constexpr float kHiccup = 2.5f;      // this many times the usual frame is a hiccup ...
    static constexpr float kHiccupMin = 0.03f;  // ... if it is also longer than 30 ms
    static constexpr float kFollow = 0.25f;     // how fast the step follows the frames

    // The middle of the frames kept so far. An insertion sort of at most eight
    // numbers: as fast as std::sort here, and GCC 15 cannot mistake it for an
    // access out of bounds (it does with std::sort on a std::array).
    float median() const {
        std::array<float, kHistory> sorted = history_;
        const std::size_t n = static_cast<std::size_t>(std::clamp(count_, 1, kHistory));
        for (std::size_t i = 1; i < n; ++i) {
            const float value = sorted[i];
            std::size_t j = i;
            for (; j > 0 && sorted[j - 1] > value; --j) sorted[j] = sorted[j - 1];
            sorted[j] = value;
        }
        return sorted[n / 2];
    }

    std::array<float, kHistory> history_{};
    int next_ = 0, count_ = 0;
    float smooth_ = 0.0f;
};

}  // namespace citylab
