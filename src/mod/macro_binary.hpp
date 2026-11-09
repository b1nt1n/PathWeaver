#pragma once
// Standard GDR1 MessagePack and pinned GDReplayFormat GDR2, with native P1/P2 channels.
#include "mod/macro_gdr.hpp"
#include <bit>
#include <cstdint>
#include <climits>
#include <gdr/gdr.hpp>

namespace p1::macro {
inline bool binaryPlanValid(const std::vector<InputCmd>& edges, int tps, int levelId, bool allowEmpty = false) {
    if ((!allowEmpty && edges.empty()) || (tps != 240 && tps != 480) || levelId < 0) return false;
    int previous = -1;
    for (const auto& e : edges) {
        if (e.step < 0 || e.step < previous) return false;
        previous = e.step;
    }
    return true;
}

inline void msgpackBE(std::ostream& out, uint64_t value, int bytes) {
    for (int i = bytes - 1; i >= 0; --i) out.put(static_cast<char>(value >> (i * 8)));
}
inline void msgpackString(std::ostream& out, const std::string& text) {
    if (text.size() < 32) out.put(static_cast<char>(0xa0 | text.size()));
    else { out.put(static_cast<char>(0xdb)); msgpackBE(out, text.size(), 4); }
    out.write(text.data(), text.size());
}
inline void msgpackUInt(std::ostream& out, uint32_t value) {
    out.put(static_cast<char>(0xce)); msgpackBE(out, value, 4);
}
inline void msgpackDouble(std::ostream& out, double value) {
    out.put(static_cast<char>(0xcb)); msgpackBE(out, std::bit_cast<uint64_t>(value), 8);
}
inline void msgpackBool(std::ostream& out, bool value) {
    out.put(static_cast<char>(value ? 0xc3 : 0xc2));
}

inline bool writeGdrBinary(std::ostream& out, const std::vector<InputCmd>& edges, int tps,
                           int levelId, const std::string& name, const std::string& version,
                           bool xdBotLab = false, long long endTick = -1, bool partial = false) {
    if (!binaryPlanValid(edges, tps, levelId, partial) || (partial && endTick <= 0)) return false;
    out.put(static_cast<char>(0x8d)); // thirteen metadata/input fields
    msgpackString(out, "version"); msgpackDouble(out, 1.0);
    msgpackString(out, "gameVersion"); msgpackDouble(out, 2.2081);
    msgpackString(out, "framerate"); msgpackDouble(out, tps);
    msgpackString(out, "author"); msgpackString(out, "PathWeaver");
    msgpackString(out, "description");
    msgpackString(out, partial ? "PathWeaver partial; FRESH_ENTRY"
        : xdBotLab ? "PathWeaver xdBot Lab adapter; FRESH_ENTRY" : "PathWeaver standard native channels; FRESH_ENTRY");
    msgpackString(out, "duration"); msgpackDouble(out, (endTick >= 0 ? static_cast<double>(endTick) : edges.back().step + 1.0) / tps);
    msgpackString(out, "seed"); msgpackUInt(out, 0);
    msgpackString(out, "coins"); msgpackUInt(out, 0);
    msgpackString(out, "ldm"); msgpackBool(out, false);
    msgpackString(out, "bot"); out.put(static_cast<char>(0x82));
    msgpackString(out, "name"); msgpackString(out, "PathWeaver");
    msgpackString(out, "version"); msgpackString(out, version);
    msgpackString(out, "level"); out.put(static_cast<char>(0x82));
    msgpackString(out, "id"); msgpackUInt(out, levelId);
    msgpackString(out, "name"); msgpackString(out, name);
    msgpackString(out, "inputs"); out.put(static_cast<char>(0xdd)); msgpackBE(out, edges.size(), 4);
    for (const auto& e : edges) {
        out.put(static_cast<char>(0x84));
        msgpackString(out, "frame"); msgpackUInt(out, e.step);
        msgpackString(out, "btn"); msgpackUInt(out, 1);
        msgpackString(out, "2p"); msgpackBool(out, xdBotLab ? e.player1 : !e.player1);
        msgpackString(out, "down"); msgpackBool(out, e.down);
    }
    msgpackString(out, "frameFixes"); out.put(static_cast<char>(0x90));
    return static_cast<bool>(out);
}

// Eclipse 1.9.4 delivers file frame N at native step N-1 (frame 0 is clamped).
// Keep this consumer-specific translation out of standard GDR files.
inline bool eclipse194Plan(const std::vector<InputCmd>& native, std::vector<InputCmd>& file) {
    if (std::any_of(native.begin(), native.end(), [](const auto& e) { return e.step == INT_MAX; })) return false;
    file = native;
    for (auto& e : file) ++e.step;
    return true;
}

// GDR2 imports P1 before P2 at equal frames. Refuse an order it cannot preserve.
inline bool gdr2OrderSafe(const std::vector<InputCmd>& edges) {
    int tick = -1;
    bool sawP2 = false;
    for (const auto& e : edges) {
        if (e.step != tick) { tick = e.step; sawP2 = false; }
        if (e.player1 && sawP2) return false;
        if (!e.player1) sawP2 = true;
    }
    return true;
}

inline bool writeGdr2(std::ostream& out, const std::vector<InputCmd>& edges, int tps,
                      int levelId, const std::string& name, const std::string& version, long long endTick = -1,
                      bool partial = false) {
    if (!binaryPlanValid(edges, tps, levelId, partial) || !gdr2OrderSafe(edges) || (partial && endTick <= 0))
        return false;
    gdr::Replay<> replay("PathWeaver", 3);
    replay.author = "PathWeaver";
    replay.description = version + (partial ? "; partial; FRESH_ENTRY" : "; native channels; FRESH_ENTRY");
    replay.duration = static_cast<float>((endTick >= 0 ? static_cast<double>(endTick) : edges.back().step + 1.0) / tps);
    replay.gameVersion = 22081;
    replay.framerate = tps;
    replay.levelInfo = gdr::Level(name, static_cast<uint32_t>(levelId));
    for (const auto& e : edges) replay.inputs.emplace_back(e.step, 1, !e.player1, e.down);
    auto encoded = replay.exportData();
    if (encoded.isErr()) return false;
    auto bytes = std::move(std::move(encoded).unwrap());
    auto decoded = gdr::Replay<>::importData(std::span<uint8_t>(bytes));
    if (decoded.isErr() || decoded.unwrap().inputs.size() != edges.size()) return false;
    for (size_t i = 0; i < edges.size(); ++i) {
        const auto& actual = decoded.unwrap().inputs[i];
        if (actual.frame != edges[i].step || actual.button != 1
            || actual.player2 != !edges[i].player1 || actual.down != edges[i].down) return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return static_cast<bool>(out);
}
} // namespace p1::macro
