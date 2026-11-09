#pragma once
#include "mod/continue_store.hpp"

namespace p1::partial_export {
// A copy's native flight supplies a candidate, never permission to export it.
struct Request {
    continue_store::Identity identity; // the original level to verify
    continue_store::Prefix prefix;
    std::optional<continue_store::Identity> slicedFrom;
};
struct Source {
    confirmed_prefix::Owner owner;
    continue_store::Identity identity;
    std::optional<continue_store::Identity> original;
};
inline bool matchesSource(const Request& request, const continue_store::Identity& live,
                          const std::optional<continue_store::Identity>& original = {}) {
    return request.slicedFrom ? live == *request.slicedFrom && original && *original == request.identity
        : !original && live == request.identity;
}
inline bool matchesReplay(const Request& request, const continue_store::Identity& live,
                          const std::optional<confirmed_prefix::Snapshot>& proof,
                          long long tick, size_t applied) {
    if (live != request.identity || !proof || tick <= 0 || tick != request.prefix.boundary
        || proof->boundary != tick || applied != request.prefix.inputs.size()
        || proof->heldMask != request.prefix.heldMask || proof->inputs.size() != applied) return false;
    return std::equal(proof->inputs.begin(), proof->inputs.end(), request.prefix.inputs.begin(),
        [tick](const InputCmd& a, const InputCmd& b) {
            return a.step >= 0 && a.step < tick && a.step == b.step && a.down == b.down && a.player1 == b.player1;
        });
}
} // namespace p1::partial_export
