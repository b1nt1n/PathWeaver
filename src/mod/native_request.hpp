#pragma once
#include "input_command.hpp"
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace p1::native_request {
enum class Verdict {
    VerifiedWindow, ProposalOnly, Limited, BudgetActual, BudgetProjected,
    RestoreRefused, RestoreSuspect, ReplayMismatch, PrefixFailed,
    Exception, Cancelled, Stale
};

inline const char* name(Verdict verdict) {
    switch (verdict) {
        case Verdict::VerifiedWindow: return "VERIFIED_WINDOW";
        case Verdict::ProposalOnly: return "PROPOSAL_ONLY";
        case Verdict::Limited: return "LIMITED";
        case Verdict::BudgetActual: return "BUDGET_ACTUAL";
        case Verdict::BudgetProjected: return "BUDGET_PROJECTED";
        case Verdict::RestoreRefused: return "RESTORE_REFUSED";
        case Verdict::RestoreSuspect: return "RESTORE_SUSPECT";
        case Verdict::ReplayMismatch: return "REPLAY_MISMATCH";
        case Verdict::PrefixFailed: return "PREFIX_FAILED";
        case Verdict::Exception: return "EXCEPTION";
        case Verdict::Cancelled: return "CANCELLED";
        case Verdict::Stale: return "STALE";
    }
    return "UNKNOWN";
}

struct Request {
    unsigned long long id = 0;
    int generation = -1;
    const void* layer = nullptr;
    long long verifiedDeath = -1, requestedBase = -1, actualBase = -1;
    int horizon = 0;
    size_t cap = 0;
    double localDeadlineSeconds = 0;
    std::vector<InputCmd> source;
    int targetDepth = 0;
    double targetX = 0.0;
    std::chrono::steady_clock::time_point prefixStarted{};
    double prefixSeconds = 0., qualifySeconds = 0., flightSecondsPerTick = 0.;
    int searchedDepth = 0;
    bool active() const { return id != 0; }
    double flightSeconds() const {
        return (static_cast<double>(actualBase) + horizon + 2.) * flightSecondsPerTick;
    }
    double reserveSeconds(double jointSeconds) const {
        if (actualBase < 0 || searchedDepth <= 0 || searchedDepth > horizon
            || !(prefixSeconds > 0. && qualifySeconds > 0. && qualifySeconds <= jointSeconds
                && flightSecondsPerTick > 0.)) return 0.;
        // Each child replays the prefix and qualifies its own head. Only the
        // remaining native work is extrapolated from the joint layers reached.
        const double search = (jointSeconds - qualifySeconds) * horizon / searchedDepth;
        const double reserve = 2. * (prefixSeconds + qualifySeconds + search) + flightSeconds();
        return std::isfinite(reserve) ? reserve : 0.;
    }
};

struct Result {
    unsigned long long id = 0;
    Verdict verdict = Verdict::Limited;
    long long actualBase = -1, endBoundary = -1, steps = 0, restores = 0;
    double milliseconds = 0;
};

inline bool sameInputs(const std::vector<InputCmd>& a, const std::vector<InputCmd>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
        [](const InputCmd& x, const InputCmd& y) {
            return x.step == y.step && x.down == y.down && x.player1 == y.player1;
        });
}

// The native branch feed emits at most one edge per channel per tick.
// Preserve the raw prefix and decline windows whose fixed P2 edges it cannot reproduce.
inline bool fixedP2Masks(const std::vector<InputCmd>& source, long long base, int headHeld,
                         int count, std::vector<uint8_t>& out) {
    out.clear();
    if (base < 0 || count <= 0) return false;
    auto at = std::lower_bound(source.begin(), source.end(), base,
        [](const InputCmd& cmd, long long tick) { return cmd.step < tick; });
    if (inputMaskOfPrefix(source, at - source.begin()) != headHeld) return false;
    int held = headHeld & 2;
    out.assign(count, static_cast<uint8_t>(held));
    for (int i = 0; i < count; ++i) {
        bool changed = false;
        for (; at != source.end() && at->step - base == i; ++at) {
            if (at->player1) continue;
            if (changed || at->down == (held != 0)) {
                out.clear();
                return false;
            }
            changed = true;
            held = at->down ? 2 : 0;
        }
        out[i] = static_cast<uint8_t>(held);
    }
    return true;
}

inline bool owns(const Request& request, unsigned long long id, int generation, const void* layer) {
    return request.active() && request.id == id && request.generation == generation
        && request.layer == layer;
}

inline bool sourceMatches(const Request& request, long long verifiedDeath,
                          const std::vector<InputCmd>& best, const std::vector<InputCmd>& installedInputs) {
    return request.verifiedDeath == verifiedDeath && sameInputs(request.source, best)
        && sameInputs(request.source, installedInputs);
}

inline bool maySplice(const Request& request, const Result& result) {
    if (!request.active() || result.id != request.id || result.verdict != Verdict::VerifiedWindow
        || request.actualBase < 0 || request.horizon <= 0
        || request.actualBase != request.requestedBase || result.actualBase != request.actualBase
        || result.endBoundary <= result.actualBase) return false;
    // Commands start at base; the first resulting state is base + 2.
    const long long distance = result.endBoundary - result.actualBase;
    return distance > 1 && distance <= static_cast<long long>(request.horizon) + 1
        && (request.targetDepth <= 0 || distance - 1 >= request.targetDepth);
}
}
