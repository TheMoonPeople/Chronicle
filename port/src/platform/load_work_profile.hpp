#pragma once

#include <array>
#include <chrono>
#include <cstdint>

namespace dc::platform::load_profile {

/**
 * The kinds of loading work timed on the calling thread.
 */
enum class Kind {
    BackgroundRead,
    Metadata,
    Png,
    Texture,
    Visual,
    Mesh,
    Count
};

/**
 * How often one kind of work ran and the wall time it took.
 */
struct Counter {
    std::uint64_t calls = 0;       /**< Spans of this kind that finished while profiling was active. */
    std::uint64_t nanoseconds = 0; /**< Their summed wall time. */
};

/** Whether spans on this thread are being timed. */
inline thread_local bool active = false;

/** The per-kind totals of this thread. */
inline thread_local std::array<Counter, static_cast<unsigned>(Kind::Count)> counters;

/**
 * Turns timing on for this thread for its lifetime. Spans are nested wall spans on the calling
 * thread, not GPU execution timings.
 */
class Activation {
public:
    Activation() : previous(active) { active = true; }

    ~Activation() { active = previous; }

private:
    bool previous;
};

/**
 * Adds its lifetime to kind's counter when timing was active at its construction.
 */
class Span {
public:
    explicit Span(Kind kind) : kind(kind), enabled(active) {
        if (enabled) {
            started = Clock::now();
        }
    }

    ~Span() {
        if (enabled) {
            const auto elapsed = Clock::now() - started;
            auto      &counter = counters[static_cast<unsigned>(kind)];
            ++counter.calls;
            counter.nanoseconds += std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
        }
    }

private:
    using Clock = std::chrono::steady_clock;
    Kind              kind;
    bool              enabled;
    Clock::time_point started;
};

/**
 * The milliseconds kind has taken since the counters were copied into before.
 */
inline double MillisecondsSince(const decltype(counters) &before, Kind kind) {
    const auto index = static_cast<unsigned>(kind);
    return static_cast<double>(counters[index].nanoseconds - before[index].nanoseconds) / 1'000'000.0;
}

} // namespace dc::platform::load_profile
