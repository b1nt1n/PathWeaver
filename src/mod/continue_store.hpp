#pragma once
// Local Windows continuation records. A checksum is integrity checking, not gameplay proof.
#include "mod/save_stop.hpp"
#include <matjson.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace p1::continue_store {
using Json = matjson::Value;
using Prefix = confirmed_prefix::Snapshot;
inline constexpr const char* schema = "PathWeaver.continue.v1";
inline constexpr const char* profile = "native240.raw-before-routing.before-input.v1";

struct Identity {
    std::string contentHash;
    int64_t contentBytes = 0;
    bool twoPlayer = false, coins = false;
    std::optional<bool> lowDetail, extraLdm; // legacy diagnostic records may leave these unknown
    bool operator==(const Identity&) const = default;
    bool sameProfile(const Identity& other) const {
        return twoPlayer==other.twoPlayer && coins==other.coins
            && lowDetail==other.lowDetail && extraLdm==other.extraLdm;
    }
};
struct Allowance {
    bool finite = false, reserved = false;
    double total = 0, remaining = 0, held = 0;
    std::string operation;
    int64_t iterationLimit = 0, iterationsUsed = 0, attemptLimit = 0, attemptsUsed = 0;
};
struct Record {
    std::string lineage, id, parent, producer, binarySha256;
    int64_t revision = 0;
    bool resumed = false;
    Identity identity;
    std::optional<Prefix> latest, deepest;
    std::optional<save_stop::Revisit> revisit;
    Allowance budget;
    bool completed = false;
    std::string levelName;
};
inline const Prefix& recheckSource(const Record& record) {
    return record.deepest ? *record.deepest : *record.latest;
}
struct Rechecked {
    Identity target;
    Prefix prefix;
    double remaining = 0;
    std::string levelName;
};
struct Origin {
    confirmed_prefix::Owner owner;
    Identity identity;
    std::string lineage, record; // stable first-Save key, retained across I/O retries
    std::optional<Allowance> allowance;
    int saveGeneration = -1;
};
inline std::optional<Origin> origin;

inline std::string checksum(const std::string& text) {
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : text) { h ^= c; h *= 1099511628211ULL; }
    return std::to_string(h);
}
inline bool uuid(const std::string& s) {
    if (s.size() != 36) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (s[i] != '-') return false; }
        else if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
    }
    return true;
}
struct Invalid { const char* reason; };
inline void need(bool ok, const char* reason) { if (!ok) throw Invalid{reason}; }
inline void keys(const Json& v, std::initializer_list<const char*> names) {
    need(v.isObject() && v.size() == names.size(), "invalid_fields");
    for (auto name : names) need(v.contains(name), "missing_field");
}
inline std::string text(const Json& v) { need(v.isString(), "expected_string"); return v.asString().unwrap(); }
inline bool boolean(const Json& v) { need(v.isBool(), "expected_bool"); return v.asBool().unwrap(); }
inline int64_t integer(const Json& v, int64_t low = 0, int64_t high = INT64_MAX) {
    need(v.isExactlyInt() || v.isExactlyUInt(), "expected_integer");
    const auto n = v.asInt(); need(n.isOk(), "integer_range");
    const auto out = n.unwrap(); need(out >= low && out <= high, "integer_range"); return out;
}
inline double number(const Json& v) {
    need(v.isNumber(), "expected_number");
    const double n = v.asDouble().unwrap(); need(std::isfinite(n) && n >= 0, "invalid_seconds"); return n;
}
inline Json nullable(std::optional<bool> v) { return v ? Json(*v) : Json(nullptr); }
inline std::optional<bool> maybeBool(const Json& v) { return v.isNull() ? std::nullopt : std::optional<bool>(boolean(v)); }

inline Json packPrefix(const std::optional<Prefix>& p) {
    if (!p) return nullptr;
    std::vector<Json> edges;
    for (const auto& c : p->inputs) edges.emplace_back(std::vector<Json>{c.step, c.player1 ? 1 : 2, c.down ? 1 : 0});
    return matjson::makeObject({{"entry",p->owner.entry},{"attempt",p->owner.attempt},
        {"generation",p->owner.generation},{"branch",p->branch},{"boundary",p->boundary},
        {"count",p->inputs.size()},{"held",p->heldMask},{"inputs",std::move(edges)}});
}
inline std::optional<Prefix> unpackPrefix(const Json& v) {
    if (v.isNull()) return std::nullopt;
    keys(v,{"entry","attempt","generation","branch","boundary","count","held","inputs"});
    Prefix p;
    p.owner = {static_cast<uint64_t>(integer(v["entry"],1)),0,integer(v["attempt"],1),static_cast<int>(integer(v["generation"],0,INT32_MAX))};
    p.branch = integer(v["branch"],1); p.boundary = integer(v["boundary"],1,static_cast<int64_t>(INT32_MAX)+1);
    p.heldMask = static_cast<int>(integer(v["held"],0,3));
    need(v["inputs"].isArray(), "expected_inputs");
    const auto& edges = v["inputs"].asArray().unwrap();
    need(static_cast<uint64_t>(integer(v["count"])) == edges.size(), "input_count");
    long long previous = -1;
    for (const auto& e : edges) {
        need(e.isArray() && e.size() == 3, "input_fields");
        const auto tick = integer(e[0],0,p.boundary-1);
        need(tick >= previous && tick <= INT32_MAX, "input_order"); previous = tick;
        p.inputs.push_back({static_cast<int>(tick),integer(e[2],0,1)!=0,integer(e[1],1,2)==1});
    }
    need(inputMaskOfPrefix(p.inputs,p.inputs.size()) == p.heldMask, "held_mask");
    return p;
}
inline Json packIdentity(const Identity& i) {
    return matjson::makeObject({{"gd","2.2081"},{"geode","5.11.2"},{"profile",profile},{"tps",240},
        {"content_hash",i.contentHash},{"content_bytes",i.contentBytes},{"content_codec","trim-leading-and-gzip-v1"},
        {"low_detail",nullable(i.lowDetail)},{"extra_ldm",nullable(i.extraLdm)},{"two_player",i.twoPlayer},
        {"flip",false},{"platformer",false},{"coins",i.coins},{"entry","observed-head-requires-fresh-recheck-v1"}});
}
inline Json pack(const Record& r) {
    Json intent = nullptr;
    if (r.revisit) {
        intent=matjson::makeObject({{"tick",r.revisit->tick},{"reason",r.revisit->reason},{"source",packPrefix(r.revisit->source)}});
        if (r.revisit->through>0) intent["through"]=r.revisit->through;
    }
    const auto& b = r.budget;
    auto result = matjson::makeObject({{"lineage",r.lineage},{"record",r.id},{"parent",r.parent},{"revision",r.revision},
        {"producer",r.producer},{"binary_sha256",r.binarySha256},{"resumed",r.resumed},{"identity",packIdentity(r.identity)},
        {"latest",packPrefix(r.latest)},{"deepest",packPrefix(r.deepest)},{"revisit",std::move(intent)},
        {"budget",matjson::makeObject({{"mode",b.finite ? "finite" : "off"},
            {"state",r.completed ? "completed" : b.reserved ? "reserved" : "available"},
            {"total_seconds",b.total},{"remaining_seconds",b.remaining},{"reserved_seconds",b.held},{"operation",b.operation},
            {"iteration_limit",b.iterationLimit},{"iterations_used",b.iterationsUsed},{"attempt_limit",b.attemptLimit},{"attempts_used",b.attemptsUsed}})}});
    if (!r.levelName.empty()) result["level_name"] = r.levelName;
    return result;
}
inline std::string encode(const Record& r) {
    auto payload = pack(r);
    return matjson::makeObject({{"schema",schema},{"checksum_kind","fnv1a64-decimal-v1"},
        {"checksum",checksum(payload.dump())},{"payload",std::move(payload)}}).dump(2) + '\n';
}
inline std::optional<Record> decode(const std::string& bytes, std::string& error) {
    try {
        auto parsed = matjson::parse(bytes); need(parsed.isOk(), "invalid_json");
        const auto& envelope = parsed.unwrap();
        keys(envelope,{"schema","checksum_kind","checksum","payload"});
        need(text(envelope["schema"]) == schema, "unsupported_schema");
        need(text(envelope["checksum_kind"]) == "fnv1a64-decimal-v1", "unsupported_checksum");
        const auto& v = envelope["payload"];
        need(text(envelope["checksum"]) == checksum(v.dump()), "checksum_mismatch");
        if (v.contains("level_name"))
            keys(v,{"lineage","record","parent","revision","producer","binary_sha256","resumed","identity","latest","deepest","revisit","budget","level_name"});
        else
            keys(v,{"lineage","record","parent","revision","producer","binary_sha256","resumed","identity","latest","deepest","revisit","budget"});
        Record r;
        if (v.contains("level_name")) r.levelName=text(v["level_name"]);
        r.lineage=text(v["lineage"]); r.id=text(v["record"]); r.parent=text(v["parent"]); r.revision=integer(v["revision"],1);
        need(uuid(r.lineage) && uuid(r.id) && r.id!=r.parent
            && (r.revision == 1 ? r.parent.empty() : uuid(r.parent)), "invalid_identity");
        r.producer=text(v["producer"]); r.binarySha256=text(v["binary_sha256"]); r.resumed=boolean(v["resumed"]);
        need(!r.producer.empty() && r.binarySha256.size()==64
            && r.binarySha256.find_first_not_of("0123456789abcdef")==std::string::npos, "invalid_producer");
        const auto& i=v["identity"];
        keys(i,{"gd","geode","profile","tps","content_hash","content_bytes","content_codec","low_detail","extra_ldm","two_player","flip","platformer","coins","entry"});
        need(text(i["gd"])=="2.2081" && text(i["geode"])=="5.11.2" && text(i["profile"])==profile
            && integer(i["tps"])==240 && !boolean(i["flip"]) && !boolean(i["platformer"]), "unsupported_profile");
        need(text(i["entry"])=="observed-head-requires-fresh-recheck-v1"
            && text(i["content_codec"])=="trim-leading-and-gzip-v1", "unsupported_entry");
        r.identity={text(i["content_hash"]),integer(i["content_bytes"],1),boolean(i["two_player"]),boolean(i["coins"]),maybeBool(i["low_detail"]),maybeBool(i["extra_ldm"])};
        need(!r.identity.contentHash.empty() && r.identity.contentHash.find_first_not_of("0123456789")==std::string::npos, "invalid_content_hash");
        r.latest=unpackPrefix(v["latest"]); r.deepest=unpackPrefix(v["deepest"]);
        need(r.latest || r.deepest, "no_prefix");
        if (r.latest && r.deepest) need(r.deepest->boundary>=r.latest->boundary,"deepest_boundary");
        if (!v["revisit"].isNull()) {
            const auto& p=v["revisit"];
            if (p.contains("through")) keys(p,{"tick","reason","source","through"});
            else keys(p,{"tick","reason","source"});
            r.revisit=save_stop::Revisit{integer(p["tick"]),text(p["reason"]),unpackPrefix(p["source"])};
            need(r.revisit->source && r.revisit->tick<r.revisit->source->boundary, "unbound_revisit");
            if (p.contains("through")) r.revisit->through=integer(p["through"],r.revisit->source->boundary+1);
            need(r.revisit->reason=="native_early" || r.revisit->reason=="missed_portal_wedge"
                || r.revisit->reason=="missed_portal_ladder" || r.revisit->reason=="input_anchor", "unsupported_revisit");
        }
        const auto& b=v["budget"];
        keys(b,{"mode","state","total_seconds","remaining_seconds","reserved_seconds","operation","iteration_limit","iterations_used","attempt_limit","attempts_used"});
        const auto mode=text(b["mode"]), state=text(b["state"]);
        need((mode=="off" || mode=="finite") && (state=="available" || state=="reserved" || state=="completed"), "invalid_budget_mode");
        r.completed=state=="completed";
        r.budget={mode=="finite",state=="reserved",number(b["total_seconds"]),number(b["remaining_seconds"]),number(b["reserved_seconds"]),text(b["operation"]),
            integer(b["iteration_limit"]),integer(b["iterations_used"]),integer(b["attempt_limit"]),integer(b["attempts_used"])};
        const auto& a=r.budget;
        need(a.finite ? a.total>0 && a.remaining<=a.total && a.held<=a.total-a.remaining
                      : a.total==0 && a.remaining==0 && a.held==0, "budget_range");
        need(a.reserved ? a.remaining==0 && uuid(a.operation) : a.held==0 && a.operation.empty(), "budget_reservation");
        need(!r.completed || (r.resumed && a.remaining==0 && !r.revisit), "invalid_completion");
        error.clear(); return r;
    } catch (const Invalid& e) { error=e.reason; return std::nullopt; }
}

using IdFactory = std::function<std::string()>;
using Replace = std::function<bool(const std::filesystem::path&,const std::string&,const std::string&,std::string&)>;
class WriterLock {
#ifdef _WIN32
    HANDLE m_handle = INVALID_HANDLE_VALUE;
#endif
public:
    explicit WriterLock(const std::filesystem::path& path) {
#ifdef _WIN32
        m_handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
#endif
    }
    ~WriterLock() {
#ifdef _WIN32
        if (valid()) CloseHandle(m_handle);
#endif
    }
    bool valid() const {
#ifdef _WIN32
        return m_handle!=INVALID_HANDLE_VALUE;
#else
        return false; // This first store adapter is qualified only on Windows.
#endif
    }
    WriterLock(const WriterLock&)=delete;
    WriterLock& operator=(const WriterLock&)=delete;
};
inline bool atomicReplace(const std::filesystem::path& target, const std::string& bytes, const std::string& nonce, std::string& error) {
    const auto temp=target.parent_path()/(nonce+".tmp");
    std::ofstream out(temp,std::ios::binary|std::ios::trunc);
    if (!out) { error="temporary_open_failed"; return false; }
    out.write(bytes.data(),static_cast<std::streamsize>(bytes.size())); out.flush();
    const bool written=out.good(); out.close();
    std::error_code ec;
    if (!written || out.fail()) { std::filesystem::remove(temp,ec); error="temporary_write_failed"; return false; }
    std::filesystem::rename(temp,target,ec);
    if (ec) { std::filesystem::remove(temp,ec); error="replace_failed"; return false; }
    return true;
}

// One writer per lineage. The full head is authoritative; history never restores allowance.
class Store {
    std::filesystem::path m_root;
    IdFactory m_id;
    Replace m_replace;
    std::optional<Record> m_active;
    std::unique_ptr<WriterLock> lock(const std::string& lineage) {
        if (!uuid(lineage)) { error="invalid_lineage"; return {}; }
        std::error_code ec; std::filesystem::create_directories(m_root/lineage,ec);
        if (ec) { error="directory_failed"; return {}; }
        auto guard=std::make_unique<WriterLock>(m_root/lineage/"writer.lock");
        if (!guard->valid()) { error="writer_busy_or_unavailable"; return {}; }
        return guard;
    }
    std::string freshId() { auto id=m_id(); if (!uuid(id)) { error="invalid_generated_id"; return {}; } return id; }
    std::filesystem::path head(const std::string& lineage) const { return m_root/lineage/"head.json"; }
    bool readBytes(const std::filesystem::path& path, std::string& out) {
        std::ifstream file(path,std::ios::binary);
        if (!file) { error="read_failed"; return false; }
        out.assign(std::istreambuf_iterator<char>(file),{});
        if (file.bad()) { error="read_failed"; return false; }
        return true;
    }
    std::optional<Record> commit(Record next, const std::optional<Record>& previous) {
        std::string why; const auto bytes=encode(next);
        if (!decode(bytes,why)) { error=why; return std::nullopt; }
        std::error_code ec;
        const auto directory=m_root/next.lineage;
        std::filesystem::create_directories(directory/"history",ec);
        if (ec) { error="directory_failed"; return std::nullopt; }
        if (previous) {
            const auto archive=directory/"history"/(previous->id+".json");
            const auto oldBytes=encode(*previous);
            if (std::filesystem::exists(archive,ec)) {
                std::string stored;
                if (!readBytes(archive,stored) || stored!=oldBytes) { error="history_conflict"; return std::nullopt; }
            } else {
                const auto nonce=freshId(); if (nonce.empty() || !m_replace(archive,oldBytes,nonce,error)) return std::nullopt;
            }
        }
        const auto nonce=freshId();
        if (nonce.empty() || !m_replace(head(next.lineage),bytes,nonce,error)) return std::nullopt;
        return next;
    }
    std::optional<Record> expected(const Record& selected) {
        auto now=load(selected.lineage);
        if (now && encode(*now)!=encode(selected)) { error="revision_conflict"; return std::nullopt; }
        return now;
    }
    bool advance(Record& next, const Record& previous) {
        if (previous.revision==INT64_MAX) { error="revision_exhausted"; return false; }
        next.lineage=previous.lineage; next.parent=previous.id; next.revision=previous.revision+1;
        next.id=freshId(); return !next.id.empty();
    }
public:
    std::string error;
    Store(std::filesystem::path root, IdFactory ids, Replace replace=atomicReplace)
        : m_root(std::move(root)),m_id(std::move(ids)),m_replace(std::move(replace)) {}
    std::optional<Record> load(const std::string& lineage) {
        error.clear(); if (!uuid(lineage)) { error="invalid_lineage"; return std::nullopt; }
        std::string bytes; if (!readBytes(head(lineage),bytes)) return std::nullopt;
        auto record=decode(bytes,error);
        if (record && record->lineage!=lineage) { error="lineage_mismatch"; return std::nullopt; }
        return record;
    }
    std::optional<Record> newest(const Identity& identity) {
        std::error_code ec;
        std::optional<Record> best;
        std::filesystem::file_time_type bestTime{};
        for (std::filesystem::directory_iterator it(m_root, ec), end; !ec && it!=end; it.increment(ec)) {
            const auto lineage = it->path().filename().string();
            if (!uuid(lineage)) continue;
            std::error_code stampError;
            const auto time = std::filesystem::last_write_time(head(lineage), stampError);
            if (stampError || (best && time<bestTime)) continue;
            auto candidate = load(lineage);
            if (!candidate || candidate->identity!=identity) continue;
            if (best && time==bestTime && lineage<=best->lineage) continue;
            best = std::move(candidate);
            bestTime = time;
        }
        return best;
    }
    std::vector<Record> recheckCandidates(const Identity& target) {
        std::vector<std::pair<std::filesystem::file_time_type,Record>> matches;
        std::error_code ec;
        for (std::filesystem::directory_iterator it(m_root,ec),end; !ec && it!=end; it.increment(ec)) {
            const auto lineage=it->path().filename().string();
            if (!uuid(lineage)) continue;
            auto record=load(lineage);
            if (!record || record->identity==target || !record->identity.sameProfile(target)
                || record->completed || record->budget.reserved
                || (record->budget.finite && record->budget.remaining<=0)
                || record->budget.attemptsUsed>=record->budget.attemptLimit
                || record->budget.iterationsUsed>record->budget.iterationLimit) continue;
            std::error_code stampError;
            const auto stamp=std::filesystem::last_write_time(head(lineage),stampError);
            if (!stampError) matches.emplace_back(stamp,std::move(*record));
        }
        std::sort(matches.begin(),matches.end(),[](const auto& a,const auto& b) {
            return a.first==b.first ? a.second.lineage>b.second.lineage : a.first>b.first;
        });
        std::vector<Record> result;
        for (auto& match : matches) result.push_back(std::move(match.second));
        return result;
    }
    std::optional<Record> create(Record draft) {
        error.clear();
        if (draft.lineage.empty() && draft.id.empty()) {
            draft.lineage=freshId(); draft.id=freshId(); draft.parent.clear(); draft.revision=1;
        }
        if (draft.revision!=1 || !draft.parent.empty() || draft.budget.reserved || draft.completed) { error="invalid_create_ticket"; return std::nullopt; }
        if (draft.lineage.empty() || draft.id.empty()) return std::nullopt;
        auto guard=lock(draft.lineage); if (!guard) return std::nullopt;
        std::error_code ec;
        if (std::filesystem::exists(head(draft.lineage),ec)) {
            auto previous=load(draft.lineage);
            if (previous && encode(*previous)==encode(draft)) return previous;
            if (previous) error="lineage_exists";
            return std::nullopt;
        }
        return commit(std::move(draft),std::nullopt);
    }
    std::optional<Record> reserve(const Record& selected, const Rechecked* rechecked = nullptr) {
        error.clear(); if (m_active) { error="operation_active"; return std::nullopt; }
        auto guard=lock(selected.lineage); if (!guard) return std::nullopt;
        auto old=expected(selected); if (!old) return std::nullopt;
        if (old->completed) { error="search_completed"; return std::nullopt; }
        if (old->budget.reserved) { error="reservation_unsettled"; return std::nullopt; }
        if ((old->budget.finite && old->budget.remaining<=0)
            || old->budget.iterationsUsed>old->budget.iterationLimit
            || (old->budget.attemptLimit>0 && old->budget.attemptsUsed>=old->budget.attemptLimit)) {
            error="allowance_exhausted"; return std::nullopt;
        }
        auto next=*old;
        if (rechecked) {
            const auto& p=rechecked->prefix;
            const auto& source=recheckSource(*old);
            if (rechecked->target==old->identity || !rechecked->target.sameProfile(old->identity)
                || !p.owner.layer || p.boundary<=0 || p.boundary>source.boundary
                || !save_stop::samePrefix(p,source.inputs)
                || !std::isfinite(rechecked->remaining) || rechecked->remaining<0
                || rechecked->remaining>old->budget.remaining
                || (old->budget.finite && rechecked->remaining<=0)) {
                error="invalid_recheck_proof"; return std::nullopt;
            }
            next.identity=rechecked->target;
            next.levelName=rechecked->levelName;
            next.latest=next.deepest=p;
            next.revisit.reset();
            next.budget.remaining=rechecked->remaining;
        }
        if (!advance(next,*old)) return std::nullopt;
        next.resumed=true; next.budget.reserved=true; next.budget.operation=freshId();
        if (next.budget.operation.empty()) return std::nullopt;
        next.budget.held=next.budget.remaining; next.budget.remaining=0;
        auto saved=commit(std::move(next),old); if (saved) m_active=*saved;
        return saved;
    }
    std::optional<Record> settle(Record proof, double remaining, const std::optional<save_stop::Resolution>& resolution = {}) {
        error.clear(); if (!m_active) { error="no_active_operation"; return std::nullopt; }
        auto guard=lock(m_active->lineage); if (!guard) return std::nullopt;
        auto old=expected(*m_active); if (!old) return std::nullopt;
        if (resolution && (!old->revisit || !save_stop::sameIntent(*old->revisit,resolution->intent) || !save_stop::accepted(*resolution))) {
            error="invalid_revisit_resolution"; return std::nullopt;
        }
        if (!std::isfinite(remaining) || remaining<0 || remaining>old->budget.held
            || (proof.completed && (remaining!=0 || proof.revisit))
            || packIdentity(proof.identity)!=packIdentity(old->identity)
            || proof.budget.iterationLimit!=old->budget.iterationLimit || proof.budget.attemptLimit!=old->budget.attemptLimit
            || proof.budget.iterationsUsed<old->budget.iterationsUsed || proof.budget.attemptsUsed<old->budget.attemptsUsed) {
            error="invalid_settlement"; return std::nullopt;
        }
        const auto iterations=proof.budget.iterationsUsed, attempts=proof.budget.attemptsUsed;
        const auto previousBest=old->deepest ? old->deepest : old->latest;
        if (previousBest && (!proof.deepest || previousBest->boundary>proof.deepest->boundary)) proof.deepest=previousBest;
        if (proof.latest && (!proof.deepest || proof.latest->boundary>proof.deepest->boundary)) proof.deepest=proof.latest;
        if (!proof.completed && !proof.revisit && !resolution) proof.revisit=old->revisit;
        if (!advance(proof,*old)) return std::nullopt;
        proof.resumed=true; proof.budget=old->budget; proof.budget.reserved=false;
        proof.budget.operation.clear(); proof.budget.remaining=remaining; proof.budget.held=0;
        proof.budget.iterationsUsed=iterations; proof.budget.attemptsUsed=attempts;
        auto saved=commit(std::move(proof),old); if (saved) m_active.reset();
        return saved;
    }
};
} // namespace p1::continue_store
