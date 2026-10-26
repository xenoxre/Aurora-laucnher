#include "ui/pages.h"

#include "app.h"
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

namespace {

struct VersionFilter {
    int index = 1;
    const char* labels[5] = { "Toutes", "Releases", "Snapshots", "Anciennes", "Installées" };
    char search[64] = "";
};

VersionFilter g_filter;

bool Matches(const VersionEntry& v) {
    if (g_filter.index == 1 && v.type != L"release") return false;
    if (g_filter.index == 2 && v.type != L"snapshot") return false;
    if (g_filter.index == 3 && v.type != L"old_beta" && v.type != L"old_alpha") return false;
    if (g_filter.search[0]) {
        const std::wstring q = utf8ToWide(g_filter.search);
        if (!containsNoCase(v.id, q)) return false;
    }
    return true;
}

} // namespace

void DrawVersions(Bridge* bridge) {
    auto& st = bridge->state();

    Title("Versions", "Installez et gérez les versions de Minecraft.");

    ImGui::SetNextItemWidth(190.0f * Dpi());
    ImGui::Combo("##filter", &g_filter.index, g_filter.labels, 5);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(230.0f * Dpi());
    ImGui::InputTextWithHint("##search", "Rechercher une version...", g_filter.search,
                             sizeof(g_filter.search));
    ImGui::SameLine();
    if (ActionButton("Actualiser", 120.0f * Dpi(), 34.0f * Dpi())) bridge->refreshManifest();
    if (!st.manifestError.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, Danger);
        ImGui::TextWrapped("Manifeste indisponible : %s", U(st.manifestError).c_str());
        ImGui::PopStyleColor();
    }

    if (!st.manifest.latestRelease.empty()) {
        const bool latestInstalled = bridge->isInstalled(st.manifest.latestRelease);
        ImGui::TextDisabled("Dernière version :");
        ImGui::SameLine();
        ImGui::TextUnformatted(U(st.manifest.latestRelease).c_str());
        ImGui::SameLine();
        if (latestInstalled) {
            if (GhostButton("Jouer", 80.0f * Dpi(), 26.0f * Dpi()))
                bridge->playVersion(st.manifest.latestRelease);
        } else {
            if (GhostButton("Installer", 90.0f * Dpi(), 26.0f * Dpi()))
                bridge->installVersion(st.manifest.latestRelease);
        }
    }

    ImGui::Spacing();

    std::vector<const VersionEntry*> visible;
    visible.reserve(st.manifest.versions.size());
    for (const auto& v : st.manifest.versions) {
        if (!Matches(v)) continue;
        if (g_filter.index == 4 && !bridge->isInstalled(v.id)) continue;
        visible.push_back(&v);
    }

    if (st.manifestLoading && st.manifest.versions.empty()) {
        SpinnerText("Chargement du manifeste Mojang...");
        return;
    }
    if (visible.empty()) {
        EmptyState("Aucune version",
                   st.manifest.versions.empty()
                       ? "Le manifeste n'est pas encore chargé. Cliquez sur Actualiser."
                       : "Aucune version ne correspond au filtre ou à la recherche.",
                   nullptr, nullptr);
        return;
    }

    ImGui::TextDisabled("%d version(s) affichée(s)", (int)visible.size());
    const float tableH =
        std::max(160.0f * Dpi(), ImGui::GetContentRegionAvail().y - 4.0f * Dpi());
    if (ImGui::BeginTable("##versions", 4,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                              ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
                          ImVec2(0, tableH))) {
        ImGui::TableSetupColumn("Version", ImGuiTableColumnFlags_WidthStretch, 0.28f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.14f);
        ImGui::TableSetupColumn("Statut", ImGuiTableColumnFlags_WidthStretch, 0.20f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch, 0.38f);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin((int)visible.size());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const VersionEntry& v = *visible[(size_t)i];
                const std::string id = U(v.id);
                const bool installed = bridge->isInstalled(v.id);
                const bool busy = bridge->isBusyVersion(v.id);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(id.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%s", U(v.type).c_str());

                ImGui::TableSetColumnIndex(2);
                if (busy) {
                    ImGui::PushStyleColor(ImGuiCol_Text, Warn);
                    ImGui::TextUnformatted("En cours...");
                    ImGui::PopStyleColor();
                } else if (installed) {
                    ImGui::PushStyleColor(ImGuiCol_Text, Ok);
                    ImGui::TextUnformatted("Installée");
                    ImGui::PopStyleColor();
                } else {
                    ImGui::TextDisabled("Non installée");
                }

                ImGui::TableSetColumnIndex(3);
                ImGui::PushID(id.c_str());
                if (!installed) {
                    ImGui::BeginDisabled(busy);
                    if (ActionButton("Installer", 100.0f * Dpi(), 30.0f * Dpi()))
                        bridge->installVersion(v.id);
                    ImGui::EndDisabled();
                } else {
                    // Narrow windows get a compact menu instead of three clipped buttons.
                    const bool compact = ImGui::GetContentRegionAvail().x < 300.0f * Dpi();
                    if (PrimaryButton("Jouer", 92.0f * Dpi(), 30.0f * Dpi()))
                        bridge->playVersion(v.id);
                    if (!compact) {
                        ImGui::SameLine();
                        if (GhostButton("Désinstaller", 110.0f * Dpi(), 30.0f * Dpi())) {
                            RequestConfirm(ConfirmUninstallVersion, v.id,
                                           L"Désinstaller la version",
                                           L"La version " + v.id +
                                               L" sera supprimée du disque. Les instances qui "
                                               L"l'utilisent ne pourront plus être lancées.");
                        }
                        ImGui::SameLine();
                        if (GhostButton("Dossier", 78.0f * Dpi(), 30.0f * Dpi())) {
                            bridge->openFolder(bridge->dataRoot() / L"versions" / v.id);
                        }
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Ouvrir le dossier de la version installée");
                    } else {
                        ImGui::SameLine();
                        if (GhostButton("...", 40.0f * Dpi(), 30.0f * Dpi()))
                            ImGui::OpenPopup("##rowactions");
                        if (ImGui::BeginPopup("##rowactions")) {
                            if (ImGui::MenuItem("Ouvrir le dossier")) {
                                bridge->openFolder(bridge->dataRoot() / L"versions" / v.id);
                                ImGui::CloseCurrentPopup();
                            }
                            if (ImGui::MenuItem("Désinstaller")) {
                                RequestConfirm(
                                    ConfirmUninstallVersion, v.id,
                                    L"Désinstaller la version",
                                    L"La version " + v.id +
                                        L" sera supprimée du disque. Les instances qui "
                                        L"l'utilisent ne pourront plus être lancées.");
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }
                    }
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
}

} // namespace aurora::ui
