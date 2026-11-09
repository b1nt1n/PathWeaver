#include "mod/playlayer_helpers.hpp"
#include "mod/continue_runtime.hpp"

namespace p1::continue_runtime {
using continue_session::Phase;

static bool refuse(const std::string& reason) {
    continue_session::current.phase=Phase::Failed;
    writeResult("continue_resume: REFUSED "+reason);
    endSession("continue_refused_"+reason);
    if (g_uiSession) notify::show("PathWeaver: could not continue the saved search ("+reason+")",
        NotificationIcon::Warning, 5.f);
    return false;
}

bool canSaveFromPause(PlayLayer* layer) {
    if (!layer || layer!=PlayLayer::get() || !g_uiSession || !g_started || g_sessionOver
        || !g_cfg.dpSolve || g_dpShowSolution || continue_session::uiSaving) return false;
    const auto owner=prefixProbeOwner(layer);
    const auto& request=save_stop::current;
    if (request.phase==save_stop::Phase::Ready) return request.owns(owner);
    if (request.active() || (continue_session::current.phase!=Phase::Off
        && continue_session::current.phase!=Phase::Searching)) return false;
    const auto& origin=continue_store::origin;
    if (!origin || origin->owner.entry!=owner.entry || origin->owner.layer!=owner.layer
        || levelslice::g_phase!=levelslice::Off || dpsolve::levelHasMovingParts()) return false;
    const auto proof=confirmed_prefix::observer.deepest();
    return proof && save_stop::sameSession(proof->owner, owner);
}

static bool canSelectSave(PlayLayer* layer) {
    return layer && layer->m_level && layer->m_levelSettings && layer->m_player1
        && !layer->m_player1->m_isPlatformer && !layer->m_startPosObject && !layer->m_isPracticeMode
        && !GameManager::sharedState()->getGameVariable("0010") && g_rngFix && !g_rngSeedSet;
}

std::optional<std::string> findSavedSearch(PlayLayer* layer, bool coins) {
    if (!canSelectSave(layer)) return std::nullopt;
    continue_store::Store store(std::filesystem::path(DATA_DIR)/"continues", {});
    auto record=store.newest(identity(layer, coins));
    if (!record || record->completed || record->budget.reserved
        || (record->budget.finite && record->budget.remaining<=0)
        || record->budget.iterationsUsed>record->budget.iterationLimit
        || record->budget.attemptsUsed>=record->budget.attemptLimit) return std::nullopt;
    return record->lineage;
}

std::vector<RecheckSave> findRecheckSaves(PlayLayer* layer, bool coins) {
    if (!canSelectSave(layer)) return {};
    continue_store::Store store(std::filesystem::path(DATA_DIR)/"continues", {});
    std::vector<RecheckSave> result;
    for (const auto& record : store.recheckCandidates(identity(layer, coins))) {
        const auto& source=continue_store::recheckSource(record);
        result.push_back({record.lineage,record.id,record.levelName,source.boundary,source.inputs.size()});
    }
    return result;
}

static continue_store::Identity liveIdentity(GJBaseGameLayer* layer, const continue_store::Identity& saved) {
    auto live=identity(layer, g_cfg.coinFiles);
    if (!g_uiSession && !saved.lowDetail) live.lowDetail.reset();
    if (!g_uiSession && !saved.extraLdm) live.extraLdm.reset();
    return live;
}

bool prepare(GJBaseGameLayer* layer) {
    if (g_cfg.continueLineage.empty()) return true;
    auto& s=continue_session::current;
    if (s.phase!=Phase::Off) return s.phase==Phase::Reserved;
    auto& entry=continue_session::entry;
    auto* pl=PlayLayer::get();
    const auto owner=prefixProbeOwner(layer);
    if (!layer || layer!=pl || !entry.eligible || entry.consumed || !save_stop::sameSession(entry.owner,owner)
        || g_tick!=0 || g_attempt!=1) return refuse("fresh_entry_required");
    entry.consumed=true;
    // First live adapter: whole static classic levels, native input, no alternate driving roles.
    if (!g_started || g_sessionOver || !g_cfg.enabled || (g_uiSession ? g_cfg.dpShow!=-1 : g_cfg.dpShow!=0)
        || (g_uiSession && (!g_rngFix || g_rngSeedSet))
        || g_cfg.saveStopProbe || g_cfg.continueStoreProbe || g_cfg.slice || levelslice::g_phase!=levelslice::Off
        || g_cfg.profile480 || g_cfg.cbs!=0 || g_cfg.cos!=1 || !g_cfg.blockInput || g_cfg.noDeath
        || pl->m_isPracticeMode || pl->m_startPosObject || !layer->m_player1 || layer->m_player1->m_isPlatformer
        || !layer->m_level || !layer->m_levelSettings || GameManager::sharedState()->getGameVariable("0010")
        || !g_cfg.dpSeedPlan.empty() || !g_cfg.inputs.empty() || !g_prelude.empty() || !g_injects.empty()
        || !g_cfg.toggles.empty() || !g_cfg.snapAt.empty() || g_cfg.practiceAt>=0 || g_cfg.restoreAt>=0 || g_cfg.checkpointAt>=0
        || g_cfg.secPairSoloP1 || g_cfg.secPairProbe || g_cfg.secPairFixed || secsolve::g_on
        // UI seek checkpoints are taken only during Replay, after this Solve has ended.
        || cpflight::g_from>=0 || g_cfg.cpFlight>0 || (!g_uiSession && itermap::g_seekCpEvery>0)
        || dpsolve::levelHasMovingParts())
        return refuse("unsupported_profile");
    if (dpsolve::g_running.load() || secsolve::g_task.valid() || solve_budget::current.active)
        return refuse("executor_not_quiescent");
    s.store=std::make_unique<continue_store::Store>(std::filesystem::path(DATA_DIR)/"continues",
        []{return geode::utils::random::generateUUID();});
    auto record=s.store->load(g_cfg.continueLineage);
    if (!record) return refuse(s.store->error);
    const auto live=liveIdentity(layer,record->identity);
    const bool recheck=!g_cfg.continueRecheckRecord.empty();
    if (recheck && (g_cfg.continueRecheckRecord!=record->id || live==record->identity
        || !live.sameProfile(record->identity)))
        return refuse("recheck_selection_changed");
    if (!recheck && record->identity!=live)
        return refuse("identity_recheck_required");
    if (record->budget.iterationLimit>INT32_MAX || record->budget.attemptLimit<=0 || record->budget.attemptLimit>INT32_MAX
        || record->budget.iterationsUsed>INT32_MAX || record->budget.attemptsUsed>INT32_MAX)
        return refuse("unsupported_limits");
    if (!recheck && record->revisit && record->revisit->through <= record->revisit->source->boundary)
        return refuse("pending_endpoint_recheck_required");
    if (!recheck && record->revisit && record->identity.twoPlayer && (!g_cfg.dpSecAuto || !g_cfg.dpSecContract))
        return refuse("pending_joint_contract_required");
    if (recheck) {
        if (record->completed || record->budget.reserved
            || (record->budget.finite && record->budget.remaining<=0)
            || record->budget.iterationsUsed>record->budget.iterationLimit
            || record->budget.attemptsUsed>=record->budget.attemptLimit) return refuse("recheck_save_unavailable");
        s.recheckTarget=live;
        s.reserved=*record;
        s.source=continue_store::recheckSource(*record);
    } else {
        auto reserved=s.store->reserve(*record);
        if (!reserved) return refuse(s.store->error);
        s.reserved=*reserved;
        s.source=record->revisit ? *record->revisit->source : *(record->latest ? record->latest : record->deepest);
    }
    s.owner=owner;
    s.started=std::chrono::steady_clock::now();
    s.phase=Phase::Reserved;
    const auto& allowance=s.reserved.budget;
    g_cfg.dpSolveBudget=allowance.finite ? (recheck ? allowance.remaining : allowance.held) : 0;
    g_cfg.dpMaxIters=static_cast<int>(allowance.iterationLimit);
    g_cfg.maxAttempts=static_cast<int>(allowance.attemptLimit);
    g_cfg.prefixProbe=true;
    writeResult(std::string(recheck ? "continue_recheck: SELECTED lineage=" : "continue_resume: RESERVED lineage=")
        +record->lineage+" revision="+std::to_string(s.reserved.revision)
        +" mode="+(allowance.finite ? "finite" : "off")+" allowance="+std::to_string(g_cfg.dpSolveBudget)
        +" boundary="+std::to_string(s.source.boundary)+" inputs="+std::to_string(s.source.inputs.size()));
    return true;
}

bool installPrefix(GJBaseGameLayer* layer) {
    auto& s=continue_session::current;
    if (s.phase==Phase::Stopping) return true;
    if (s.phase!=Phase::Reserved) return false;
    if (!s.owns(prefixProbeOwner(layer))) { s.stop("continue_owner_changed"); return true; }
    s.owner=prefixProbeOwner(layer);
    if (g_tick!=0 || g_attempt!=1) { s.stop("continue_entry_advanced"); g_paused=true; return true; }
    dpsolve::g_iter=static_cast<int>(s.reserved.budget.iterationsUsed);
    g_finishedAttempts=static_cast<int>(s.reserved.budget.attemptsUsed);
    // Same read-only reset-pose observation as the ordinary path-quality startup.
    if (g_cfg.pathQuality) {
        auto rows=std::move(anchors::g_live);
        anchors::record(layer,1);
        anchors::g_live=std::move(rows);
    }
    dpsolve::g_plan=s.source.inputs;
    g_cfg.inputs=s.source.inputs;
    g_nextInput=0;
    dpsolve::g_planClaimsGoal=false;
    g_dpSolving=false;
    g_paused=false;
    g_hudPhase=s.recheckTarget ? "Recheck: replaying the saved path on the edited level"
        : "Continue: checking the saved prefix from a fresh entry";
    s.phase=Phase::Replaying;
    writeResult("continue_resume: PREFIX start attempt="+std::to_string(g_attempt)
        +" generation="+std::to_string(s.owner.generation)+" iterUsed="+std::to_string(dpsolve::g_iter)
        +" attemptsUsed="+std::to_string(g_finishedAttempts)+" claimsGoal=0 pin="+std::to_string(g_secPin));
    return true;
}

bool beforeInput(GJBaseGameLayer* layer,float dt,bool half) {
    auto& s=continue_session::current;
    if (!s.active()) return true;
    if (s.phase==Phase::Stopping) return false;
    if (s.phase==Phase::Reserved) { s.stop("continue_entry_advanced"); g_paused=true; return false; }
    if (s.phase==Phase::Replaying) {
        if (s.rechecked) return false;
        const auto owner=prefixProbeOwner(layer);
        const bool ending=s.recheckTarget && g_finishInputLimit!=noFinishInputLimit;
        if (!(owner==s.owner) || !prefixProbeEligible(layer,ending) || dt!=1.f/240.f || half
            || layer->m_clickBetweenSteps || !layer->m_clickOnSteps
            || (g_tick==0 && (psnap::holdingJump(layer->m_player1)
                || (layer->m_gameState.m_isDualMode && psnap::holdingJump(layer->m_player2))))) {
            s.stop("continue_prefix_contract"); g_paused=true; return false;
        }
        if (ending) {
            const auto proof=confirmed_prefix::observer.latest();
            if (proof && proof->owner==owner) s.rechecked=proof;
            else s.stop("recheck_no_verified_prefix");
            g_paused=true;
            return false;
        }
        if (g_tick>s.source.boundary) { s.stop("continue_prefix_boundary_missed"); g_paused=true; return false; }
        if (g_tick==s.source.boundary) {
            // Recheck must reach the handoff between native updates. A step
            // here would advance the world past the state being transferred.
            if (s.recheckTarget) { s.stop("recheck_boundary_inside_update"); g_paused=true; return false; }
            const auto proof=confirmed_prefix::observer.latest();
            if (!proof || proof->boundary!=s.source.boundary || !(proof->owner==owner)
                || !native_request::sameInputs(proof->inputs,s.source.inputs) || proof->heldMask!=s.source.heldMask
                || g_nextInput!=s.source.inputs.size()) {
                s.stop("continue_prefix_unverified"); g_paused=true; return false;
            }
            const bool boundaryTrace=!g_cfg.diagnosticTrace && g_cfg.continueProbeSeconds>0;
            if (boundaryTrace) g_cfg.diagnosticTrace=true;
            diagnostic::emit(layer,"continue_before_handoff",nullptr,dt,0,0,true);
            s.prefixVerified=true;
            s.phase=Phase::Searching;
            if (s.reserved.revisit) {
                save_stop::remember(owner,s.reserved.revisit->tick,s.reserved.revisit->reason.c_str(),g_cfg.inputs,proof,s.reserved.revisit->through);
                if (!s.reserved.identity.twoPlayer) dpsolve::g_forceAnchorT=s.reserved.revisit->tick;
                dpsolve::g_forcePortalBand.clear(); // geometry hints are recomputed, never imported
            }
            diagnostic::emit(layer,"continue_handoff",nullptr,dt,0,0,true);
            if (boundaryTrace) g_cfg.diagnosticTrace=false;
            writeResult("continue_resume: HANDOFF boundary="+std::to_string(g_tick)
                +" inputs="+std::to_string(g_nextInput)+" held="+std::to_string(proof->heldMask)
                +" claimsGoal="+std::to_string(dpsolve::g_planClaimsGoal)+" pin="+std::to_string(g_secPin)+" resumed=1");
            g_hudPhase="Continue: ordinary search from the verified prefix";
        }
    }
    return true;
}

bool recheckDeath(PlayLayer* layer,long long tick,float x) {
    auto& s=continue_session::current;
    if (!s.recheckTarget || s.phase!=Phase::Replaying) return false;
    const auto proof=s.rechecked ? s.rechecked : confirmed_prefix::observer.latest();
    if (!proof || !(proof->owner==s.owner) || !(s.owner==prefixProbeOwner(layer))
        || !prefixProbeEligible(layer,true)) {
        s.stop("recheck_no_verified_prefix");
    } else {
        s.rechecked=proof;
        s.recheckDeath=continue_session::Session::ReplayDeath{tick,x,g_nextInput};
    }
    g_paused=true;
    return true;
}

bool recheckStopsBatch() {
    const auto& s=continue_session::current;
    return s.recheckTarget && (g_paused || (s.phase==Phase::Replaying && g_tick>=s.source.boundary));
}

static void commitRecheck(GJBaseGameLayer* layer) {
    auto& s=continue_session::current;
    if (!(s.rechecked->owner==s.owner) || !(s.owner==prefixProbeOwner(layer))
        || liveIdentity(layer,*s.recheckTarget)!=*s.recheckTarget) {
        refuse("recheck_target_changed");
        return;
    }
    const auto& budget=solve_budget::current;
    const continue_store::Rechecked transfer{*s.recheckTarget,*s.rechecked,
        budget.active ? budget.remaining() : 0, layer->m_level->m_levelName.c_str()};
    auto reserved=s.store->reserve(s.reserved,&transfer);
    if (!reserved) { refuse(s.store->error); return; }

    const auto death=s.recheckDeath;
    s.reserved=std::move(*reserved);
    s.source=transfer.prefix;
    s.recheckTarget.reset();
    s.rechecked.reset();
    s.recheckDeath.reset();
    s.prefixVerified=true;
    s.phase=Phase::Searching;
    // Keep the real death for repair, including the inputs that were sent in
    // the dying step. The saved head contains only the earlier live prefix.
    const size_t sent=death ? death->inputs : g_nextInput;
    g_cfg.inputs.resize(std::min(sent,g_cfg.inputs.size()));
    g_nextInput=g_cfg.inputs.size();
    dpsolve::g_plan=g_cfg.inputs;
    g_paused=false;
    g_hudPhase="Recheck: continuing on the edited level";
    writeResult("continue_recheck: HANDOFF revision="+std::to_string(s.reserved.revision)
        +" boundary="+std::to_string(s.source.boundary)+" inputs="+std::to_string(s.source.inputs.size())
        +" death="+(death ? std::to_string(death->tick) : std::string("none")));
    if (death) dpsolve::onDeath(death->tick,death->x);
}

bool deferEnd(const std::string& why) {
    auto& s=continue_session::current;
    if (!s.active()) return false;
    if (s.recheckTarget) {
        s.phase=Phase::Failed;
        writeResult("continue_recheck: stopped before migration, reason="+why);
        return false;
    }
    // An explicit scene leave cannot wait for a future callback on this layer. Never refund its reservation.
    if (why=="level_exit" || why=="editor_exit" || why=="pause_mode_change" || why=="user_quit_hotkey") {
        save_stop::workerCancel=true;
        dpsolve::stopSolveBudgetWork();
        dpbridge::cancelSearch(true);
        s.phase=Phase::Failed;
        writeResult("continue_resume: ABANDONED reservation remains unsettled reason="+why);
        return false;
    }
    s.stop(why);
    g_paused=true;
    return true;
}

void frame(GJBaseGameLayer* layer) {
    auto& s=continue_session::current;
    if (!s.active()) return;
    if (!s.owns(prefixProbeOwner(layer)) || PlayLayer::get()!=layer) {
        s.phase=Phase::Failed;
        writeResult("continue_resume: owner changed; reservation remains unsettled");
        return;
    }
    if (s.recheckTarget && s.phase==Phase::Replaying) {
        if (!s.rechecked && g_finishInputLimit!=noFinishInputLimit) {
            const auto proof=confirmed_prefix::observer.latest();
            if (!proof || !(proof->owner==s.owner)) {
                refuse("recheck_no_verified_prefix");
                return;
            }
            s.rechecked=proof;
        } else if (!s.rechecked && g_tick==s.source.boundary) {
            const auto owner=prefixProbeOwner(layer);
            const bool dual=layer->m_gameState.m_isDualMode;
            const auto proof=confirmed_prefix::observer.closedUpdate(owner,g_tick,dual);
            if (!(owner==s.owner) || !prefixProbeEligible(layer) || !layer->m_player1 || layer->m_player1->m_isDead
                || (dual && (!layer->m_player2 || layer->m_player2->m_isDead))
                || !proof || !native_request::sameInputs(proof->inputs,s.source.inputs)
                || proof->heldMask!=s.source.heldMask || g_nextInput!=s.source.inputs.size()) {
                refuse("continue_prefix_unverified");
                return;
            }
            s.rechecked=proof;
        }
        if (s.rechecked) {
            commitRecheck(layer);
            return;
        }
    }
    if (s.phase==Phase::Searching && g_cfg.continueStopAt>s.source.boundary) {
        const auto proof=confirmed_prefix::observer.deepest();
        if (proof && proof->boundary>=g_cfg.continueStopAt) s.stop("continue_probe_progress");
    }
    if (s.phase!=Phase::Stopping && g_cfg.continueProbeSeconds>0
        && std::chrono::duration<double>(std::chrono::steady_clock::now()-s.started).count()>=g_cfg.continueProbeSeconds)
        s.stop("continue_probe_deadline");
    if (s.phase!=Phase::Stopping || s.stopRequested) return;
    if (s.recheckTarget) { refuse(s.terminal); return; }
    s.latest=confirmed_prefix::observer.latest();
    s.deepest=confirmed_prefix::observer.deepest();
    s.revisit=save_stop::pendingRevisit;
    if (s.revisit && !s.revisit->source) s.failure="unbound_new_revisit";
    if (!requestSaveStop(layer,true)) {
        s.phase=Phase::Failed;
        writeResult("continue_resume: cannot quiesce; reservation remains unsettled");
        endSession("continue_stop_refused");
        return;
    }
    s.stopRequested=true;
}

void finish(GJBaseGameLayer* layer) {
    auto& s=continue_session::current;
    auto& request=save_stop::current;
    if (request.phase!=save_stop::Phase::Ready || !request.owns(prefixProbeOwner(layer))) return;
    if (s.phase==Phase::Off && partial_export::afterStop && !continue_session::uiSaving) {
        request.phase=save_stop::Phase::Done;
        uiStopModeFromPause(PlayLayer::get(), true);
        return;
    }
    if (s.phase==Phase::Off && continue_session::uiSaving) {
        continue_session::uiSaving=false;
        if (commit(layer)) {
            request.phase=save_stop::Phase::Done;
            uiStopModeFromPause(PlayLayer::get(), true);
        } else {
            partial_export::afterStop.reset();
            notify::show("PathWeaver: save failed. Choose Save and stop to retry.", NotificationIcon::Error, 5.f);
        }
        return;
    }
    if (s.phase!=Phase::Stopping || !s.stopRequested || !s.owns(prefixProbeOwner(layer))) return;
    const bool userSave=s.terminal=="continue_ui_save";
    const bool completed=s.terminal=="continue_ui_cleared";
    if (userSave && !continue_session::uiSaving) return;
    if (userSave) s.failure.clear();
    s.phase=Phase::Finalizing;
    auto proof=s.reserved;
    if (s.prefixVerified || !g_uiSession) {
        if (s.latest) proof.latest=s.latest;
        if (s.deepest) proof.deepest=s.deepest;
    }
    proof.completed=completed;
    s.revisit=completed ? std::nullopt : save_stop::pendingRevisit; // includes the last worker publication
    if (s.revisit && !s.revisit->source) s.failure="unbound_new_revisit";
    proof.revisit=s.revisit;
    proof.budget.iterationsUsed=std::max<int64_t>(proof.budget.iterationsUsed,dpsolve::g_iter);
    proof.budget.attemptsUsed=std::max<int64_t>(proof.budget.attemptsUsed,g_finishedAttempts);
    const auto binary=geode::sha256File(Mod::get()->getBinaryPath());
    if (binary.isErr()) s.failure="binary_hash_failed";
    else { proof.binarySha256=binary.unwrap().toString(); proof.producer=Mod::get()->getVersion().toVString(); }
    const auto& budget=solve_budget::current;
    const double remaining=!completed && s.reserved.budget.finite && budget.active
        ? std::min(s.reserved.budget.held,budget.remaining()) : 0;
    const auto saved=s.failure.empty() ? s.store->settle(std::move(proof),remaining,s.resolution) : std::nullopt;
    if (!saved && s.failure.empty()) s.failure=s.store->error;
    if (saved) writeResult("continue_resume: SETTLED revision="+std::to_string(saved->revision)
        +" remaining="+std::to_string(saved->budget.remaining)+" latest="
        +std::to_string(saved->latest ? saved->latest->boundary : -1)+" resumed=1");
    else writeResult("continue_resume: UNSETTLED "+s.failure);
    if (userSave) {
        continue_session::uiSaving=false;
        s.phase=saved ? Phase::Finished : Phase::Stopping;
        if (saved) {
            request.phase=save_stop::Phase::Done;
            uiStopModeFromPause(PlayLayer::get(), true);
        } else {
            partial_export::afterStop.reset();
            notify::show("PathWeaver: save failed. Choose Save and stop to retry.", NotificationIcon::Error, 5.f);
        }
        return;
    }
    if (!g_uiSession) {
        std::ofstream out(std::string(DATA_DIR)+"/continue_resume_probe.json",std::ios::binary|std::ios::trunc);
        out<<matjson::makeObject({{"lineage",s.reserved.lineage},{"prefix_verified",s.prefixVerified},
            {"source_boundary",s.source.boundary},{"settled",saved.has_value()},{"reason",s.terminal},{"failure",s.failure},
            {"revision",saved ? saved->revision : s.reserved.revision},{"remaining",saved ? saved->budget.remaining : 0}}).dump(2)<<'\n';
    }
    const bool injecting=g_injecting;
    g_injecting=true;layer->handleButton(false,1,true);layer->handleButton(false,1,false);g_injecting=injecting;
    save_stop::current.phase=save_stop::Phase::Done;
    s.phase=saved ? Phase::Finished : Phase::Failed;
    if (completed && saved) {
        if (lifecycle::pending()) return;
        showSolvedLevelPrompt();
        return;
    }
    endSession(saved ? s.terminal : "continue_settle_failed");
    if (g_uiSession && !s.prefixVerified) notify::show("PathWeaver: could not replay the saved start.",
        NotificationIcon::Warning, 5.f);
}
} // namespace p1::continue_runtime

namespace p1::partial_export {
std::optional<Request> live(PlayLayer* layer, bool coins) {
    if (!layer || layer!=PlayLayer::get() || !g_uiSession || !g_started || g_sessionOver
        || !g_cfg.dpSolve || g_dpShowSolution || !g_cfg.prefixProbe || g_cfg.profile480
        || g_cfg.cbs!=0 || g_cfg.cos!=1 || !g_cfg.blockInput || coins!=g_cfg.coinFiles
        || !layer->m_level || !layer->m_levelSettings || layer->m_isTestMode || layer->m_startPosObject
        || layer->m_level->isPlatformer() || GameManager::sharedState()->getGameVariable("0010")
        || !g_rngFix || g_rngSeedSet
        || (levelslice::g_phase!=levelslice::Off && levelslice::g_phase!=levelslice::Sliced
            && levelslice::g_phase!=levelslice::Whole)
        || continue_session::uiSaving || afterStop || save_stop::current.active()) return std::nullopt;
    const auto phase=continue_session::current.phase;
    if (phase!=continue_session::Phase::Off && phase!=continue_session::Phase::Searching) return std::nullopt;
    const auto proof=candidates.deepest();
    const auto owner=prefixProbeOwner(layer);
    const auto& origin=source;
    if (!proof || proof->boundary<=0 || proof->owner.entry!=owner.entry || proof->owner.layer!=owner.layer
        || !origin || origin->owner.entry!=owner.entry || origin->owner.layer!=owner.layer) return std::nullopt;
    auto identity=continue_runtime::identity(layer,coins);
    if (identity!=origin->identity) return std::nullopt;
    Request request{origin->original.value_or(identity),*proof,
        origin->original ? std::optional(identity) : std::nullopt};
    // The copy's survival is only a candidate. A fresh flight on the original authorizes export.
    return sourceLevel(layer,request) ? std::optional(std::move(request)) : std::nullopt;
}

GJGameLevel* sourceLevel(PlayLayer* layer, const Request& request) {
    if (!layer || layer!=PlayLayer::get() || !layer->m_level || !layer->m_levelSettings) return nullptr;
    const auto live=continue_runtime::identity(layer,request.identity.coins);
    std::optional<continue_store::Identity> original;
    if (request.slicedFrom) {
        if ((levelslice::g_phase!=levelslice::Sliced && levelslice::g_phase!=levelslice::Whole)
            || !levelslice::g_orig) return nullptr;
        original=live;
        const auto raw=continue_runtime::content(levelslice::g_orig);
        original->contentHash=executionHash(raw);
        original->contentBytes=static_cast<int64_t>(raw.size());
    }
    if (!matchesSource(request,live,original)) return nullptr;
    return request.slicedFrom ? levelslice::g_orig : layer->m_level;
}

bool matches(PlayLayer* layer, const Request& request) {
    return continue_runtime::canSelectSave(layer) && !layer->m_isTestMode
        && continue_runtime::identity(layer, request.identity.coins) == request.identity;
}

std::optional<Request> ready(PlayLayer* layer, bool coins) {
    if (!verified || verifiedEntry!=g_uiEntrySerial || !g_sessionOver
        || layer!=PlayLayer::get() || verified->identity.coins!=coins || !matches(layer,*verified)) return std::nullopt;
    return verified;
}

std::optional<Request> find(PlayLayer* layer, bool coins) {
    if ((botDriving() && !g_sessionOver) || !continue_runtime::canSelectSave(layer) || layer->m_isTestMode) return std::nullopt;
    auto identity = continue_runtime::identity(layer, coins);
    continue_store::Store store(std::filesystem::path(DATA_DIR) / "continues", {});
    const auto record = store.newest(identity);
    if (!record || record->completed) return std::nullopt;
    return Request{std::move(identity), continue_store::recheckSource(*record)};
}
}
