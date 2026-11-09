#pragma once
#include "mod/work_stop.hpp"
#include <functional>

namespace p1::stop_transition {
enum class Kind { End, Switch, Leave };

// One main-thread intent for one stop ticket. Save proof stays in save_stop::Request.
class Transition {
    work_stop::Barrier m_barrier;
    Kind m_kind = Kind::End;
    std::function<void()> m_action;
    bool m_ready = false, m_issued = false, m_suspended = false;
public:
    bool pending() const { return !m_issued && static_cast<bool>(m_action); }
    bool suspended() const { return m_suspended; }
    bool request(const confirmed_prefix::Owner& owner, uint64_t job, Kind kind, std::function<void()> action) {
        if (m_issued || !m_barrier.request(owner, job)) return false;
        if (!m_action || kind >= m_kind) { m_kind = kind; m_action = std::move(action); }
        m_suspended = false; // an explicit retry may complete an already acknowledged ticket
        return true;
    }
    bool acknowledge(const confirmed_prefix::Owner& owner, uint64_t job, bool workerStopped, bool nativeStopped) {
        if (!workerStopped || !nativeStopped || !m_barrier.matches(owner, job)) return false;
        if (!m_ready) m_ready = m_barrier.acknowledge(owner, job, true, true);
        return m_ready;
    }
    bool suspend() {
        if (!pending() || m_suspended) return false;
        m_suspended = true;
        return true;
    }
    bool finalize(const confirmed_prefix::Owner& owner, uint64_t job) {
        if (m_suspended || !m_ready) return false;
        return m_barrier.finalize(owner, job, [&] {
            m_issued = true;
            auto action = std::move(m_action);
            action();
        });
    }
};
} // namespace p1::stop_transition
