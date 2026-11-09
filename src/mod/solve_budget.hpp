#pragma once
#include <chrono>
#include <cmath>
#include <limits>

namespace p1::solve_budget {
using Clock = std::chrono::steady_clock;

enum class Phase {
    Starting, Worker, Flight, Prefix, Qualification, Search, Correction,
    Leaf, FreshTransition, Fresh, Finishing
};

inline const char* name(Phase phase) {
    switch (phase) {
        case Phase::Starting: return "starting";
        case Phase::Worker: return "worker";
        case Phase::Flight: return "flight";
        case Phase::Prefix: return "prefix";
        case Phase::Qualification: return "qualification";
        case Phase::Search: return "search";
        case Phase::Correction: return "correction";
        case Phase::Leaf: return "leaf";
        case Phase::FreshTransition: return "fresh_transition";
        case Phase::Fresh: return "fresh";
        case Phase::Finishing: return "finishing";
    }
    return "unknown";
}

struct Budget {
    bool active = false, exhausted = false, awaitingFresh = false;
    unsigned long long id = 0;
    double seconds = 0.0;
    Clock::time_point started{}, deadline{};
    Phase phase = Phase::Starting, expiredPhase = Phase::Starting;

    bool begin(double limit, unsigned long long runId, Clock::time_point now = Clock::now()) {
        if (!std::isfinite(limit) || limit < 0.0) return false;
        if (limit == 0.0) { cancel(); return true; }
        using Rep = Clock::duration::rep;
        using Ticks = std::chrono::duration<long double, Clock::period>;
        const long double ticks = Ticks(std::chrono::duration<long double>(limit)).count();
        const Rep maximum = std::numeric_limits<Rep>::max();
        // Validate before converting: overflowing floating-to-integer casts are undefined.
        if (!std::isfinite(ticks) || ticks < 1.0L || ticks >= static_cast<long double>(maximum)) return false;
        const Rep delta = static_cast<Rep>(ticks);
        const Rep origin = now.time_since_epoch().count();
        if (origin > maximum - delta) return false;
        *this = Budget{};
        active = true;
        id = runId;
        seconds = limit;
        started = now;
        deadline = Clock::time_point(Clock::duration(origin + delta));
        return true;
    }

    bool check(Clock::time_point now) {
        if (active && !exhausted && now >= deadline) {
            exhausted = true;
            expiredPhase = phase;
        }
        return active && exhausted;
    }

    bool check() { return active && check(Clock::now()); }

    void cancel() { *this = Budget{}; }

    double elapsed(Clock::time_point now = Clock::now()) const {
        if (!active || now <= started) return 0.0;
        return static_cast<double>(std::chrono::duration<long double>(now.time_since_epoch()).count()
            - std::chrono::duration<long double>(started.time_since_epoch()).count());
    }

    double remaining(Clock::time_point now = Clock::now()) const {
        if (!active || exhausted || now >= deadline) return 0.0;
        return static_cast<double>(std::chrono::duration<long double>(deadline.time_since_epoch()).count()
            - std::chrono::duration<long double>(now.time_since_epoch()).count());
    }
};

inline Budget current;
inline unsigned long long nextId = 0;
inline bool nativeActive = false, nativeAborted = false;
struct Expired {};

inline void require() {
    if (nativeActive && current.active && current.check()) throw Expired{};
}

class NativeScope {
    bool m_previous;
public:
    NativeScope() : m_previous(nativeActive) { nativeActive = true; }
    ~NativeScope() { nativeActive = m_previous; }
    NativeScope(const NativeScope&) = delete;
    NativeScope& operator=(const NativeScope&) = delete;
};

class PhaseScope {
    unsigned long long m_id;
    Phase m_previous;
    bool m_active;
public:
    explicit PhaseScope(Phase phase)
        : m_id(current.id), m_previous(current.phase), m_active(current.active) {
        if (m_active) current.phase = phase;
    }
    ~PhaseScope() {
        if (m_active && current.active && current.id == m_id) current.phase = m_previous;
    }
    PhaseScope(const PhaseScope&) = delete;
    PhaseScope& operator=(const PhaseScope&) = delete;
};

// A worker owns only its copied deadline; it never reads the main-thread ledger.
inline thread_local Clock::time_point workerDeadline = Clock::time_point::max();

class WorkerScope {
    Clock::time_point m_previous;
public:
    explicit WorkerScope(Clock::time_point deadline) : m_previous(workerDeadline) { workerDeadline = deadline; }
    ~WorkerScope() { workerDeadline = m_previous; }
    WorkerScope(const WorkerScope&) = delete;
    WorkerScope& operator=(const WorkerScope&) = delete;
};

inline bool workerSpent() {
    return workerDeadline != Clock::time_point::max() && Clock::now() >= workerDeadline;
}
}
