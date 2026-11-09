#pragma once
#include "mod/confirmed_prefix.hpp"
#include "mod/work_stop.hpp"
#include <atomic>
#include <mutex>

namespace p1::save_stop {
using confirmed_prefix::Owner;
using confirmed_prefix::Snapshot;

inline bool sameSession(const Owner& a, const Owner& b) {
    return work_stop::sameOwner(a, b);
}
inline bool samePrefix(const Snapshot& proof, const std::vector<InputCmd>& source) {
    auto end = std::find_if(source.begin(), source.end(),
        [&](const InputCmd& in) { return in.step >= proof.boundary; });
    return static_cast<size_t>(end - source.begin()) == proof.inputs.size()
        && std::equal(source.begin(), end, proof.inputs.begin(), [](const InputCmd& a, const InputCmd& b) {
            return a.step == b.step && a.down == b.down && a.player1 == b.player1;
        });
}

struct Revisit {
    long long tick = -1;
    std::string reason;
    std::optional<Snapshot> source;
    long long through = 0; // required live boundary; 0 is a legacy, unqualified endpoint
};
inline std::optional<Revisit> pendingRevisit;

inline Revisit bindIntent(const Owner& owner, long long tick, const char* reason,
                    const std::vector<InputCmd>& source, std::optional<Snapshot> proof, long long through) {
    if (proof && (!sameSession(proof->owner,owner) || tick>=proof->boundary || !samePrefix(*proof,source))) proof.reset();
    if (proof) through=std::max(through,proof->boundary+1);
    return {tick,reason,std::move(proof),through};
}
inline bool sameIntent(const Revisit& a,const Revisit& b) {
    return a.tick==b.tick && a.reason==b.reason && a.through==b.through && a.source && b.source
        && a.source->boundary==b.source->boundary && a.source->heldMask==b.source->heldMask
        && samePrefix(*a.source,b.source->inputs);
}
struct Resolution { Revisit intent; Snapshot witness; };
inline bool accepted(const Resolution& r) {
    return r.intent.source && r.intent.through>r.intent.source->boundary && r.witness.owner.layer!=0
        && sameSession(r.intent.source->owner,r.witness.owner) && r.witness.boundary>=r.intent.through;
}
inline std::optional<Resolution> observe(const Snapshot& proof) {
    if (!pendingRevisit) return std::nullopt;
    Resolution resolution{*pendingRevisit,proof};
    if (!accepted(resolution)) return std::nullopt;
    pendingRevisit.reset();
    return resolution;
}

struct WorkerSource {
    uint64_t job = 0;
    Owner owner;
    bool enabled = false;
    std::optional<Snapshot> proof;
    std::optional<Revisit> revisit; // immutable restored P1 priority, never read from the worker's session globals
};
struct WorkerMessage { WorkerSource origin; Revisit intent; };
class WorkerMailbox {
    std::mutex m_mutex;
    std::optional<WorkerMessage> m_message;
public:
    void publish(WorkerMessage message) { std::lock_guard lock(m_mutex); m_message=std::move(message); }
    std::optional<WorkerMessage> take() { std::lock_guard lock(m_mutex); auto message=std::move(m_message); m_message.reset(); return message; }
};
inline WorkerMailbox workerMailbox;
inline WorkerSource workerSource; // main-thread job identity; copied into the worker
inline uint64_t nextWorker = 0;
inline thread_local std::optional<WorkerSource> activeWorker;
inline thread_local bool workerPublished = false;
inline void publishWorker(long long tick,long long through,const std::vector<InputCmd>& source) {
    if (!activeWorker || !activeWorker->enabled || workerPublished) return;
    workerPublished=true;
    workerMailbox.publish({*activeWorker,bindIntent(activeWorker->owner,tick,"input_anchor",source,activeWorker->proof,through)});
}

// Issued on the main thread, before a worker can consume the ordinary anchor hint.
inline void remember(const Owner& owner, long long tick, const char* reason,
                     const std::vector<InputCmd>& source, std::optional<Snapshot> proof, long long through = 0) {
    if (tick < 0) return;
    pendingRevisit=bindIntent(owner,tick,reason,source,std::move(proof),through);
}

enum class Phase { Idle, Waiting, Ready, Done, Refused, Cancelled };
struct Request {
    Phase phase = Phase::Idle;
    Owner owner; // generation after invalidating old work; attempt is provenance only
    std::optional<Snapshot> latest, deepest;
    std::optional<Revisit> revisit;
    bool workerAtRequest = false, nativeAtRequest = false;
    int waitFrames = 0;
    uint64_t workerJob = 0;
    work_stop::Barrier quiescence;
    const char* reason = "idle";

    bool active() const { return phase == Phase::Waiting || phase == Phase::Ready; }
    bool owns(const Owner& now) const { return sameSession(owner, now); }
    bool begin(const Owner& before, std::optional<Snapshot> last, std::optional<Snapshot> best,
               const std::optional<Revisit>& intent, bool worker, bool native, bool requirePrefix = true,
               uint64_t job = 0) {
        if (phase != Phase::Idle) return false;
        if (last && !sameSession(last->owner, before)) last.reset();
        if (best && !sameSession(best->owner, before)) best.reset();
        if ((requirePrefix && !last && !best) || (intent && (!intent->source || !sameSession(intent->source->owner, before)))) {
            phase = Phase::Refused;
            reason = intent ? "unbound_revisit" : "no_confirmed_prefix";
            return false;
        }
        owner = before;
        ++owner.generation;
        latest = std::move(last);
        deepest = std::move(best);
        revisit = intent;
        workerAtRequest = worker;
        nativeAtRequest = native;
        workerJob = job;
        if (!quiescence.request(owner, job)) return false;
        phase = Phase::Waiting;
        reason = "waiting";
        return true;
    }
    bool acknowledge(const Owner& now, bool workerStopped, bool nativeStopped, uint64_t completedJob = 0) {
        if (phase != Phase::Waiting) return false;
        if (!owns(now)) { phase = Phase::Cancelled; reason = "owner_changed"; return false; }
        ++waitFrames;
        if (!quiescence.acknowledge(now, completedJob, workerStopped, nativeStopped)) return false;
        phase = Phase::Ready;
        reason = "quiescent";
        return true;
    }
};
inline Request current;
// Sticky across checkpoint retries. Only a new worker, on the main thread, clears it.
inline std::atomic<bool>& workerCancel = work_stop::cancelRequested;
inline std::atomic<bool> workerEntered{false};
inline void clearSession() { current = {}; pendingRevisit.reset(); }
} // namespace p1::save_stop
