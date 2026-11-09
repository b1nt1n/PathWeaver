#pragma once
// Read-only G2-A observations. g_tick remains the executor's only step counter.
#include "mod/trace_io.hpp"
#include <iomanip>

namespace p1::diagnostic {
inline std::ofstream stream;
inline unsigned long long sequence = 0, epoch = 0;

inline bool active(GJBaseGameLayer* layer) {
    return g_cfg.diagnosticTrace && g_started && !g_sessionOver
        && layer && layer == static_cast<GJBaseGameLayer*>(PlayLayer::get());
}

inline int playerNumber(GJBaseGameLayer* layer, PlayerObject* p) {
    return p && layer ? (p == layer->m_player1 ? 1 : p == layer->m_player2 ? 2 : 0) : 0;
}

inline int uid(GameObject* object) { return object ? object->m_uniqueID : -1; }

inline void player(PlayerObject* p) {
    if (!p) { stream << "null"; return; }
    const auto held = p->m_holdingButtons.find(1);
    stream << "{\"x\":" << p->getPositionX() << ",\"y\":" << p->getPositionY()
           << ",\"frame_cache\":{\"stored_x\":" << p->m_position.x << ",\"stored_y\":" << p->m_position.y
           << ",\"rect_x\":" << p->m_objectRect.origin.x << ",\"rect_y\":" << p->m_objectRect.origin.y
           << ",\"rect_w\":" << p->m_objectRect.size.width << ",\"rect_h\":" << p->m_objectRect.size.height << '}';
#define PW_DIAG_FIELD(name) stream << ",\"" #name "\":" << +p->m_##name
    PW_DIAG_FIELD(yVelocity); PW_DIAG_FIELD(platformerXVelocity);
    PW_DIAG_FIELD(playerSpeed); PW_DIAG_FIELD(vehicleSize); PW_DIAG_FIELD(gravityMod);
    PW_DIAG_FIELD(isShip); PW_DIAG_FIELD(isBird); PW_DIAG_FIELD(isBall);
    PW_DIAG_FIELD(isDart); PW_DIAG_FIELD(isRobot); PW_DIAG_FIELD(isSpider); PW_DIAG_FIELD(isSwing);
    PW_DIAG_FIELD(isUpsideDown); PW_DIAG_FIELD(isGoingLeft); PW_DIAG_FIELD(isSideways);
    PW_DIAG_FIELD(isOnGround); PW_DIAG_FIELD(isOnGround2); PW_DIAG_FIELD(isOnSlope);
    PW_DIAG_FIELD(wasOnSlope); PW_DIAG_FIELD(slopeVelocity);
    PW_DIAG_FIELD(isDead); PW_DIAG_FIELD(isLocked); PW_DIAG_FIELD(controlsDisabled);
    PW_DIAG_FIELD(inputsLocked); PW_DIAG_FIELD(isPlatformer);
    PW_DIAG_FIELD(jumpBuffered); PW_DIAG_FIELD(wasJumpBuffered); PW_DIAG_FIELD(stateJumpBuffered);
    PW_DIAG_FIELD(stateRingJump); PW_DIAG_FIELD(stateRingJump2);
    PW_DIAG_FIELD(accelerationOrSpeed); PW_DIAG_FIELD(isAccelerating);
    PW_DIAG_FIELD(wasRobotJump); PW_DIAG_FIELD(fixRobotJump);
    PW_DIAG_FIELD(touchedPad); PW_DIAG_FIELD(touchedRing); PW_DIAG_FIELD(touchedGravityPortal);
    PW_DIAG_FIELD(padRingRelated); PW_DIAG_FIELD(isDashing);
    PW_DIAG_FIELD(stateDartSlide);
    PW_DIAG_FIELD(dashX); PW_DIAG_FIELD(dashY); PW_DIAG_FIELD(dashAngle); PW_DIAG_FIELD(dashStartTime);
#undef PW_DIAG_FIELD
    stream << ",\"held_jump\":" << (held != p->m_holdingButtons.end() && held->second ? 1 : 0)
           << ",\"dash_uid\":" << uid(p->m_dashRing)
           << ",\"portal_uid\":" << uid(p->m_lastActivatedPortal)
           << ",\"snapped_uid\":" << uid(p->m_objectSnappedTo)
           << ",\"touched_rings\":[";
    // Native unordered iteration is not a stable cross-process representation.
    std::vector<int> rings(p->m_touchedRings.begin(), p->m_touchedRings.end());
    std::sort(rings.begin(), rings.end());
    for (size_t i = 0; i < rings.size(); ++i) stream << (i ? "," : "") << rings[i];
    stream << "]}";
}

inline void emit(GJBaseGameLayer* layer, const char* phase, PlayerObject* who = nullptr,
                 double dt = 0, int a = 0, int b = 0, bool state = false, int sourcePlayer = -1) {
    if (!active(layer) || (who && playerNumber(layer, who) == 0)) return;
    if (!stream.is_open()) {
        stream.open(std::string(DATA_DIR) + "/diagnostic.jsonl", std::ios::trunc);
        sequence = epoch = 0;
        stream << "{\"schema\":\"PathWeaver.diagnostic.v1\",\"kind\":\"header\","
                  "\"step_counter\":\"g_tick\",\"input_phase\":\""
               << (g_cfg.profile480 ? "before processQueuedButtons" : "before processCommands") << "\","
                  "\"state_phase\":\"before_input; collision_post is per-player\","
                  "\"target\":\"GD 2.2081 / Geode 5.11.2\",\"flip_controls\":"
               << (GameManager::sharedState()->getGameVariable("0010") ? 1 : 0)
               << ",\"two_player\":" << (layer->m_levelSettings && layer->m_levelSettings->m_twoPlayerMode ? 1 : 0);
        if (g_cfg.profile480) stream << ",\"profile\":\"PathWeaver.480.v1\",\"delivery\":\"native processQueuedButtons\"";
        stream << "}\n";
        stream << std::setprecision(17);
    }
    stream << "{\"kind\":\"observation\",\"seq\":" << ++sequence << ",\"epoch\":" << epoch
           << ",\"attempt\":" << g_attempt << ",\"tick\":" << g_tick
           << ",\"phase\":\"" << phase << "\",\"player\":" << playerNumber(layer, who)
           << ",\"dt\":" << dt << ",\"a\":" << a << ",\"b\":" << b;
    if (sourcePlayer >= 0) stream << ",\"source_player\":" << sourcePlayer;
    if (state) {
        stream << ",\"world\":{\"dual\":" << (layer->m_gameState.m_isDualMode ? 1 : 0)
               << ",\"level_time\":" << layer->m_gameState.m_levelTime
               << ",\"progress\":" << layer->m_gameState.m_currentProgress
               << ",\"time_warp\":" << layer->m_gameState.m_timeWarp;
        if (g_cfg.profile480) {
            const auto base = geode::base::get();
            stream << ",\"rng_trigger\":" << *reinterpret_cast<const long long*>(base + gdoff::kSeedTriggerRva)
                   << ",\"rng_index\":" << *reinterpret_cast<const long long*>(base + gdoff::kSeedVarIndexRva)
                   << ",\"rng_table\":" << *reinterpret_cast<const long long*>(base + gdoff::kSeedVarTableRva);
        }
        stream << "},\"p1\":";
        player(layer->m_player1);
        stream << ",\"p2\":"; player(layer->m_player2);
    }
    stream << "}\n";
}

inline void reset(GJBaseGameLayer* layer) {
    if (!active(layer)) return;
    emit(layer, "reset_begin");
    ++epoch;
}

inline void finish(const std::string& why) {
    if (!stream.is_open()) return;
    stream << "{\"kind\":\"footer\",\"observations\":" << sequence
           << ",\"reason\":" << matjson::Value(why).dump() << "}\n";
    stream.close();
    if (suite::active()) {
        std::error_code ec;
        std::filesystem::copy_file(std::string(DATA_DIR) + "/diagnostic.jsonl",
            std::string(DATA_DIR) + "/diagnostic_suite" + std::to_string(suite::g_at + 1) + ".jsonl",
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) log::error("PathWeaver: cannot archive suite diagnostic: {}", ec.message());
    }
}
} // namespace p1::diagnostic
