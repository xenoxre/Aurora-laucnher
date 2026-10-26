#include "ui/pages.h"

#include "app.h"
#include "core/log.h"
#include "core/util.h"
#include "ui/components.h"
#include "ui/imgui_bridge.h"
#include "ui/ui_state.h"
#include "ui/ui_theme.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace aurora;
using namespace aurora::ui;
using namespace aurora::ui::theme;

namespace aurora::ui {

void DrawConsole(Bridge* bridge) {
    auto& st = bridge->state();

    Title("Console", "Journaux du lanceur et sortie du jeu.");

    static const char* filters[] = { "Tout", "Info", "Avertissements", "Erreurs", "Jeu" };
    static char search[64] = "";
    int filter = std::clamp(st.logFilter, 0, 4);
    ImGui::SetNextItemWidth(180.0f * Dpi());
    if (ImGui::Combo("##logfilter", &filter, filters, 5)) {
        bridge->setLogFilter(filter);
    }

    ImGui::SameLine();
    bool autoScroll = st.autoScroll;
    if (ImGui::Checkbox("Défilement auto", &autoScroll)) bridge->setAutoScroll(autoScroll);

    SameLineFits(110.0f * Dpi());
    if (GhostButton("Effacer", 110.0f * Dpi(), 30.0f * Dpi())) {
        bridge->clearLogs();
    }
    SameLineFits(110.0f * Dpi());
    if (GhostButton("Journaux", 110.0f * Dpi(), 30.0f * Dpi())) {
        bridge->openFolder(fs::path(Logger::instance().logFilePath()).parent_path());
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ouvrir le dossier des journaux");

    std::vector<const LogEntry*> lines;
    lines.reserve(st.logs.size());
    const std::wstring needle = utf8ToWide(search);
    for (const auto& e : st.logs) {
        const int lvl = (int)e.level;
        if (filter == 1 && lvl < (int)LogLevel::Info) continue;
        if (filter == 2 && lvl != (int)LogLevel::Warn && lvl != (int)LogLevel::Error) continue;
        if (filter == 3 && lvl != (int)LogLevel::Error) continue;
        if (filter == 4 && lvl != (int)LogLevel::Game) continue;
        if (!needle.empty() && !containsNoCase(e.text, needle) &&
            !containsNoCase(e.source, needle))
            continue;
        lines.push_back(&e);
    }

    ImGui::Spacing();
    ImGui::SetNextItemWidth(260.0f * Dpi());
    ImGui::InputTextWithHint("##logsearch", "Filtrer les lignes...", search, sizeof(search));
    ImGui::SameLine();
    ImGui::TextDisabled("%d ligne(s)", (int)lines.size());
    SameLineFits(90.0f * Dpi());
    if (ActionButton("Copier", 90.0f * Dpi(), 30.0f * Dpi())) {
        std::wstring all;
        all.reserve(lines.size() * 80);
        for (const LogEntry* e : lines) {
            all += e->time;
            all += L" [";
            all += e->source;
            all += L"] ";
            all += e->text;
            all += L"\n";
        }
        ImGui::SetClipboardText(wideToUtf8(all).c_str());
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copier les lignes affichées dans le presse-papiers");

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.035f, 0.045f, 0.065f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
    ImGui::BeginChild("##logview", ImVec2(0, 0), true);
    if (lines.empty()) {
        ImGui::TextDisabled("Aucun journal pour ce filtre.");
    }
    for (const LogEntry* e : lines) {
        ImVec4 col(0.78f, 0.84f, 0.92f, 1.0f);
        switch (e->level) {
        case LogLevel::Debug: col = ImVec4(0.48f, 0.55f, 0.65f, 1.0f); break;
        case LogLevel::Success: col = Ok; break;
        case LogLevel::Warn: col = Warn; break;
        case LogLevel::Error: col = Danger; break;
        case LogLevel::Game: col = ImVec4(0.45f, 0.85f, 0.95f, 1.0f); break;
        default: break;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(U(e->time).c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", U(e->source).c_str());
        ImGui::SameLine();
        ImGui::TextUnformatted(U(e->text).c_str());
        ImGui::PopStyleColor();
    }
    if (st.autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

} // namespace aurora::ui
