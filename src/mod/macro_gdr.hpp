#pragma once
// GDR1 JSON for the pinned xdBot G3 Lab player. No retiming or frame fixes.
#include "mod/input_command.hpp"
#include <istream>
#include <filesystem>
#include <string_view>

namespace p1::macro {
inline std::filesystem::path exportPath(const std::filesystem::path& folder, const std::string& name) {
    return folder / std::filesystem::u8path(name);
}

inline std::string exportBase(const std::filesystem::path& folder, std::string name,
                              const std::vector<std::string>& suffixes =
                                  {".gdr", ".gdr2", "_xdBotLab.gdr", "_Eclipse194.gdr", "_Eclipse194.gdr2"}) {
    for (char& c : name)
        if (static_cast<unsigned char>(c) < 32 || std::string_view(" <>:\"/\\|?*").find(c) != std::string_view::npos) c = '_';
    for (size_t i = name.size(); i > 0 && name[i - 1] == '.'; --i) name[i - 1] = '_';
    if (name.empty()) name = "Unnamed";
    std::string device = name.substr(0, name.find('.'));
    for (char& c : device) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (device == "CON" || device == "PRN" || device == "AUX" || device == "NUL"
        || (device.size() == 4 && (device.starts_with("COM") || device.starts_with("LPT"))
            && device[3] >= '1' && device[3] <= '9')) name.insert(0, "_");
    std::string base = name;
    for (int n = 1; ; ++n) {
        bool occupied = false;
        for (const auto& suffix : suffixes)
            occupied = occupied || std::filesystem::exists(exportPath(folder, base + suffix));
        if (!occupied) return base;
        base = name + "_" + std::to_string(n);
    }
}

inline bool readPlan(std::istream& in, std::vector<InputCmd>& edges, std::string& error) {
    edges.clear();
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (first && line.starts_with("\xef\xbb\xbf")) line.erase(0, 3);
        first = false;
        const auto start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos || line[start] == '#') continue;
        line = line.substr(start);
        InputCmd edge{};
        if (parseInputCommand(line, edge) != 1) {
            error = "Only continuous input/input2 plans can be exported; attempt sequences are unsupported.";
            return false;
        }
        edges.push_back(edge);
    }
    if (in.bad() || edges.empty()) { error = "The solution is empty or unreadable."; return false; }
    sortInputs(edges);
    return true;
}

inline void jsonString(std::ostream& out, const std::string& text) {
    constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u00" << hex[c >> 4] << hex[c & 15];
        else out << c;
    }
    out << '"';
}

inline bool writeGdr(std::ostream& out, const std::vector<InputCmd>& edges, int tps,
                     int levelId, const std::string& name, const std::string& version) {
    if (edges.empty() || (tps != 240 && tps != 480)) return false;
    int previous = -1;
    for (const auto& e : edges) {
        if (e.step < 0 || e.step < previous) return false;
        previous = e.step;
    }
    out << "{\n  \"version\": 1.0,\n  \"gameVersion\": 2.2081,\n  \"framerate\": " << tps
        << ",\n  \"author\": \"PathWeaver\",\n  \"description\": \"Input-only; xdBot G3 Lab; FRESH_ENTRY\","
           "\n  \"seed\": 0,\n  \"coins\": 0,\n  \"ldm\": false,\n  \"bot\": {\"name\": \"PathWeaver\", \"version\": ";
    jsonString(out, version);
    out << "},\n  \"level\": {\"id\": " << levelId << ", \"name\": ";
    jsonString(out, name);
    out << "},\n  \"inputs\": [\n";
    for (size_t i = 0; i < edges.size(); ++i) {
        const auto& e = edges[i];
        // This player inverts the imported channel before native queueButton.
        out << "    {\"frame\": " << e.step << ", \"btn\": 1, \"2p\": "
            << (e.player1 ? "true" : "false") << ", \"down\": " << (e.down ? "true" : "false")
            << '}' << (i + 1 == edges.size() ? "\n" : ",\n");
    }
    out << "  ],\n  \"frameFixes\": []\n}\n";
    return static_cast<bool>(out);
}
} // namespace p1::macro
