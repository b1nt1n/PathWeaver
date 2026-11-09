#pragma once
#include "mod/continue_store.hpp"
#include <chrono>

namespace p1::continue_session {
enum class Phase { Off, Reserved, Replaying, Searching, Stopping, Finalizing, Finished, Failed };
struct Entry {
    confirmed_prefix::Owner owner;
    bool eligible = false, consumed = false;
};
inline Entry entry;
struct Session {
    Phase phase = Phase::Off;
    std::unique_ptr<continue_store::Store> store;
    continue_store::Record reserved;
    // During an edited-level replay, reserved still contains the untouched selected head.
    std::optional<continue_store::Identity> recheckTarget;
    std::optional<confirmed_prefix::Snapshot> rechecked;
    struct ReplayDeath { long long tick; float x; size_t inputs; };
    std::optional<ReplayDeath> recheckDeath;
    confirmed_prefix::Snapshot source;
    confirmed_prefix::Owner owner;
    std::string terminal, failure;
    bool stopRequested = false, prefixVerified = false;
    std::optional<confirmed_prefix::Snapshot> latest, deepest;
    std::optional<save_stop::Revisit> revisit;
    std::optional<save_stop::Resolution> resolution;
    bool revisitDispatched = false;
    std::chrono::steady_clock::time_point started;
    bool active() const {
        return phase==Phase::Reserved || phase==Phase::Replaying || phase==Phase::Searching || phase==Phase::Stopping;
    }
    bool owns(const confirmed_prefix::Owner& now) const { return owner.entry==now.entry && owner.layer==now.layer; }
    void stop(const std::string& reason) {
        if (!active() || phase==Phase::Stopping) return;
        terminal=reason; phase=Phase::Stopping;
    }
};
inline Session current;
inline bool uiSaving = false;
inline void reset() { current=Session{}; entry={}; uiSaving=false; }
} // namespace p1::continue_session
