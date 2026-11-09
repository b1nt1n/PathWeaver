#pragma once
#include "mod/execution_receipt.hpp"
#include <Geode/utils/hash.hpp>
#include <Geode/utils/random.hpp>

namespace p1::continue_runtime {
inline std::string content(GJGameLevel* level) {
    std::string raw=level->m_levelType==GJLevelType::Main
        ? std::string(LocalLevelManager::sharedState()->getMainLevelString(level->m_levelID.value()))
        : std::string(level->m_levelString);
    const auto start=raw.find_first_not_of(" \r\n\t");
    if (start!=std::string::npos) raw.erase(0,start);
    if (raw.starts_with("H4sI")) raw=std::string(cocos2d::ZipUtils::decompressString(raw,false,0));
    return raw;
}
inline continue_store::Identity identity(GJBaseGameLayer* layer, bool coins) {
    const auto raw = content(layer->m_level);
    return {executionHash(raw), static_cast<int64_t>(raw.size()), layer->m_levelSettings->m_twoPlayerMode,
        coins, layer->m_lowDetailMode, layer->m_extraLDM};
}
inline void capture(GJBaseGameLayer* layer) {
    if (continue_store::origin || !layer->m_level || !layer->m_levelSettings) return;
    auto entry = identity(layer, g_cfg.coinFiles);
    if (entry.contentBytes == 0) return;
    if (!g_uiSession) {
        entry.lowDetail.reset();
        entry.extraLdm.reset();
    }
    continue_store::origin=continue_store::Origin{prefixProbeOwner(layer), std::move(entry)};
}
inline bool commit(GJBaseGameLayer* layer) {
    const auto& request=save_stop::current;
    auto& origin=continue_store::origin;
    if (request.phase!=save_stop::Phase::Ready || !origin || origin->owner.entry!=request.owner.entry
        || origin->owner.layer!=request.owner.layer || request.owner.layer!=reinterpret_cast<uintptr_t>(layer)
        || !layer->m_level || !layer->m_levelSettings
        || dpsolve::g_running.load() || secsolve::g_task.valid() || solve_budget::nativeActive) {
        writeResult("continue_store: refused missing_or_unowned_origin"); return false;
    }
    auto live = identity(layer, g_cfg.coinFiles);
    if (!g_uiSession) {
        live.lowDetail.reset();
        live.extraLdm.reset();
    }
    if (live != origin->identity) {
        writeResult("continue_store: refused content_changed"); return false;
    }
    const auto& budget=solve_budget::current;
    if (g_cfg.dpSolveBudget>0 && !budget.active) {
        writeResult("continue_store: refused finite_budget_unavailable"); return false;
    }
    if (origin->saveGeneration>=0 && origin->saveGeneration!=request.owner.generation) {
        writeResult("continue_store: refused first_save_ticket_changed"); return false;
    }
    const auto binary=geode::sha256File(Mod::get()->getBinaryPath());
    if (binary.isErr()) { writeResult("continue_store: refused binary_hash_failed"); return false; }
    if (origin->lineage.empty()) {
        origin->lineage=geode::utils::random::generateUUID();
        origin->record=geode::utils::random::generateUUID();
        origin->saveGeneration=request.owner.generation;
    }
    if (!origin->allowance) origin->allowance=continue_store::Allowance{
        budget.active,false,budget.active ? budget.seconds : 0,budget.active ? budget.remaining() : 0,0,"",
        g_cfg.dpMaxIters,std::max(0,dpsolve::g_iter),g_cfg.maxAttempts,std::max(0,g_finishedAttempts)};
    continue_store::Record record;
    record.lineage=origin->lineage; record.id=origin->record; record.revision=1;
    record.producer=Mod::get()->getVersion().toVString(); record.binarySha256=binary.unwrap().toString();
    record.identity=origin->identity; record.latest=request.latest; record.deepest=request.deepest; record.revisit=request.revisit;
    record.budget=*origin->allowance;
    if (g_uiSession) record.levelName=layer->m_level->m_levelName.c_str();
    continue_store::Store store(std::filesystem::path(DATA_DIR)/"continues",[]{ return geode::utils::random::generateUUID(); });
    const auto saved=store.create(std::move(record));
    if (!saved) { writeResult("continue_store: refused "+store.error); return false; }
    const auto loaded=store.load(saved->lineage);
    if (!loaded || continue_store::encode(*loaded)!=continue_store::encode(*saved)) {
        writeResult("continue_store: refused readback_failed"); return false;
    }
    writeResult("continue_store: committed lineage="+saved->lineage+" record="+saved->id
        +" revision="+std::to_string(saved->revision)+" remaining="+std::to_string(saved->budget.remaining));
    if (g_cfg.continueStoreProbe) {
        std::ofstream out(std::string(DATA_DIR)+"/continue_store_probe.json",std::ios::binary|std::ios::trunc);
        out << matjson::makeObject({{"lineage",saved->lineage},{"record",saved->id},{"revision",saved->revision},
            {"remaining",saved->budget.remaining},{"requires_fresh_recheck",true}}).dump(2) << '\n';
    }
    return true;
}
} // namespace p1::continue_runtime

namespace p1::partial_export {
inline void capture(GJBaseGameLayer* layer) {
    if (!layer->m_level || !layer->m_levelSettings || source) return;
    auto live = continue_runtime::identity(layer, g_cfg.coinFiles);
    std::optional<continue_store::Identity> original;
    if (levelslice::g_phase != levelslice::Off) {
        if (!levelslice::g_orig) return;
        original = live;
        const auto raw = continue_runtime::content(levelslice::g_orig);
        original->contentHash = executionHash(raw);
        original->contentBytes = static_cast<int64_t>(raw.size());
    }
    source = Source{prefixProbeOwner(layer), std::move(live), std::move(original)};
    writeResult("partial_export: candidate_source sliced="+std::to_string(source->original.has_value())
        +" source="+source->identity.contentHash
        +" original="+source->original.value_or(source->identity).contentHash);
}
} // namespace p1::partial_export
