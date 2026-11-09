#pragma once
#include "mod/macro_binary.hpp"
#include <fstream>
#include <sstream>

namespace p1::macro {
enum class ExportPlayer { Standard, Eclipse194, XdBotLab };

struct ExportSelection {
    ExportPlayer player = ExportPlayer::Standard;
    bool gdr = true, gdr2 = false;
};

inline const char* exportPlayerName(ExportPlayer player) {
    switch (player) {
        case ExportPlayer::Standard: return "Standard";
        case ExportPlayer::Eclipse194: return "Eclipse 1.9.4";
        case ExportPlayer::XdBotLab: return "xdBot Lab";
    }
    return "";
}

inline std::string exportSelectionError(const ExportSelection& choice,
                                        const std::vector<InputCmd>& edges) {
    if (!choice.gdr && !choice.gdr2) return "Select GDR or GDR2.";
    if (choice.gdr2 && choice.player == ExportPlayer::XdBotLab)
        return "xdBot Lab uses GDR.";
    if (choice.gdr2 && !gdr2OrderSafe(edges))
        return "GDR2 cannot preserve this P1/P2 event order. Select GDR.";
    if (choice.player == ExportPlayer::Eclipse194
        && std::any_of(edges.begin(), edges.end(), [](const auto& edge) { return edge.step == INT_MAX; }))
        return "This timeline exceeds the Eclipse 1.9.4 frame range.";
    return "";
}

struct ExportFiles {
    std::vector<std::filesystem::path> written;
    std::vector<std::string> errors;
};

inline ExportFiles writeSelectedMacros(const std::filesystem::path& folder,
                                      const std::string& name, int levelId,
                                      const std::string& version, const std::vector<InputCmd>& edges,
                                      int tps, long long endTick, const ExportSelection& choice,
                                      bool partial = false, const std::string& filenameSuffix = {}) {
    ExportFiles result;
    auto error = exportSelectionError(choice, edges);
    if (!error.empty()) { result.errors.push_back(error); return result; }
    if (!binaryPlanValid(edges, tps, levelId, partial) || endTick <= 0
        || (!edges.empty() && endTick <= edges.back().step)) {
        result.errors.push_back("The verified timeline is invalid.");
        return result;
    }
    std::vector<InputCmd> shifted;
    const auto* inputs = &edges;
    std::string metadataVersion = version;
    if (choice.player == ExportPlayer::Eclipse194) {
        if (!eclipse194Plan(edges, shifted) || endTick == LLONG_MAX) {
            result.errors.push_back("This timeline exceeds the Eclipse 1.9.4 frame range.");
            return result;
        }
        inputs = &shifted;
        ++endTick;
        metadataVersion += "; Eclipse1.9.4 +1";
    }
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    if (ec) { result.errors.push_back("Could not create the exports folder."); return result; }
    std::vector<std::string> suffixes;
    if (choice.gdr) suffixes.emplace_back(".gdr");
    if (choice.gdr2) suffixes.emplace_back(".gdr2");
    const auto base = exportBase(folder, name + filenameSuffix, suffixes);
    for (const auto& suffix : suffixes) {
        const auto filename = base + suffix;
        const auto output = exportPath(folder, filename);
        std::ostringstream data(std::ios::binary | std::ios::out);
        const bool encoded = suffix == ".gdr2"
            ? writeGdr2(data, *inputs, tps, levelId, name, metadataVersion, endTick, partial)
            : writeGdrBinary(data, *inputs, tps, levelId, name, metadataVersion,
                             choice.player == ExportPlayer::XdBotLab, endTick, partial);
        if (!encoded) {
            result.errors.push_back("Could not encode " + suffix + ".");
            continue;
        }
        // A second export cannot truncate a file that appeared after the name was chosen.
        std::ofstream file(output, std::ios::binary | std::ios::noreplace);
        if (!file) {
            result.errors.push_back("Could not create " + filename + ".");
            continue;
        }
        const auto bytes = data.str();
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        file.close();
        if (file.fail()) {
            std::filesystem::remove(output, ec);
            result.errors.push_back("Could not write " + filename + ".");
            continue;
        }
        result.written.push_back(output);
    }
    return result;
}

inline ExportFiles writeSelectedProfiles(const std::filesystem::path& folder,
                                       const std::string& name, int levelId,
                                       const std::string& version, const std::vector<InputCmd>& edges,
                                       int tps, long long endTick, const std::vector<ExportSelection>& choices,
                                       bool partial = false) {
    ExportFiles result;
    size_t selected = 0;
    for (const auto& choice : choices) {
        if (!choice.gdr && !choice.gdr2) continue;
        ++selected;
        const auto error = exportSelectionError(choice, edges);
        if (!error.empty()) result.errors.push_back(std::string(exportPlayerName(choice.player)) + ": " + error);
    }
    if (!selected) result.errors.push_back("Select at least one macro format.");
    if (!result.errors.empty()) return result;
    for (const auto& choice : choices) {
        if (!choice.gdr && !choice.gdr2) continue;
        const std::string suffix = selected == 1 || choice.player == ExportPlayer::Standard ? ""
            : choice.player == ExportPlayer::Eclipse194 ? "_Eclipse" : "_xdBot";
        auto profile = writeSelectedMacros(folder, name, levelId, version, edges, tps, endTick, choice, partial, suffix);
        result.written.insert(result.written.end(), profile.written.begin(), profile.written.end());
        for (const auto& error : profile.errors)
            result.errors.push_back(std::string(exportPlayerName(choice.player)) + ": " + error);
    }
    return result;
}
} // namespace p1::macro
