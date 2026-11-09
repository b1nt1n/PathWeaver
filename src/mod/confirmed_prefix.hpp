#pragma once
// Main-thread observations only. This is not a saved state or a full-solution receipt.
#include "mod/input_command.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace p1::confirmed_prefix {
struct Owner {
    std::uint64_t entry = 0;
    std::uintptr_t layer = 0;
    long long attempt = 0;
    int generation = 0;
    bool sameEntry(const Owner& other) const {
        return entry == other.entry && layer == other.layer && attempt == other.attempt;
    }
    bool operator==(const Owner& other) const {
        return sameEntry(other) && generation == other.generation;
    }
};

struct Snapshot {
    Owner owner;
    std::uint64_t branch = 0;
    long long boundary = 0;
    int heldMask = 0;
    std::vector<InputCmd> inputs;
};

class Ledger {
    struct Stream {
        Owner owner;
        std::uint64_t branch;
        std::vector<InputCmd> inputs;
        long long before = -1, returned = -1;
        std::array<long long, 2> collisions{-1, -1};
        int held = 0;
    };
    struct Watermark {
        std::shared_ptr<Stream> stream;
        long long boundary = 0;
        std::size_t count = 0;
        int held = 0;
        std::optional<Snapshot> freeze() const {
            if (!stream || boundary <= 0) return std::nullopt;
            return Snapshot{stream->owner, stream->branch, boundary, held,
                std::vector<InputCmd>(stream->inputs.begin(), stream->inputs.begin() + count)};
        }
    };
    std::optional<Owner> m_reset;
    std::shared_ptr<Stream> m_current;
    Watermark m_latest, m_deepest;
    std::uint64_t m_branch = 0;
    const char* m_reason = "not_started";

    bool canClose(long long tick, bool dual) const {
        return m_current && tick > 0 && m_current->before == tick - 1
            && m_current->returned == tick - 1 && m_current->collisions[0] == tick
            && (!dual || m_current->collisions[1] == tick);
    }

    void invalidate(const char* reason) {
        m_current.reset();
        m_reason = reason;
    }
public:
    void clear() { *this = Ledger{}; }
    const char* reason() const { return m_reason; }
    std::optional<Snapshot> latest() const { return m_latest.freeze(); }
    std::optional<Snapshot> deepest() const { return m_deepest.freeze(); }

    // Only at the end of a native update, after all collision checks returned.
    std::optional<Snapshot> closedUpdate(const Owner& owner, long long tick, bool dual) const {
        if (!canClose(tick, dual) || !(m_current->owner == owner)) return std::nullopt;
        return Watermark{m_current, tick, m_current->inputs.size(), m_current->held}.freeze();
    }

    void reset(const Owner& owner, bool eligible) {
        invalidate(eligible ? "reset_armed" : "reset_unsupported");
        m_reset = eligible ? std::optional<Owner>(owner) : std::nullopt;
    }

    void beforeInput(const Owner& owner, long long tick, bool eligible,
                     bool p1Alive, bool dual, bool p2Alive) {
        if (!eligible) { invalidate("temporary_or_unsupported"); return; }
        if (!p1Alive || (dual && !p2Alive)) { invalidate("death"); return; }
        if (tick == 0 && m_reset && m_reset->sameEntry(owner)) {
            // start() may change generation after the full reset, before the first input.
            m_current = std::make_shared<Stream>(Stream{owner, ++m_branch});
            m_reset.reset();
        }
        if (!m_current || !(m_current->owner == owner)) { invalidate("owner"); return; }
        auto& stream = *m_current;
        if (tick != stream.before + 1) { invalidate("discontinuous_tick"); return; }
        if (tick > 0) {
            if (stream.returned != tick - 1) { invalidate("missing_commands_return"); return; }
            // GD updates/collides players after processCommands returns and increments g_tick.
            // Publish only at the next before-input point, never at that early increment.
            if (canClose(tick, dual)) {
                m_latest = Watermark{m_current, tick, stream.inputs.size(), stream.held};
                if (tick >= m_deepest.boundary) m_deepest = m_latest;
            }
        }
        stream.before = tick;
        m_reason = "observing";
    }

    void dispatched(const Owner& owner, long long tick, const InputCmd& command) {
        if (!m_current) return;
        if (!(m_current->owner == owner) || tick != m_current->before || command.step != tick) {
            invalidate("late_or_wrong_command");
            return;
        }
        m_current->inputs.push_back(command);
        m_current->held = applyInputMask(m_current->held, command);
    }

    void commandsReturned(const Owner& owner, long long tick, bool eligible) {
        if (!m_current) return;
        if (!eligible || !(m_current->owner == owner) || m_current->before != tick) {
            invalidate("commands_owner");
            return;
        }
        m_current->returned = tick;
    }

    void collision(const Owner& owner, long long tick, int player, bool eligible, bool alive) {
        if (!m_current) return;
        if (!eligible || !(m_current->owner == owner)) { invalidate("collision_owner"); return; }
        if (!alive) { invalidate("death"); return; }
        if (player == 1 || player == 2) m_current->collisions[player - 1] = tick;
    }
};

inline Ledger observer;
} // namespace p1::confirmed_prefix
