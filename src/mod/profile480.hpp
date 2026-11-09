#pragma once
// Eclipse's published API is a build dependency, not vendored source.
#include <eclipse/config.hpp>

namespace p1::profile480 {
inline bool prepare() {
    if (!g_cfg.profile480) return true;
    auto* mod = geode::Loader::get()->getLoadedMod("eclipse.eclipse-menu");
    if (!mod || mod->getVersion().toVString() != "v1.9.4") {
        writeResult("profile480: refused - Eclipse 1.9.4 must own the timebase");
        return false;
    }
    if (g_cfg.dpSolve || g_serveMode || secsolve::g_on || !g_prelude.empty()
        || g_cfg.practiceAt >= 0 || g_cfg.checkpointAt >= 0 || g_cfg.restoreAt >= 0
        || !g_cfg.snapAt.empty() || g_cfg.fastdt > 0 || g_cfg.framedt > 0
        || g_cfg.cbs != 0 || g_cfg.cos != 1 || g_cfg.maxAttempts != 1
        || itermap::g_seekCpEvery != 0 || !g_cfg.toggles.empty()) {
        writeResult("profile480: refused - continuous FRESH_ENTRY replay only; no search/restore/time overrides");
        return false;
    }
    if (!g_cfg.externalReplay.empty() && (!g_cfg.inputs.empty() || g_cfg.blockInput)) {
        writeResult("profile480: refused - external playback requires empty internal inputs and blockinput=0");
        return false;
    }
    using namespace eclipse::config;
    // Eclipse's own modules read the internal store. The public session store
    // is separate and changing it alone does not activate the replay bot.
    // The isolated launch profile owns the physics settings; never rewrite them.
    if (!getInternal<bool>("global.tpsbypass.toggle", false)
        || getInternal<double>("global.tpsbypass", 0) != 480.0
        || getInternal<bool>("global.speedhack.toggle", true)
        || getInternal<int>("bot.practice-fix-mode", -1) != 0) {
        writeResult("profile480: refused - configure the isolated Eclipse profile for 480, speedhack off, checkpoint-only fixes");
        return false;
    }
    if (getInternal<int>("bot.state", -1) != 0) {
        writeResult("profile480: refused - Eclipse Replay Bot must be Disabled; Eclipse owns timing only");
        return false;
    }
    if (!g_cfg.externalReplay.empty()) {
        auto* player = geode::Loader::get()->getLoadedMod("zilko.xdbot");
        if (!player || player->getVersion().toVString() != "v2.6.0-prerelease.4801"
            || player->getSettingValue<std::string>("macro_accuracy") != "Vanilla"
            || player->getSettingValue<int64_t>("frame_offset") != 0
            || player->getSettingValue<bool>("lock_delta")) {
            writeResult("profile480: refused - external replay requires xdBot 2.6.0-prerelease.4801, Vanilla, offset 0, lock delta off");
            return false;
        }
    }
    writeResult(std::string("profile480: Eclipse 1.9.4; TPS=480; checkpoint-only fixes, no checkpoints; owner=")
        + (g_cfg.externalReplay.empty() ? "PathWeaver native queue" : "xdBot native queue"));
    return true;
}
}
