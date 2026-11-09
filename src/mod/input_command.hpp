#pragma once
// Native input channels, before GD applies Flip Controls and common-dual routing.
#include <algorithm>
#include <cstdio>
#include <ostream>
#include <string>
#include <vector>

namespace p1 {
struct InputCmd { int step; bool down; bool player1 = true; };

// 0: another kind of line; 1: valid edge; -1: malformed/unsupported input.
inline int parseInputCommand(const std::string& line, InputCmd& out) {
    const bool second = line.rfind("input2=", 0) == 0;
    if (!second && line.rfind("input=", 0) != 0) return 0;
    int tick = 0, down = 0;
    char extra = 0;
    if (std::sscanf(line.c_str() + (second ? 7 : 6), "%d,%d %c", &tick, &down, &extra) != 2
        || tick < 0 || (down != 0 && down != 1)) return -1;
    out = {tick, down != 0, !second};
    return 1;
}

inline void sortInputs(std::vector<InputCmd>& inputs) {
    std::stable_sort(inputs.begin(), inputs.end(),
                     [](const InputCmd& a, const InputCmd& b) { return a.step < b.step; });
}

inline bool hasPlayer2Inputs(const std::vector<InputCmd>& inputs) {
    return std::any_of(inputs.begin(), inputs.end(), [](const InputCmd& c) { return !c.player1; });
}

// Held native channels, before GD routes them through Flip Controls.
inline int applyInputMask(int mask, const InputCmd& command) {
    const int bit = command.player1 ? 1 : 2;
    return command.down ? mask | bit : mask & ~bit;
}

inline int inputMaskOfPrefix(const std::vector<InputCmd>& inputs, size_t count) {
    int mask = 0;
    for (size_t i = 0; i < std::min(count, inputs.size()); ++i)
        mask = applyInputMask(mask, inputs[i]);
    return mask;
}

inline void appendInputMask(std::vector<InputCmd>& inputs, int tick, int before, int after) {
    if ((before ^ after) & 1) inputs.push_back({tick, (after & 1) != 0, true});
    if ((before ^ after) & 2) inputs.push_back({tick, (after & 2) != 0, false});
}

// Sample actual hold changes, newest first with exponentially older choices.
// The opening anchor also lets repair reconsider an omitted press. Sorted input
// plans are required, as for inputMaskOfPrefix; P1 and P2 remain separate channels.
inline std::vector<long long> repairInputAnchors(const std::vector<InputCmd>& inputs, long long before) {
    if (before <= 2) return {};
    std::vector<long long> changes;
    int mask = 0;
    for (const auto& c : inputs) {
        if (c.step >= before) break;
        const int next = applyInputMask(mask, c);
        if (next != mask && c.step >= 4) changes.push_back(c.step - 2);
        mask = next;
    }
    changes.erase(std::unique(changes.begin(), changes.end()), changes.end());
    std::vector<long long> out{2};
    for (size_t distance = 1; distance <= changes.size();) {
        out.push_back(changes[changes.size() - distance]);
        if (distance > changes.size() / 2) break;
        distance *= 2;
    }
    if (!changes.empty()) out.push_back(changes.front());
    std::sort(out.begin(), out.end(), [](long long a, long long b) { return a > b; });
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// One earlier input request after the first ordinary attempt. Search failure
// changes the request order; it does not prove the current entrance impossible.
// Independent P2 plans still require native search; ordinary attempts are kept.
inline long long interleaveRepairInputAnchor(std::vector<int>& rungs, size_t first,
                                             const std::vector<InputCmd>& inputs,
                                             long long death) {
    if (hasPlayer2Inputs(inputs) || first >= rungs.size()) return -1;
    const auto choices = repairInputAnchors(inputs, death - rungs[first]);
    if (choices.empty()) return -1;
    const long long at = choices.front();
    const int back = (int)(death - at);
    if (back <= rungs[first]) return -1;
    auto next = rungs.begin() + (std::ptrdiff_t)(first + 1);
    auto existing = std::find(next, rungs.end(), back);
    if (existing != rungs.end()) {
        if (existing == next) return -1;
        rungs.erase(existing);
    }
    rungs.insert(rungs.begin() + (std::ptrdiff_t)(first + 1), back);
    return at;
}

inline void writeInputCommand(std::ostream& out, const InputCmd& c) {
    out << (c.player1 ? "input=" : "input2=") << c.step << ',' << (c.down ? 1 : 0) << '\n';
}
} // namespace p1
