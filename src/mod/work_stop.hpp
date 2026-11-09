#pragma once
// Main-thread stop tickets. Cancellation is a request, never permission to free a job's data.
#include "mod/confirmed_prefix.hpp"
#include <atomic>
#include <utility>

namespace p1::work_stop {
using confirmed_prefix::Owner;
inline std::atomic<bool> cancelRequested{false};

inline bool sameOwner(const Owner& a, const Owner& b) {
    return a.entry == b.entry && a.layer == b.layer && a.generation == b.generation;
}

class Barrier {
    enum class Phase { Idle, Waiting, Ready, Done };
    Phase m_phase = Phase::Idle;
    Owner m_owner;
    uint64_t m_job = 0;
public:
    bool active() const { return m_phase == Phase::Waiting || m_phase == Phase::Ready; }
    bool matches(const Owner& now, uint64_t job) const {
        return sameOwner(m_owner, now) && m_job == job;
    }
    bool request(const Owner& owner, uint64_t job) {
        if (m_phase != Phase::Idle) return active() && matches(owner, job);
        m_owner = owner;
        m_job = job;
        m_phase = Phase::Waiting;
        cancelRequested = true; // only a new worker on the main thread clears this
        return true;
    }
    bool acknowledge(const Owner& now, uint64_t completedJob, bool workerStopped, bool nativeStopped) {
        if (m_phase != Phase::Waiting || !matches(now, completedJob)
            || !workerStopped || !nativeStopped) return false;
        m_phase = Phase::Ready;
        return true;
    }
    template<class Finalize>
    bool finalize(const Owner& now, uint64_t job, Finalize&& finish) {
        if (m_phase != Phase::Ready || !matches(now, job)) return false;
        m_phase = Phase::Done; // reentrant/repeated callbacks cannot finalize twice
        std::forward<Finalize>(finish)();
        return true;
    }
};
}
