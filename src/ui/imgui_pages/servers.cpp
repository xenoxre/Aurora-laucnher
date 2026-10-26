#include "ui/pages.h"

#include "app.h"
#include "core/util.h"
#include "ui/components.h"
#include "ui/imgui_bridge.h"
#include "ui/ui_state.h"
#include "ui/ui_theme.h"

#include <string>

using namespace aurora;
using namespace aurora::ui;
using namespace aurora::ui::theme;

namespace aurora::ui {

namespace {
char g_pingAddress[128] = "play.hypixel.net";
}

void DrawServers(Bridge* bridge) {
    auto& st = bridge->state();
    auto& cfg = bridge->config();
    UiState& ui = State();

    Title("Serveurs", "Rejoignez un serveur ou hébergez-en un localement.");

    PushCardStyle();
    ImGui::BeginChild("##ping", ImVec2(0, 0), true);
    ImGui::TextDisabled("Adresse du serveur");
    ImGui::SetNextItemWidth(320.0f * Dpi());
    ImGui::InputText("##addr", g_pingAddress, sizeof(g_pingAddress));
    ImGui::SameLine();
    if (ActionButton("Pinguer", 100.0f * Dpi(), 34.0f * Dpi())) {
        bridge->pingServer(utf8ToWide(g_pingAddress));
    }
    ImGui::SameLine();
    if (PrimaryButton("Rejoindre", 120.0f * Dpi(), 34.0f * Dpi())) {
        bridge->joinServer(utf8ToWide(g_pingAddress));
    }

    if (st.pingLoading) {
        SpinnerText("Connexion au serveur...");
    } else if (!st.pingError.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, Danger);
        ImGui::TextWrapped("%s", U(st.pingError).c_str());
        ImGui::PopStyleColor();
    } else if (st.pingStatus.ok) {
        ImGui::Spacing();
        InfoRow("MOTD", U(stripMinecraftColors(st.pingStatus.motd)));
        InfoRow("Joueurs", formatA("%d / %d", st.pingStatus.online, st.pingStatus.maxPlayers));
        InfoRow("Version", U(st.pingStatus.versionName));
        InfoRow("Latence", formatA("%d ms", st.pingStatus.pingMs));
    }
    ImGui::EndChild();
    PopCardStyle();

    ImGui::Spacing();
    ImGui::TextDisabled("Serveurs locaux");
    ImGui::SameLine();
    if (ActionButton("Créer un serveur", 150.0f * Dpi(), 30.0f * Dpi())) ui.openServer = true;

    if (st.localServerRunning) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, Ok);
        ImGui::Text("·  %s en ligne", U(st.localServerName).c_str());
        ImGui::PopStyleColor();
        SameLineFits(100.0f * Dpi());
        if (GhostButton("Arrêter", 100.0f * Dpi(), 30.0f * Dpi())) {
            RequestConfirm(ConfirmStopServer, {}, L"Arrêter le serveur local",
                           L"Le serveur local va être arrêté et les joueurs connectés seront "
                           L"déconnectés.");
        }
    }

    ImGui::Spacing();
    if (cfg.localServers.empty()) {
        Hint("Aucun serveur local. Vous pouvez en créer un pour jouer entre amis sur votre réseau.");
        return;
    }

    for (size_t i = 0; i < cfg.localServers.size(); ++i) {
        auto& s = cfg.localServers[i];
        ImGui::PushID((int)i);
        PushCardStyle();
        ImGui::BeginChild("##srv", ImVec2(0, 0), true);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.97f, 1.0f, 1.0f));
        ImGui::TextUnformatted(U(s.name).c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Minecraft %s · port %d · %d Mo · %s", U(s.version).c_str(), s.port,
                            s.ramMb,
                            s.onlineMode ? "authentification Mojang" : "hors ligne");
        ImGui::Spacing();
        if (st.localServerRunning && st.localServerName == s.name) {
            if (GhostButton("Arrêter", 120.0f * Dpi(), 32.0f * Dpi()))
                RequestConfirm(ConfirmStopServer, {}, L"Arrêter le serveur local",
                               L"Le serveur local va être arrêté et les joueurs connectés seront "
                               L"déconnectés.");
        } else {
            if (PrimaryButton("Démarrer", 120.0f * Dpi(), 32.0f * Dpi()))
                bridge->startLocalServer(s);
        }
        SameLineFits(160.0f * Dpi());
        if (GhostButton("Ouvrir le dossier", 160.0f * Dpi(), 32.0f * Dpi()))
            bridge->openFolder(bridge->dataRoot() / L"servers" / s.dir);
        SameLineFits(90.0f * Dpi());
        if (GhostButton("Adresse", 90.0f * Dpi(), 32.0f * Dpi())) {
            const std::string addr = "localhost:" + std::to_string(s.port);
            ImGui::SetClipboardText(addr.c_str());
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Copier localhost:%d dans le presse-papiers", s.port);
        SameLineFits(80.0f * Dpi());
        if (GhostButton("Retirer", 80.0f * Dpi(), 32.0f * Dpi())) {
            RequestConfirm(ConfirmRemoveLocalServer, s.dir, L"Retirer ce serveur",
                           L"Le serveur « " + s.name +
                               L" » sera retiré de la liste. Ses fichiers restent sur le disque.");
        }
        ImGui::EndChild();
        PopCardStyle();
        ImGui::PopID();
        ImGui::Dummy(ImVec2(0, 2.0f * Dpi()));
    }
}

} // namespace aurora::ui
