#pragma once
#include "mod/macro_export_selection.hpp"
#include "mod/execution_receipt.hpp"
#include "mod/ui_popup.hpp"
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/async.hpp>
#include <array>
#ifdef GEODE_IS_WINDOWS
#include <shellapi.h>
#endif

namespace p1 {
void uiVerifySavedReplay(GJGameLevel* level, bool coins);

inline bool verifiedSolutionForExport(GJGameLevel* level, bool coins, std::vector<InputCmd>& edges,
                                      int& tps, long long& endTick, std::string& error, bool* needsReplay = nullptr) {
    if (needsReplay) *needsReplay = false;
    const std::string source = uiSolutionPath(level->m_levelID.value(), coins);
    std::ifstream input(source, std::ios::binary);
    if (!macro::readPlan(input, edges, error)) return false;
    tps = solutionTps(source);
    if (!tps) {
        error = "The solution's timebase is unknown. Solve again with this build before exporting.";
        return false;
    }
    if (!verifiedExportPrefix(source, level, edges, tps, error, &endTick)) {
        if (needsReplay) *needsReplay = true;
        writeResult("macro_export_refused: " + error);
        return false;
    }
    return true;
}

namespace export_ui {
namespace ui = gdsolver::ui;
#if GEODE_COMP_GD_VERSION >= 22090
using PopupBase = ui::Popup;
#else
using PopupBase = geode::Popup;
#endif

inline std::string utf8Filename(const std::filesystem::path& path) {
    const auto text = path.filename().generic_u8string();
    return std::string(text.begin(), text.end());
}

struct VerifiedPart {
    partial_export::Request request;
    float percent;
};

class SelectionPopup : public PopupBase {
public:
    static SelectionPopup* create(GJGameLevel* level, bool coins, int tps,
                                  const std::vector<InputCmd>& edges) {
        auto* popup = new SelectionPopup();
        if (popup->init(level, coins, tps, edges)) { popup->autorelease(); return popup; }
        delete popup;
        return nullptr;
    }
    static SelectionPopup* createPartial(GJGameLevel* level, VerifiedPart part) {
        auto* popup = new SelectionPopup();
        popup->m_part = std::move(part);
        popup->m_entrySerial = g_uiEntrySerial;
        const auto& request = popup->m_part->request;
        if (popup->init(level, request.identity.coins, 240, request.prefix.inputs)) {
            popup->autorelease();
            return popup;
        }
        delete popup;
        return nullptr;
    }

protected:
    geode::Ref<GJGameLevel> m_level;
    bool m_coins = false, m_orderSafe = true;
    std::optional<VerifiedPart> m_part;
    unsigned long long m_entrySerial = 0;
    std::array<macro::ExportSelection, 3> m_choices{{
        {macro::ExportPlayer::Standard, true, false},
        {macro::ExportPlayer::Eclipse194, false, false},
        {macro::ExportPlayer::XdBotLab, false, false}
    }};
    std::filesystem::path m_folder;
    CCMenuItemSpriteExtra* m_export = nullptr;
    CCMenuItemSpriteExtra* m_path = nullptr;
    ButtonSprite* m_exportSprite = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    cocos2d::CCLabelTTF* m_folderLabel = nullptr;
    static constexpr const char* kProfiles[] = {"standard", "eclipse", "xdbot"};

    static std::string pathText(const std::filesystem::path& path) {
        const auto text = path.generic_u8string();
        return {text.begin(), text.end()};
    }

    bool init(GJGameLevel* level, bool coins, int tps, const std::vector<InputCmd>& edges) {
        using namespace cocos2d;
        if (!PopupBase::init(390.f, 286.f)) return false;
        PlatformToolbox::showCursor();
        this->setID("export-menu"_spr);
        this->setTitle("Export macros");
        m_level = level;
        m_coins = coins;
        m_orderSafe = macro::gdr2OrderSafe(edges);
        const auto savedPath = Mod::get()->getSavedValue<std::string>("export-folder", "");
        m_folder = savedPath.empty() ? std::filesystem::path(DATA_DIR) / "exports" : std::filesystem::u8path(savedPath);
        auto label = [&](const std::string& text, float x, float y, float scale, const char* font = "chatFont.fnt") {
            auto* node = CCLabelBMFont::create(text.c_str(), font);
            node->setScale(scale);
            ui::limitNodeWidth(node, 350.f, scale, .3f);
            ui::addChildAtPosition(m_mainLayer, node, geode::Anchor::Top, ccp(x, y));
            return node;
        };
        if (m_part) {
            const auto tick = m_part->request.prefix.boundary;
            label(fmt::format("Solved part | {:.3f} s | {:.1f}% | 240 TPS", tick / 240.0, m_part->percent), 0, -45.f, .6f);
        } else {
            label("Full level | " + std::to_string(tps) + " TPS", 0, -45.f, .65f);
        }
        auto name = std::string(level->m_levelName);
        for (char& c : name) if (c == ' ') c = '_';
        label(name, 0, -65.f, .6f)->setID("export-name"_spr);
        label("Player", -105.f, -86.f, .55f);
        label("GDR", 55.f, -86.f, .45f, "bigFont.fnt");
        label("GDR2", 125.f, -86.f, .45f, "bigFont.fnt");
        const char* names[] = {"Standard", "Eclipse 1.9.4", "xdBot (lab)"};
        for (size_t row = 0; row < m_choices.size(); ++row) {
            const float y = -113.f - row * 32.f;
            auto* nameLabel = label(names[row], -82.f, y, .43f, "bigFont.fnt");
            ui::limitNodeWidth(nameLabel, 163.f, .43f, .3f);
            for (size_t col = 0; col < 2; ++col) {
                const size_t index = row * 2 + col;
                const auto key = std::string("export-") + kProfiles[row] + (col ? "-gdr2" : "-gdr");
                bool selected = Mod::get()->getSavedValue<bool>(key, row == 0 && col == 0);
                const bool supported = !col || (m_orderSafe && row != 2);
                if (!supported) selected = false;
                (col ? m_choices[row].gdr2 : m_choices[row].gdr) = selected;
                auto* toggle = CCMenuItemToggler::createWithStandardSprites(this,
                    menu_selector(SelectionPopup::onFormat), .55f);
                toggle->setTag(static_cast<int>(index));
                toggle->setID((std::string(Mod::get()->getID()) + "/" + key).c_str());
                toggle->toggle(selected);
                toggle->setEnabled(supported);
                if (!supported) {
                    toggle->setVisible(false);
                    label("N/A", col ? 125.f : 55.f, y, .5f)->setColor({170, 170, 170});
                }
                ui::addChildAtPosition(m_buttonMenu, toggle, geode::Anchor::Top, ccp(col ? 125.f : 55.f, y));
            }
        }
        m_status = label("", 0, -205.f, .5f);
        m_status->setID("export-status"_spr);
        m_folderLabel = CCLabelTTF::create("", "Arial", 10.f);
        ui::addChildAtPosition(m_mainLayer, m_folderLabel, geode::Anchor::Top, ccp(0, -234.f));
        m_folderLabel->setID("export-folder-label"_spr);
        auto* pathSprite = ButtonSprite::create("Path", "goldFont.fnt", "GJ_button_04.png", .65f);
        m_path = CCMenuItemSpriteExtra::create(pathSprite, this, menu_selector(SelectionPopup::onPath));
        m_path->setID("export-path"_spr);
        ui::addChildAtPosition(m_buttonMenu, m_path, geode::Anchor::Bottom, ccp(-95.f, 29.f));
        m_exportSprite = ButtonSprite::create("Export", "goldFont.fnt", "GJ_button_01.png", .7f);
        m_export = CCMenuItemSpriteExtra::create(m_exportSprite, this, menu_selector(SelectionPopup::onExport));
        m_export->setID("export-selected"_spr);
        ui::addChildAtPosition(m_buttonMenu, m_export, geode::Anchor::Bottom, ccp(75.f, 29.f));
        refresh();
        return true;
    }

    void refresh() {
        size_t count = 0;
        for (const auto& choice : m_choices) count += choice.gdr + choice.gdr2;
        m_export->setEnabled(count > 0);
        m_exportSprite->setColor(count ? cocos2d::ccColor3B{255, 255, 255} : cocos2d::ccColor3B{145, 145, 145});
        const auto text = !count ? "Tick a format for each player you want."
            : !m_orderSafe ? "GDR2 cannot keep this P1/P2 order. Use GDR."
            : m_part ? "Partial macro. Stop playback at the shown time."
            : "Choose several players. xdBot supports GDR only.";
        m_status->setString(text);
        ui::limitNodeWidth(m_status, 350.f, .5f, .35f);
        auto folderName = m_folder.filename();
        if (folderName.empty()) folderName = m_folder.root_path();
        m_folderLabel->setString(("Save to: " + pathText(folderName) + "  (Path to change)").c_str());
        ui::limitNodeWidth(m_folderLabel, 350.f, 1.f, .5f);
    }

    void onFormat(cocos2d::CCObject* sender) {
        auto* toggle = static_cast<CCMenuItemToggler*>(sender);
        const size_t index = static_cast<size_t>(toggle->getTag());
        auto& choice = m_choices[index / 2];
        (index % 2 ? choice.gdr2 : choice.gdr) = !toggle->isToggled();
        refresh();
    }

    void onPath(cocos2d::CCObject*) {
        m_path->setEnabled(false);
        std::error_code ec;
        const auto initial = std::filesystem::is_directory(m_folder, ec)
            ? std::optional<std::filesystem::path>(m_folder) : std::nullopt;
        geode::async::spawn(
            geode::utils::file::pick(geode::utils::file::PickMode::OpenFolder, {initial, {}}),
            [self = geode::Ref<SelectionPopup>(this)](geode::utils::file::PickResult result) {
                if (!self->getParent()) return;
                self->m_path->setEnabled(true);
                PlatformToolbox::showCursor();
                if (result.isErr()) {
                    FLAlertLayer::create("Export folder", "The folder picker could not be opened. Your saved path was kept.", "OK")->show();
                    return;
                }
                auto selected = std::move(result).unwrap();
                if (!selected) return;
                self->m_folder = std::move(*selected);
                Mod::get()->setSavedValue<std::string>("export-folder", pathText(self->m_folder));
                if (Mod::get()->saveData().isErr())
                    FLAlertLayer::create("Export folder", "Folder selected for this session, but its preference could not be saved.", "OK")->show();
                self->refresh();
            });
    }
    void onExport(cocos2d::CCObject*) {
        std::vector<InputCmd> edges;
        int tps = 0;
        long long endTick = -1;
        std::string error;
        if (m_part) {
            auto* layer = PlayLayer::get();
            const auto& request = m_part->request;
            if (m_entrySerial != g_uiEntrySerial || !layer || layer->m_level != m_level.data()
                || !partial_export::matches(layer, request)) {
                FLAlertLayer::create("PathWeaver export", "The level or its settings changed. Verify Solved part again.", "OK")->show();
                return;
            }
            edges = request.prefix.inputs;
            endTick = request.prefix.boundary;
            tps = 240;
        } else if (!verifiedSolutionForExport(m_level.data(), m_coins, edges, tps, endTick, error)) {
            FLAlertLayer::create("PathWeaver export", error, "OK")->show();
            return;
        }
        const auto folder = m_folder;
        const std::vector<macro::ExportSelection> choices(m_choices.begin(), m_choices.end());
        const auto result = macro::writeSelectedProfiles(folder, std::string(m_level->m_levelName),
            m_level->m_levelID.value(), Mod::get()->getVersion().toVString(), edges, tps, endTick, choices, m_part.has_value());
        std::string message = std::to_string(result.written.size()) + " macro file(s) saved | " + std::to_string(tps) + " TPS\n";
        if (m_part) message += fmt::format("Partial | Tick {} | {:.3f} s\n", endTick, endTick / 240.0);
        for (const auto& path : result.written) message += "\n" + utf8Filename(path);
        for (const auto& failure : result.errors) message += "\n" + failure;
        writeResult("macro_export: profiles=selected tps=" + std::to_string(tps) + " inputs=" + std::to_string(edges.size())
            + (m_part ? " partial=1 boundary=" + std::to_string(endTick) : "")
            + " files=" + std::to_string(result.written.size()));
        if (result.written.empty()) {
            FLAlertLayer::create("PathWeaver export", message, "OK")->show();
            return;
        }
        for (size_t row = 0; row < m_choices.size(); ++row) {
            const auto key = std::string("export-") + kProfiles[row];
            Mod::get()->setSavedValue<bool>(key + "-gdr", m_choices[row].gdr);
            Mod::get()->setSavedValue<bool>(key + "-gdr2", m_choices[row].gdr2);
        }
        this->onClose(nullptr);
        geode::createQuickPopup("PathWeaver export", message, "OK", "Open Folder", [folder](auto*, bool open) {
            if (!open) return;
            auto native = folder;
            native.make_preferred();
#ifdef GEODE_IS_WINDOWS
            const bool opened = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", native.c_str(),
                nullptr, nullptr, SW_SHOWNORMAL)) > 32;
#else
            const bool opened = geode::utils::file::openFolder(native);
#endif
            if (!opened)
                FLAlertLayer::create("PathWeaver export", "The macro was saved, but its folder could not be opened.", "OK")->show();
        });
    }
};
} // namespace export_ui

inline void exportSolution(GJGameLevel* level, bool coins, bool offerReplay = true) {
    if (!level) return;
    PlatformToolbox::showCursor();
    std::vector<InputCmd> edges;
    int tps = 0;
    long long endTick = -1;
    std::string error;
    bool needsReplay = false;
    if (!verifiedSolutionForExport(level, coins, edges, tps, endTick, error, &needsReplay)) {
        if (needsReplay && offerReplay && !(botDriving() && !g_sessionOver)) {
            const auto serial = g_uiEntrySerial;
            geode::createQuickPopup("PathWeaver export",
                "Verify the saved solution with a Replay from the start. Export will open after verification.",
                "Cancel", "Verify",
                [level = geode::Ref<GJGameLevel>(level), coins, serial,
                 scene = geode::Ref<cocos2d::CCScene>(cocos2d::CCDirector::sharedDirector()->getRunningScene())](auto*, bool play) {
                    if (play && serial == g_uiEntrySerial
                        && scene.data() == cocos2d::CCDirector::sharedDirector()->getRunningScene())
                        uiVerifySavedReplay(level.data(), coins);
                });
            return;
        }
        FLAlertLayer::create("PathWeaver export", error, "OK")->show();
        return;
    }
    if (auto* popup = export_ui::SelectionPopup::create(level, coins, tps, edges)) popup->show();
}

inline void exportFromPause(PlayLayer* layer, bool coins) {
    if (auto ready=partial_export::ready(layer,coins)) {
        if (auto* popup=export_ui::SelectionPopup::createPartial(layer->m_level,{std::move(*ready),partial_export::verifiedPercent}))
            popup->show();
        return;
    }
    auto part = partial_export::find(layer, coins);
    if (!part) { exportSolution(layer->m_level, coins); return; }
    const bool full = std::filesystem::exists(uiSolutionPath(layer->m_level->m_levelID.value(), coins));
    const auto boundary = part->prefix.boundary;
    const auto serial = g_uiEntrySerial;
    geode::createQuickPopup("Export macro",
        fmt::format("Solved part replays the saved path from the start to tick {} ({:.3f} s).", boundary, boundary / 240.0),
        full ? "Full level" : "Cancel", "Solved part",
        [layer = geode::Ref<PlayLayer>(layer), serial, coins, full, part = std::move(*part)](auto*, bool partial) mutable {
            if (serial != g_uiEntrySerial || PlayLayer::get() != layer.data() || (botDriving() && !g_sessionOver)) return;
            if (partial) partial_export::verify(layer.data(), std::move(part));
            else if (full) exportSolution(layer->m_level, coins);
        });
}
} // namespace p1
