#pragma once
// Bind an exported prefix to the exact plan, level geometry and a fresh native replay.
#include <Geode/utils/file.hpp>

namespace p1 {
inline std::string executionHash(const std::string& bytes) {
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : bytes) { hash ^= c; hash *= 1099511628211ULL; }
    return std::to_string(hash);
}
inline std::string executionFileBytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return {(std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()};
}
inline std::string executionLevelHash(GJGameLevel* level) {
    std::string raw = level->m_levelType == GJLevelType::Main
        ? std::string(LocalLevelManager::sharedState()->getMainLevelString(level->m_levelID.value()))
        : std::string(level->m_levelString);
    const auto start = raw.find_first_not_of(" \r\n\t");
    if (start != std::string::npos) raw.erase(0, start);
    if (raw.starts_with("H4sI")) raw = std::string(cocos2d::ZipUtils::decompressString(raw, false, 0));
    return executionHash(raw);
}
inline bool writeExecutionReceipt(GJGameLevel* level, const std::string& path, size_t applied,
                                   long long boundary, bool fresh) {
    if (g_exportTps != 240 && g_exportTps != 480) {
        writeResult("execution: receipt refused, unsupported or mixed TPS ("
            + std::to_string(g_exportTps) + ")");
        return false;
    }
    const auto bytes = executionFileBytes(path);
    if (bytes.empty()) {
        writeResult("execution: receipt refused, saved plan is empty or unreadable");
        return false;
    }
    auto value = matjson::makeObject({
        {"schema", "PathWeaver.execution.v1"}, {"source_hash", executionHash(bytes)},
        {"level_hash", executionLevelHash(level)}, {"applied_inputs", static_cast<int64_t>(applied)},
        {"boundary", boundary}, {"tps", g_exportTps}, {"fresh_entry", fresh}
    });
    const auto receiptPath = path + ".execution.json";
    const auto encoded = value.dump(2) + '\n';
    const auto result = geode::utils::file::writeStringSafe(receiptPath, encoded);
    if (result.isErr()) {
        writeResult("execution: receipt write failed: " + result.unwrapErr());
        return false;
    }
    if (executionFileBytes(receiptPath) != encoded) {
        writeResult("execution: receipt readback failed");
        return false;
    }
    return true;
}
inline bool verifiedExportPrefix(const std::string& path, GJGameLevel* level,
                                 std::vector<InputCmd>& inputs, int tps, std::string& error, long long* boundary = nullptr) {
    auto parsed = matjson::parse(executionFileBytes(path + ".execution.json"));
    if (parsed.isErr()) {
        error = "Run Replay once from a fresh level entry before exporting. This solution has no execution receipt.";
        return false;
    }
    const auto& value = parsed.unwrap();
    const auto count = value["applied_inputs"].asInt().unwrapOr(-1);
    if (value["schema"].asString().unwrapOr("") != "PathWeaver.execution.v1"
        || value["source_hash"].asString().unwrapOr("") != executionHash(executionFileBytes(path))
        || value["level_hash"].asString().unwrapOr("") != executionLevelHash(level)
        || value["tps"].asInt().unwrapOr(0) != tps
        || !value["fresh_entry"].asBool().unwrapOr(false)
        || count <= 0 || static_cast<size_t>(count) > inputs.size()) {
        error = "This plan or level needs a fresh Replay verification before export. No macro was written.";
        return false;
    }
    const auto end = value["boundary"].asInt().unwrapOr(-1);
    if (end <= inputs[static_cast<size_t>(count) - 1].step) {
        error = "Invalid execution boundary; run a fresh Replay again.";
        return false;
    }
    if (boundary) *boundary = end;
    inputs.resize(static_cast<size_t>(count));
    return true;
}
}
