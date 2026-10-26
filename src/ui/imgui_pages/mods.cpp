#include "ui/pages.h"

#include "app.h"
#include "core/util.h"
#include "ui/components.h"
#include "ui/imgui_bridge.h"
#include "ui/ui_state.h"
#include "ui/ui_theme.h"

#include <string>
#include <vector>

using namespace aurora;
using namespace aurora::ui;
using namespace aurora::ui::theme;

namespace aurora::ui {

void DrawMods(Bridge* bridge) {
    auto& st = bridge->state();
    UiState& ui = State();

    Title("Mods", "Recherchez et installez des mods depuis Modrinth.");

    std::vector<std::wstring> instanceIds;
    std::vector<std::string> instanceNames;
    int current = 0;
    for (size_t i = 0; i < st.instances.size(); ++i) {
        instanceIds.push_back(st.instances[i].id);
        instanceNames.push_back(U(st.instances[i].name));
        if (st.instances[i].id == st.modsInstanceId) current = (int)i;
    }

    if (!instanceNames.empty()) {
        ImGui::TextDisabled("Instance cible");
        ImGui::SetNextItemWidth(230.0f * Dpi());
        std::vector<const char*> ptrs;
        for (auto& n : instanceNames) ptrs.push_back(n.c_str());
        if (ImGui::Combo("##mods_instance", &current, ptrs.data(), (int)ptrs.size())) {
            bridge->setModsInstance(instanceIds[(size_t)current]);
        }
        SameLineFits(300.0f * Dpi());
    }

    ImGui::SetNextItemWidth(300.0f * Dpi());
    ImGui::InputTextWithHint("##modq", "Nom du mod (ex : sodium)", ui.modQuery,
                             sizeof(ui.modQuery));
    SameLineFits(130.0f * Dpi());
    if (PrimaryButton("Rechercher", 130.0f * Dpi(), 34.0f * Dpi())) {
        bridge->searchMods(utf8ToWide(ui.modQuery), true);
    }

    ImGui::Spacing();

    if (st.modsLoading) {
        SpinnerText("Recherche sur Modrinth...");
        return;
    }
    if (!st.modsError.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, Danger);
        ImGui::TextWrapped("Recherche impossible : %s", U(st.modsError).c_str());
        ImGui::PopStyleColor();
        return;
    }
    if (st.instances.empty()) {
        EmptyState("Aucune instance",
                   "Créez d'abord une instance pour installer des mods compatibles.", nullptr,
                   nullptr);
        return;
    }
    if (st.modHits.empty()) {
        EmptyState("Aucun résultat",
                   "Lancez une recherche pour découvrir des mods compatibles avec votre version.",
                   nullptr, nullptr);
        return;
    }

    ImGui::TextDisabled("%d résultat(s) sur %d", (int)st.modHits.size(), st.modTotal);
    ImGui::BeginChild("##mod_list", ImVec2(0, -46.0f * Dpi()), false);
    for (const auto& mod : st.modHits) {
        PushCardStyle();
        ImGui::PushID(U(mod.id).c_str());
        ImGui::BeginChild("##modcard", ImVec2(0, 0), true);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.97f, 1.0f, 1.0f));
        ImGui::TextUnformatted(U(mod.title).c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("par %s", U(mod.author).c_str());
        if (mod.downloads > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("·  %s téléchargements", U(humanCount(mod.downloads)).c_str());
        }
        ImGui::TextWrapped("%s", U(mod.description).c_str());
        ImGui::Spacing();
        if (ActionButton("Installer ce mod", 170.0f * Dpi(), 32.0f * Dpi())) {
            bridge->installMod(mod);
        }
        ImGui::EndChild();
        PopCardStyle();
        ImGui::PopID();
        ImGui::Dummy(ImVec2(0, 2.0f * Dpi()));
    }
    ImGui::EndChild();

    if ((int)st.modHits.size() < st.modTotal) {
        if (ActionButton("Charger plus de résultats", 220.0f * Dpi(), 34.0f * Dpi())) {
            bridge->searchMods(st.modsQuery, false);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("· %d restant(s)", st.modTotal - (int)st.modHits.size());
    } else {
        ImGui::TextDisabled("Fin des résultats.");
    }
}

} // namespace aurora::ui
