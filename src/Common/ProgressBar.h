#ifdef _MSC_VER
#pragma once
#endif

#ifndef PROGRESS_BAR_H
#define PROGRESS_BAR_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>

#include "Logging.h"

namespace libcbct {

class ProgressBar {
public:
    ProgressBar() = default;

    explicit ProgressBar(int total) : m_total(total) {
        if (total < 0) throw InvalidArgument("Progress total must be nonnegative");
    }

    virtual ~ProgressBar() = default;

    // Any positive width is accepted. Nonpositive widths are rejected.
    void setWidth(int width) {
        if (width <= 0) throw InvalidArgument("Progress width must be positive");
        std::lock_guard<std::mutex> lock(m_mtx);
        m_width = width;
    }

    template <typename... Args>
    void setDescription(const char *format, const Args &...args) {
        const auto description = STR_FMT(format, args...);
        std::lock_guard<std::mutex> lock(m_mtx);
        m_description = description;
    }

    // Negative increments are rejected; overshooting increments stop at the total.
    void step(int n = 1) {
        if (n < 0) throw InvalidArgument("Progress increment must be nonnegative");
        std::lock_guard<std::mutex> lock(m_mtx);
        advanceLocked(n);
    }

    void finish() {
        std::lock_guard<std::mutex> lock(m_mtx);
        advanceLocked(m_total - m_step);
    }

    // With no argument, retain the total; an explicit zero starts an empty task.
    void reset() {
        std::lock_guard<std::mutex> lock(m_mtx);
        resetLocked(m_total);
    }

    void reset(int total) {
        if (total < 0) throw InvalidArgument("Progress total must be nonnegative");
        std::lock_guard<std::mutex> lock(m_mtx);
        resetLocked(total);
    }

private:
    void resetLocked(int total) {
        m_step = 0;
        m_total = total;
        m_completed = false;
        m_start = std::chrono::steady_clock::now();
    }

    // The caller owns m_mtx. Completion is printed once, including empty tasks.
    void advanceLocked(int n) {
        if (m_completed) return;
        m_step += std::min(n, m_total - m_step);
        if (m_step != m_total && m_step % std::max(1, m_total / 1000) != 0) return;

        const int percent = m_total > 0 ? static_cast<int>(100LL * m_step / m_total) : 100;
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count();
        const double remaining = m_step > 0 ? (elapsed / m_step) * (m_total - m_step) : 0.0;
        const double rate = elapsed > 0.0 ? m_step / elapsed : 0.0;
        const std::string rateText = rate < 1000.0 ? std::to_string(static_cast<int>(rate)) : "1000+";
        const int tick = m_total > 0 ? static_cast<int>(static_cast<long long>(m_width) * m_step / m_total) : m_width;
        std::string bar(tick, '=');
        if (tick < m_width) bar += ">" + std::string(m_width - tick - 1, ' ');

        std::printf("\r%s [%3d%%]|%s| %d/%d [%02.0f:%02.0f<%02.0f:%02.0f, %sit/s]",
                    m_description.c_str(), percent, bar.c_str(), m_step, m_total,
                    std::floor(elapsed / 60.0), std::floor(std::fmod(elapsed, 60.0)),
                    std::floor(remaining / 60.0), std::floor(std::fmod(remaining, 60.0)), rateText.c_str());
        m_completed = m_step == m_total;
        if (m_completed) std::printf("\n");
        std::fflush(stdout);
    }

    int m_width = 40;
    int m_step = 0;
    int m_total = 0;
    bool m_completed = false;
    std::string m_description;
    std::chrono::steady_clock::time_point m_start = std::chrono::steady_clock::now();
    std::mutex m_mtx;
};

}  // namespace libcbct

#endif  // PROGRESS_BAR_H
