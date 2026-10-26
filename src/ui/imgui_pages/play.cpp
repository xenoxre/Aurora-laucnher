#include "ui/pages.h"

#include "app.h"
#include "core/util.h"
#include "ui/components.h"
#include "ui/imgui_bridge.h"
#include "ui/ui_state.h"
#include "ui/ui_theme.h"

#include <algorithm>
#include <cstdio>
#include <string>

using namespace aurora;
using namespace aurora::ui;
using namespace aurora::ui::theme;

namespace aurora::ui {

void DrawPlay(Bridge* bridge) {
    auto& st = bridge->state();
    auto& cfg = bridge->config();
    UiState& ui = State();

    Title("Jouer", "Lancez votre instance Minecraft en un clic.");

    if (st.instances.empty()) {
        bool create = false;
        EmptyState("Aucune instance pour le moment",
                   "Créez une instance pour choisir une version, un chargeur de mods et lancer le jeu.",
                   "Créer une instance", &create);
        if (create) ui.openNewInstance = true;
        return;
    }

    PushCardStyle();
    ImGui::BeginChild("##play_main", ImVec2(0, 0), true);

    ImGui::TextDisabled("Instance sélectionnée");
    ImGui::SetNextItemWidth(-1.0f);
    int selected = 0;
    for (size_t i = 0; i < st.instances.size(); ++i) {
        if (st.instances[i].id == cfg.selectedInstance) selected = (int)i;
    }
    if (ImGui::BeginCombo("##instance", U(st.instances[selected].name).c_str())) {
        for (size_t i = 0; i < st.instances.size(); ++i) {
            const bool isSel = (int)i == selected;
            if (ImGui::Selectable(U(st.instances[i].name).c_str(), isSel)) {
                bridge->selectInstance(st.instances[i].id);
            }
            if (isSel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Dummy(ImVec2(0, 4.0f * Dpi()));
    const Instance& inst = st.instances[selected];
    InfoRow("Version Minecraft", U(inst.mcVersion));
    std::wstring loader = inst.loader.empty() || inst.loader == L"none" ? L"Vanilla" : inst.loader;
    if (inst.loader != L"none" && !inst.loaderVersion.empty())
        loader += L" " + inst.loaderVersion;
    InfoRow("Chargeur", U(loader));
    InfoRow("Mémoire allouée (Xmx)", std::to_string(cfg.ramMb) + " Mo (" +
                                        formatA("%.1f", cfg.ramMb / 1024.0f) + " Go)");

    ImGui::Dummy(ImVec2(0, 10.0f * Dpi()));

    const bool busy = bridge->isBusyVersion(inst.versionId);
    if (st.gameRunning) {
        ImGui::PushStyleColor(ImGuiCol_Text, Ok);
        ImGui::TextUnformatted("Minecraft est en cours d'exécution.");
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Instance : %s", U(st.gameInstance).c_str());
    } else if (busy) {
        SpinnerText("Préparation de l'instance...");
    } else if (PrimaryButton("Jouer", 220.0f * Dpi(), 48.0f * Dpi())) {
        bridge->launchSelected();
    }

    SameLineFits(180.0f * Dpi());
    if (ActionButton("Nouvelle instance", 180.0f * Dpi(), 40.0f * Dpi())) ui.openNewInstance = true;

    SameLineFits(180.0f * Dpi());
    if (GhostButton("Gérer les versions", 180.0f * Dpi(), 40.0f * Dpi())) {
        Navigate(bridge, L"versions");
    }

    SameLineFits(100.0f * Dpi());
    if (GhostButton("Dossier", 100.0f * Dpi(), 40.0f * Dpi())) {
        bridge->openFolder(bridge->dataRoot() / L"instances" / inst.id);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ouvrir le dossier de l'instance");

    if (!bridge->isInstalled(inst.versionId)) {
        ImGui::TextDisabled(
            "Cette version n'est pas encore installée : elle le sera automatiquement au lancement.");
    }

    if (st.instances.size() > 1) {
        ImGui::Spacing();
        if (DangerButton("Supprimer cette instance", ImVec2(220.0f * Dpi(), 36.0f * Dpi()))) {
            RequestConfirm(ConfirmDeleteInstance, inst.id, L"Supprimer l'instance",
                           L"L'instance « " + inst.name +
                               L" » et son dossier seront supprimés. Cette action est irréversible.");
        }
    }

    ImGui::EndChild();
    PopCardStyle();

    ImGui::Dummy(ImVec2(0, 8.0f * Dpi()));

    PushCardStyle();
    ImGui::BeginChild("##play_account", ImVec2(0, 0), true);
    ImGui::TextDisabled("Compte utilisé");
    const Account& acc = cfg.activeAccount();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.95f, 1.0f, 1.0f));
    ImGui::TextUnformatted(acc.name.empty() ? "Aucun compte configuré" : U(acc.name).c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled(acc.isMicrosoft() ? "(Microsoft)" : "(hors ligne)");
    if (GhostButton("Gérer les comptes", 170.0f * Dpi(), 32.0f * Dpi()))
        Navigate(bridge, L"settings");
    ImGui::EndChild();
    PopCardStyle();
}

} // namespace aurora::ui
