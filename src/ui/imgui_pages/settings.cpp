#include "ui/pages.h"

#include "app.h"
#include "core/log.h"
#include "core/util.h"
#include "ui/components.h"
#include "ui/imgui_bridge.h"
#include "ui/ui_state.h"
#include "ui/ui_theme.h"
#include "version.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

using namespace aurora;
using namespace aurora::ui;
using namespace aurora::ui::theme;

namespace aurora::ui {

// ------------------------------------------------------------------ général ---

void DrawSettingsGeneral(Bridge* bridge, Config& cfg) {
    UiState& ui = State();

    ImGui::TextDisabled("Mémoire allouée au jeu");
    ImGui::Spacing();

    SystemMemoryInfo mem{};
    const bool memOk = getSystemMemoryInfo(mem);
    const uint64_t totalMo = memOk ? mem.totalPhysical / (1024ull * 1024ull) : 0;
    const uint64_t availMo = memOk ? mem.availPhysical / (1024ull * 1024ull) : 0;

    const int osReserve = totalMo >= 16384 ? 4096 : 2048;
    const int safeMax =
        memOk ? (int)std::clamp<uint64_t>(totalMo > (uint64_t)osReserve
                                              ? totalMo - (uint64_t)osReserve
                                              : 512,
                                          512, 131072)
              : 131072;
    int recommended = (int)std::clamp<uint64_t>((totalMo * 40) / 100, 1024, (uint64_t)safeMax);
    if (availMo > 0) recommended = std::min<int>(recommended, (int)(availMo * 80 / 100));
    recommended = (recommended / 256) * 256;
    if (recommended < 512) recommended = 512;

    if (memOk) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.80f, 0.90f, 1.0f));
        ImGui::Text("Mémoire physique : %.1f Go   ·   Disponible : %.1f Go", totalMo / 1024.0,
                    availMo / 1024.0);
        ImGui::PopStyleColor();
        ImGui::TextDisabled(
            "Recommandation pour cette machine : %d Mo (%.1f Go), en gardant ~%d Mo pour Windows.",
            recommended, recommended / 1024.0, osReserve);
    } else {
        ImGui::TextDisabled("Mémoire système : détection indisponible.");
    }

    ImGui::Spacing();

    const int sliderMax = std::clamp(std::max(4096, safeMax), 4096, 32768);
    ImGui::SetNextItemWidth(420.0f * Dpi());
    int ram = std::clamp(cfg.ramMb, 512, 131072);
    if (ImGui::SliderInt("##ram_slider", &ram, 512, sliderMax, "%d Mo")) {
        cfg.ramMb = std::clamp(ram, 512, 131072);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f * Dpi());
    int ramInput = cfg.ramMb;
    if (ImGui::InputInt("##ram_input", &ramInput, 256, 1024)) {
        cfg.ramMb = std::clamp(ramInput, 512, 131072);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Mo");
    ImGui::SameLine();
    if (GhostButton("Utiliser la recommandation", 210.0f * Dpi(), 30.0f * Dpi())) {
        cfg.ramMb = recommended;
    }

    ImGui::TextDisabled("%d Mo = %.2f Go  ·  Xmx est la limite maximale du tas Java.", cfg.ramMb,
                        cfg.ramMb / 1024.0f);
    Hint("La mémoire totale du processus Minecraft peut dépasser Xmx (JVM, bibliothèques natives, "
         "graphismes). Xmx n'est pas une réservation : c'est un plafond pour le tas Java.");

    if (memOk && cfg.ramMb > safeMax) {
        ImGui::PushStyleColor(ImGuiCol_Text, Warn);
        ImGui::TextWrapped("Valeur élevée : au-delà de %d Mo, Windows et les autres applications "
                           "risquent de manquer de mémoire.",
                           safeMax);
        ImGui::PopStyleColor();
    }
    if (memOk && availMo > 0 && (uint64_t)cfg.ramMb > availMo) {
        ImGui::PushStyleColor(ImGuiCol_Text, Danger);
        ImGui::TextWrapped(
            "Attention : %d Mo dépassent la mémoire actuellement disponible (%.0f Mo). Le jeu "
            "pourrait ne pas démarrer.",
            cfg.ramMb, (double)availMo);
        ImGui::PopStyleColor();
    }
    if (cfg.ramMb < 1024) {
        ImGui::TextDisabled("Moins de 1024 Mo est généralement insuffisant pour les versions modernes.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Arguments JVM personnalisés");
    if (!ui.jvmArgsInit) {
        strncpy(ui.jvmArgs, U(cfg.jvmArgs).c_str(), sizeof(ui.jvmArgs) - 1);
        ui.jvmArgs[sizeof(ui.jvmArgs) - 1] = '\0';
        ui.jvmArgsInit = true;
    }
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextMultiline("##jvmargs", ui.jvmArgs, sizeof(ui.jvmArgs),
                              ImVec2(0, 74.0f * Dpi()));
    Hint("Séparés par des espaces. Les options -Xmx et -Xms sont ignorées ici : la mémoire allouée "
         "ci-dessus reste la seule source de vérité pour éviter les conflits.");
    if (GhostButton("Réinitialiser les arguments", 200.0f * Dpi(), 28.0f * Dpi())) {
        ui.jvmArgs[0] = '\0';
        cfg.jvmArgs.clear();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Checkbox("Fermer le lanceur lorsque le jeu démarre", &cfg.closeOnLaunch);

    ImGui::Spacing();
    ImGui::TextDisabled("Résolution du jeu");
    static const char* resolutions[] = { "854 x 480",  "1024 x 576", "1280 x 720",
                                         "1366 x 768", "1600 x 900", "1920 x 1080" };
    static const int sizes[][2] = { { 854, 480 },  { 1024, 576 }, { 1280, 720 },
                                    { 1366, 768 }, { 1600, 900 }, { 1920, 1080 } };
    int resolutionChoice = 2;
    for (int i = 0; i < 6; ++i) {
        if (cfg.width == sizes[i][0] && cfg.height == sizes[i][1]) resolutionChoice = i;
    }
    ImGui::SetNextItemWidth(200.0f * Dpi());
    if (ImGui::Combo("##res", &resolutionChoice, resolutions, 6)) {
        cfg.width = sizes[resolutionChoice][0];
        cfg.height = sizes[resolutionChoice][1];
    }
}

// ---------------------------------------------------------------------- java ---

void DrawSettingsJava(Bridge* bridge, Config& cfg) {
    auto& st = bridge->state();

    ImGui::TextDisabled("Java utilisé pour lancer le jeu");
    Hint("Laissez « Automatique » pour que le lanceur choisisse une version compatible et la "
         "télécharge si nécessaire. Une sélection manuelle est prioritaire.");

    ImGui::Spacing();
    const bool automatic = cfg.javaPath.empty();
    if (ImGui::RadioButton("Automatique (recommandé)", automatic)) {
        cfg.javaPath.clear();
        bridge->saveConfig();
    }
    if (!st.javaList.empty() && automatic) {
        ImGui::SameLine();
        ImGui::TextDisabled("· %d installation(s) détectée(s)", (int)st.javaList.size());
    }

    if (st.javaLoading) {
        SpinnerText("Détection des installations Java...");
    } else if (st.javaList.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("Aucune installation Java détectée sur cette machine.");
        ImGui::TextDisabled(
            "Le lanceur peut en télécharger une automatiquement au premier lancement.");
    } else {
        ImGui::Spacing();
        for (const auto& j : st.javaList) {
            ImGui::PushID(U(j.path).c_str());
            const bool selected = !cfg.javaPath.empty() && cfg.javaPath == j.path;
            if (ImGui::RadioButton(U(L"Java " + std::to_wstring(j.major)).c_str(), selected)) {
                cfg.javaPath = j.path;
                bridge->saveConfig();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", U(j.version).c_str());
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.58f, 0.66f, 0.78f, 1.0f));
            ImGui::TextUnformatted(U(j.path).c_str());
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }

    ImGui::Spacing();
    if (ActionButton("Détecter à nouveau", 170.0f * Dpi(), 32.0f * Dpi())) bridge->detectJava();
    ImGui::SameLine();
    ImGui::TextDisabled(
        "Java 8 : versions ≤ 1.16  ·  Java 17 : 1.17 à 1.20  ·  Java 21 : 1.20.5+");
}

// ------------------------------------------------------------------ comptes ---

void DrawSettingsAccounts(Bridge* bridge, Config& cfg) {
    ImGui::TextDisabled("Comptes");
    ImGui::Spacing();

    const float listH = std::min(220.0f * Dpi(), ImGui::GetContentRegionAvail().y * 0.5f);
    ImGui::BeginChild("##accounts", ImVec2(0, listH), true);
    if (cfg.accounts.empty()) {
        ImGui::TextDisabled("Aucun compte configuré.");
    }
    for (size_t i = 0; i < cfg.accounts.size(); ++i) {
        auto& a = cfg.accounts[i];
        ImGui::PushID((int)i);
        const bool active = ((int)i == cfg.accountIndex);
        if (ImGui::RadioButton(U(a.name).c_str(), active)) {
            cfg.accountIndex = (int)i;
            bridge->saveConfig();
        }
        ImGui::SameLine();
        ImGui::TextDisabled(a.isMicrosoft() ? "Microsoft" : "Hors ligne");
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 96.0f * Dpi());
        if (GhostButton("Retirer", 84.0f * Dpi(), 26.0f * Dpi())) {
            RequestConfirm(ConfirmRemoveAccount, std::to_wstring(i), L"Retirer ce compte",
                           L"Le compte « " + a.name + L" » sera retiré du lanceur.");
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::EndChild();

    UiState& ui = State();
    ImGui::Spacing();
    if (ActionButton("Ajouter un compte hors ligne", 240.0f * Dpi(), 34.0f * Dpi()))
        ui.openOffline = true;
    ImGui::SameLine();
    if (ActionButton("Se connecter avec Microsoft", 240.0f * Dpi(), 34.0f * Dpi())) {
        ui.openMs = true;
        bridge->startMicrosoftLogin();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Identifiant client Microsoft (Azure)");
    static char clientId[96] = "";
    if (clientId[0] == '\0' && !cfg.msClientId.empty()) {
        strncpy(clientId, U(cfg.msClientId).c_str(), sizeof(clientId) - 1);
        clientId[sizeof(clientId) - 1] = '\0';
    }
    ImGui::SetNextItemWidth(420.0f * Dpi());
    ImGui::InputTextWithHint("##msclient", "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx", clientId,
                             sizeof(clientId));
    ImGui::SameLine();
    if (ActionButton("Enregistrer l'ID", 150.0f * Dpi(), 32.0f * Dpi())) {
        cfg.msClientId = utf8ToWide(clientId);
        bridge->saveConfig();
    }
    Hint("Nécessaire uniquement pour la connexion Microsoft. L'identifiant reste stocké "
         "localement.");
}

// ------------------------------------------------------------------- réseau ---

void DrawSettingsNetwork(Bridge* bridge) {
    ImGui::TextDisabled("Réseau et confidentialité");
    ImGui::Spacing();
    ImGui::TextWrapped("Les téléchargements (Minecraft, Fabric/Quilt, Modrinth) utilisent WinHTTP "
                       "et le protocole HTTPS.");
    ImGui::Spacing();
    InfoRow("User-Agent", std::string("AuroraLauncher/") + AURORA_VERSION);
    InfoRow("Télémétrie", "Aucune");
    InfoRow("Dossier de données", U(bridge->dataRoot().wstring()));
    ImGui::Spacing();
    if (ActionButton("Ouvrir le dossier de données", 240.0f * Dpi(), 32.0f * Dpi()))
        bridge->openDataFolder();
    ImGui::Spacing();
    Hint("Aucune donnée personnelle n'est envoyée à un serveur tiers par le lanceur. Les requêtes "
         "sont uniquement celles nécessaires à l'installation et à l'authentification.");
}

// ---------------------------------------------------------------- à propos ---

void DrawSettingsAbout(Bridge* bridge) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    BrandMark(dl, ImVec2(p.x + 26.0f * Dpi(), p.y + 26.0f * Dpi()), 36.0f * Dpi());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 62.0f * Dpi());
    if (TitleFont()) ImGui::PushFont(TitleFont());
    ImGui::TextUnformatted("Aurora Launcher");
    if (TitleFont()) ImGui::PopFont();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 62.0f * Dpi());
    ImGui::TextDisabled("Version " AURORA_VERSION);

    ImGui::Dummy(ImVec2(0, 34.0f * Dpi()));
    ImGui::TextWrapped("Lanceur Minecraft natif en C++ pour Windows, avec une interface "
                       "Dear ImGui et DirectX 11.");
    ImGui::Spacing();
    Hint("Fabric / Quilt · Microsoft OAuth · Modrinth · WinHTTP · GDI+");
    ImGui::Spacing();
    if (ActionButton("Ouvrir le dossier de données", 240.0f * Dpi(), 34.0f * Dpi()))
        bridge->openDataFolder();
    ImGui::SameLine();
    if (ActionButton("Dossier des journaux", 240.0f * Dpi(), 34.0f * Dpi()))
        bridge->openFolder(fs::path(Logger::instance().logFilePath()).parent_path());

    // Mises à jour -------------------------------------------------------------
    auto& st = bridge->state();
    auto& cfg = bridge->config();

    ImGui::Dummy(ImVec2(0, 26.0f * Dpi()));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 12.0f * Dpi()));
    ImGui::TextDisabled("MISES À JOUR");
    ImGui::Spacing();
    ImGui::Text("Version actuelle : " AURORA_VERSION);

    if (st.updateChecking) {
        SpinnerText("Vérification des mises à jour…");
    } else if (!st.updateError.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, Danger);
        ImGui::TextWrapped("%s", U(st.updateError).c_str());
        ImGui::PopStyleColor();
    } else if (st.updateAvailable) {
        ImGui::PushStyleColor(ImGuiCol_Text, Accent);
        ImGui::Text("Version %s disponible", U(st.updateVersion).c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Téléchargement vérifié par empreinte SHA-256.");
        if (!st.updateNotes.empty()) {
            ImGui::BeginChild("##update_notes_settings", ImVec2(0, 92.0f * Dpi()), true);
            ImGui::TextWrapped("%s", U(st.updateNotes).c_str());
            ImGui::EndChild();
        }
    } else {
        ImGui::TextDisabled("%s", st.updateStatus.empty() ? "Aurora est à jour."
                                                          : U(st.updateStatus).c_str());
    }

    if (st.updateDownloading) {
        ImGui::Spacing();
        ImGui::ProgressBar(st.updateProgress, ImVec2(0, 10.0f * Dpi()), "");
        ImGui::TextDisabled("%s", U(st.updateStatus).c_str());
        if (GhostButton("Annuler le téléchargement", 230.0f * Dpi(), 32.0f * Dpi()))
            bridge->cancelUpdate();
    } else if (st.updateReady) {
        ImGui::PushStyleColor(ImGuiCol_Text, Ok);
        ImGui::TextUnformatted("Mise à jour téléchargée, prête à être installée.");
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    if (st.updateReady) {
        if (PrimaryButton("Redémarrer et installer", 240.0f * Dpi(), 38.0f * Dpi()))
            bridge->installUpdate();
    } else if (st.updateAvailable && !st.updateDownloading) {
        if (PrimaryButton("Télécharger la mise à jour", 240.0f * Dpi(), 38.0f * Dpi()))
            bridge->downloadUpdate();
        ImGui::SameLine();
        if (GhostButton("Ignorer cette version", 190.0f * Dpi(), 38.0f * Dpi()))
            bridge->skipUpdateVersion();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Ne plus proposer cette version au démarrage");
    }

    ImGui::Spacing();
    bool checkUpdates = cfg.checkUpdates;
    if (ImGui::Checkbox("Vérifier les mises à jour au démarrage", &checkUpdates))
        bridge->setCheckUpdates(checkUpdates);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Une requête HTTPS vers le dépôt GitHub, aucune télémétrie.");
    ImGui::SameLine();
    ImGui::BeginDisabled(st.updateChecking || st.updateDownloading);
    if (GhostButton("Vérifier maintenant", 180.0f * Dpi(), 32.0f * Dpi()))
        bridge->checkUpdates(true);
    ImGui::EndDisabled();
}

// -------------------------------------------------------------------- page ---

void DrawSettings(Bridge* bridge) {
    auto& cfg = bridge->config();
    UiState& ui = State();

    Title("Paramètres", "Mémoire, Java, comptes et préférences du lanceur.");

    if (ImGui::BeginTabBar("##settings_tabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Général")) {
            DrawSettingsGeneral(bridge, cfg);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Java")) {
            DrawSettingsJava(bridge, cfg);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Comptes")) {
            DrawSettingsAccounts(bridge, cfg);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Réseau")) {
            DrawSettingsNetwork(bridge);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("À propos")) {
            DrawSettingsAbout(bridge);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Spacing();
    if (PrimaryButton("Enregistrer", 150.0f * Dpi(), 38.0f * Dpi())) {
        cfg.ramMb = std::clamp(cfg.ramMb, 512, 131072);
        cfg.jvmArgs = trim(utf8ToWide(ui.jvmArgs));
        bridge->saveConfig();
        ui.settingsSavedAt = ImGui::GetTime();
    }
    ImGui::SameLine();
    if (GhostButton("Ouvrir le dossier de données", 240.0f * Dpi(), 38.0f * Dpi()))
        bridge->openDataFolder();
    if (ImGui::GetTime() - ui.settingsSavedAt < 2.5) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, Ok);
        ImGui::TextUnformatted("Paramètres enregistrés");
        ImGui::PopStyleColor();
    }
}

} // namespace aurora::ui
