// Aurora Launcher - Dear ImGui host, navigation shell and dialog host.
//
// This translation unit owns the ImGui context (Win32 + DirectX 11), the sidebar,
// the modal dialogs and the toast overlay. Page content lives in
// src/ui/imgui_pages/*.cpp and shared visual helpers in ui_theme.*.
#include "ui/imgui_ui.h"

#include "app.h"
#include "core/util.h"
#include "ui/components.h"
#include "ui/imgui_bridge.h"
#include "ui/pages.h"
#include "ui/ui_state.h"
#include "ui/ui_theme.h"
#include "version.h"

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include <windows.h>

using namespace aurora;
using namespace aurora::ui;
using namespace aurora::ui::theme;

namespace {

Bridge* g_bridge = nullptr;

// ------------------------------------------------------------------ fonts ---

float DetectDpi(void* hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        typedef UINT(WINAPI * GetDpiFn)(HWND);
        auto getDpi = (GetDpiFn)GetProcAddress(user32, "GetDpiForWindow");
        if (getDpi) {
            UINT dpi = getDpi((HWND)hwnd);
            if (dpi >= 96) return (float)dpi / 96.0f;
        }
    }
    HDC dc = GetDC(nullptr);
    if (dc) {
        int dpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(nullptr, dc);
        if (dpi >= 96) return (float)dpi / 96.0f;
    }
    return 1.0f;
}

std::wstring WindowsFontPath(const wchar_t* file) {
    wchar_t dir[MAX_PATH] = {};
    UINT n = GetWindowsDirectoryW(dir, MAX_PATH);
    if (n == 0) return {};
    std::wstring p(dir, n);
    p += L"\\Fonts\\";
    p += file;
    return p;
}

ImFont* LoadUiFont(float pixels, const ImWchar* ranges) {
    const std::wstring path = WindowsFontPath(L"segoeui.ttf");
    if (!path.empty() && pathExists(fs::path(path))) {
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;
        cfg.PixelSnapH = false;
        const std::string utf8 = wideToUtf8(path);
        if (ImFont* f = ImGui::GetIO().Fonts->AddFontFromFileTTF(utf8.c_str(), pixels, &cfg, ranges))
            return f;
    }
    return nullptr;
}

// ---------------------------------------------------------------- sidebar ---

bool NavRow(const char* id, const char* label, int icon, bool active, float width, int shortcut,
            ImVec2* center) {    ImGui::PushID(id);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = 44.0f * Dpi();
    ImGui::InvisibleButton("##row", ImVec2(width, h));
    const bool hovered = ImGui::IsItemHovered();
    const bool pressed = ImGui::IsItemClicked();
    if (hovered && !active) ImGui::SetTooltip("%s  ·  Ctrl+%d", label, shortcut);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (active) {
        dl->AddRectFilled(p, ImVec2(p.x + width, p.y + h), IM_COL32(38, 74, 120, 255), 10.0f);
        dl->AddRectFilled(p, ImVec2(p.x + 3.0f * Dpi(), p.y + h), IM_COL32(82, 220, 204, 255),
                          2.0f);
    } else if (hovered) {
        dl->AddRectFilled(p, ImVec2(p.x + width, p.y + h), IM_COL32(30, 42, 62, 255), 10.0f);
    }

    const ImU32 iconCol = active   ? IM_COL32(150, 240, 230, 255)
                          : hovered ? IM_COL32(190, 205, 225, 255)
                                    : IM_COL32(130, 148, 172, 255);
    const ImU32 textCol = active   ? IM_COL32(240, 252, 250, 255)
                          : hovered ? IM_COL32(226, 234, 246, 255)
                                    : IM_COL32(168, 184, 206, 255);
    NavIcon(dl, ImVec2(p.x + 22.0f * Dpi(), p.y + h * 0.5f), 17.0f * Dpi(), icon, iconCol);
    dl->AddText(ImVec2(p.x + 42.0f * Dpi(), p.y + (h - ImGui::GetTextLineHeight()) * 0.5f),
                textCol, label);

    if (center) {
        center->x = p.x + width * 0.5f;
        center->y = p.y + h * 0.5f;
    }
    ImGui::PopID();
    return pressed;
}

// F5 / bouton « Rafraîchir » : chaque page a sa source de vérité.
void RefreshCurrentPage(Bridge* bridge) {
    if (!bridge) return;
    const std::wstring& page = State().page;
    if (page == L"versions")
        bridge->refreshManifest();
    else if (page == L"settings")
        bridge->detectJava();
    else
        bridge->refresh();
}

void DrawSidebar(Bridge* bridge) {
    UiState& ui = State();

    ImGui::BeginChild("##sidebar", ImVec2(216.0f * Dpi(), 0), true);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f * Dpi());

    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        BrandMark(dl, ImVec2(p.x + 16.0f * Dpi(), p.y + 12.0f * Dpi()), 20.0f * Dpi());
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 38.0f * Dpi());
        ImGui::PushStyleColor(ImGuiCol_Text, Accent);
        if (TitleFont()) ImGui::PushFont(TitleFont());
        ImGui::TextUnformatted("AURORA");
        if (TitleFont()) ImGui::PopFont();
        ImGui::PopStyleColor();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 38.0f * Dpi());
        ImGui::TextDisabled("LAUNCHER  v" AURORA_VERSION);
        ImGui::Dummy(ImVec2(0, 6.0f * Dpi()));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 6.0f * Dpi()));
    }

    const float navW = 192.0f * Dpi();

    struct NavDef {
        const char* id;
        const char* label;
        int icon;
        const wchar_t* page;
    };
    static const NavDef defs[6] = {
        { "nav_play", "Jouer", 0, L"play" },       { "nav_versions", "Versions", 1, L"versions" },
        { "nav_mods", "Mods", 2, L"mods" },        { "nav_servers", "Serveurs", 3, L"servers" },
        { "nav_console", "Console", 4, L"console" }, { "nav_settings", "Paramètres", 5, L"settings" },
    };

    for (int i = 0; i < 6; ++i) {
        const bool active = ui.page == defs[i].page;
        ImVec2 c;
        if (NavRow(defs[i].id, defs[i].label, defs[i].icon, active, navW, i + 1, &c)) {
            Navigate(bridge, defs[i].page);
        }
        ui.navCenters[i] = c;
        ui.navCentersReady = true;
        ImGui::Dummy(ImVec2(0, 4.0f * Dpi()));
    }

    // Account + status pinned to the bottom of the sidebar.
    const float footerY = ImGui::GetWindowHeight() - 160.0f * Dpi();
    if (footerY > ImGui::GetCursorPosY()) ImGui::SetCursorPosY(footerY);
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4.0f * Dpi()));

    if (bridge) {
        auto& st = bridge->state();
        auto& config = bridge->config();
        const Account& acc = config.activeAccount();
        ImGui::TextDisabled("Compte");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.94f, 0.99f, 1.0f));
        ImGui::TextUnformatted(acc.name.empty() ? "Aucun compte" : U(acc.name).c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled(acc.isMicrosoft() ? "Microsoft" : "Hors ligne");

        if (st.gameRunning) {
            ImGui::PushStyleColor(ImGuiCol_Text, Ok);
            ImGui::TextUnformatted("Jeu en cours");
            ImGui::PopStyleColor();
        } else if (st.localServerRunning) {
            ImGui::PushStyleColor(ImGuiCol_Text, Warn);
            ImGui::TextUnformatted("Serveur local actif");
            ImGui::PopStyleColor();
        }

        ImGui::Dummy(ImVec2(0, 4.0f * Dpi()));
        if (GhostButton("Rafraîchir", 108.0f * Dpi(), 28.0f * Dpi())) RefreshCurrentPage(bridge);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Actualiser la page courante (F5)");
        ImGui::SameLine();
        ImGui::TextDisabled("F5");
    }

    ImGui::EndChild();
}

// ----------------------------------------------------------------- modals ---

void DrawConfirmModal(Bridge* bridge) {
    UiState& ui = State();

    if (ui.confirm.request) {
        ImGui::OpenPopup("Confirmation");
        ui.confirm.request = false;
    }
    if (ImGui::BeginPopupModal("Confirmation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.98f, 1.0f, 1.0f));
        ImGui::TextUnformatted(U(ui.confirm.title).c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::TextWrapped("%s", U(ui.confirm.message).c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (DangerButton("Confirmer", ImVec2(140.0f * Dpi(), 36.0f * Dpi()))) {
            switch (ui.confirm.action) {
            case ConfirmUninstallVersion:
                bridge->uninstallVersion(ui.confirm.payload);
                break;
            case ConfirmDeleteInstance:
                bridge->deleteInstance(ui.confirm.payload);
                break;
            case ConfirmRemoveAccount:
                bridge->removeAccount(_wtoi(ui.confirm.payload.c_str()));
                break;
            case ConfirmStopServer:
                bridge->stopLocalServer();
                break;
            case ConfirmRemoveLocalServer: {
                auto& cfg = bridge->config();
                const std::wstring& dir = ui.confirm.payload;
                auto it = std::find_if(cfg.localServers.begin(), cfg.localServers.end(),
                                       [&](const LocalServerConfig& s) { return s.dir == dir; });
                if (it != cfg.localServers.end()) {
                    if (bridge->state().localServerRunning &&
                        bridge->state().localServerName == it->name) {
                        bridge->stopLocalServer();
                    }
                    cfg.localServers.erase(it);
                    bridge->saveConfig();
                }
                break;
            }
            default:
                break;
            }
            ui.confirm.action = ConfirmNone;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (GhostButton("Annuler", 140.0f * Dpi(), 36.0f * Dpi())) {
            ui.confirm.action = ConfirmNone;
            ImGui::CloseCurrentPopup();
        }
        // Safer default: Enter activates "Annuler", never the destructive action.
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }
}

void DrawModals(Bridge* bridge) {
    auto& st = bridge->state();
    UiState& ui = State();

    if (ui.openNewInstance) {
        ImGui::OpenPopup("Nouvelle instance");
        ui.openNewInstance = false;
    }
    if (ImGui::BeginPopupModal("Nouvelle instance", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextDisabled("Nom de l'instance");
        ImGui::SetNextItemWidth(300.0f * Dpi());
        ImGui::InputText("##name", ui.newName, sizeof(ui.newName));

        std::vector<std::string> names;
        std::vector<std::wstring> ids;
        for (auto& v : st.manifest.versions) {
            if (v.type != L"release") continue;
            names.push_back(U(v.id));
            ids.push_back(v.id);
        }
        if (names.empty()) {
            names.push_back("1.21.1");
            ids.push_back(L"1.21.1");
        }
        std::vector<const char*> ptrs;
        for (auto& n : names) ptrs.push_back(n.c_str());
        if (ui.newVersion < 0 || ui.newVersion >= (int)ptrs.size()) ui.newVersion = 0;

        ImGui::TextDisabled("Version de Minecraft");
        ImGui::SetNextItemWidth(300.0f * Dpi());
        ImGui::Combo("##version", &ui.newVersion, ptrs.data(), (int)ptrs.size());

        ImGui::TextDisabled("Chargeur de mods");
        const char* loaders[] = { "Vanilla", "Fabric", "Quilt" };
        ImGui::SetNextItemWidth(300.0f * Dpi());
        ImGui::Combo("##loader", &ui.newLoader, loaders, 3);

        ImGui::Spacing();
        if (PrimaryButton("Créer", 130.0f * Dpi(), 36.0f * Dpi())) {
            bridge->createInstance(utf8ToWide(ui.newName), ids[(size_t)ui.newVersion], ui.newLoader);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (GhostButton("Annuler", 130.0f * Dpi(), 36.0f * Dpi())) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ui.openOffline) {
        ImGui::OpenPopup("Compte hors ligne");
        ui.openOffline = false;
    }
    if (ImGui::BeginPopupModal("Compte hors ligne", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Choisissez un pseudo (3 à 16 caractères, lettres, chiffres et _).");
        ImGui::SetNextItemWidth(260.0f * Dpi());
        ImGui::InputText("##pseudo", ui.offlineName, sizeof(ui.offlineName));
        ImGui::Spacing();
        if (PrimaryButton("Ajouter", 130.0f * Dpi(), 36.0f * Dpi())) {
            bridge->addOfflineAccount(utf8ToWide(ui.offlineName));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (GhostButton("Annuler", 130.0f * Dpi(), 36.0f * Dpi())) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ui.openMs) {
        ImGui::OpenPopup("Connexion Microsoft");
        ui.openMs = false;
    }
    if (ImGui::BeginPopupModal("Connexion Microsoft", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Connexion via code d'appareil Microsoft.");
        ImGui::Spacing();
        if (!st.msStatus.empty())
            ImGui::TextWrapped("%s", U(st.msStatus).c_str());
        else
            ImGui::TextDisabled("Démarrage de la connexion...");
        if (st.msRunning) {
            ImGui::Spacing();
            SpinnerText("En attente de la validation dans le navigateur...");
        }
        ImGui::Spacing();
        if (GhostButton("Fermer", 130.0f * Dpi(), 36.0f * Dpi())) {
            bridge->cancelMicrosoftLogin();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ui.openServer) {
        ImGui::OpenPopup("Créer un serveur local");
        ui.openServer = false;
    }
    if (ImGui::BeginPopupModal("Créer un serveur local", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextDisabled("Nom du serveur");
        ImGui::SetNextItemWidth(300.0f * Dpi());
        ImGui::InputText("##sname", ui.serverName, sizeof(ui.serverName));

        ImGui::TextDisabled("Version de Minecraft");
        ImGui::SetNextItemWidth(300.0f * Dpi());
        ImGui::InputText("##sversion", ui.serverVersion, sizeof(ui.serverVersion));

        ImGui::TextDisabled("Port");
        ImGui::SetNextItemWidth(120.0f * Dpi());
        ImGui::InputInt("##sport", &ui.serverPort);
        ui.serverPort = std::clamp(ui.serverPort, 1, 65535);

        ImGui::TextDisabled("Mémoire allouée (Mo)");
        ImGui::SetNextItemWidth(300.0f * Dpi());
        ImGui::SliderInt("##sram", &ui.serverRam, 512, 16384);
        ui.serverRam = std::clamp(ui.serverRam, 512, 131072);

        ImGui::Spacing();
        Hint("Le serveur sera créé dans le dossier de données, avec un script de démarrage. "
             "Le mode hors ligne est utilisé par défaut.");
        ImGui::Spacing();
        if (PrimaryButton("Créer le serveur", 180.0f * Dpi(), 36.0f * Dpi())) {
            bridge->createLocalServer(utf8ToWide(ui.serverName),
                                      utf8ToWide(ui.serverVersion), ui.serverPort, ui.serverRam);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (GhostButton("Annuler", 130.0f * Dpi(), 36.0f * Dpi())) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    DrawConfirmModal(bridge);
}

// ----------------------------------------------------------------- toasts ---

void DrawToasts(Bridge* bridge) {
    auto& st = bridge->state();
    if (st.toasts.empty() && st.jobStatuses.empty()) return;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 18.0f * Dpi(),
                                   vp->WorkPos.y + vp->WorkSize.y - 18.0f * Dpi()),
                            ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.95f);
    ImGui::Begin("##overlay", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);

    for (auto& job : st.jobStatuses) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.82f, 0.40f, 1.00f));
        ImGui::TextUnformatted(U(job).c_str());
        ImGui::PopStyleColor();
    }
    for (size_t i = 0; i < st.toasts.size(); ++i) {
        const ImVec4 col = st.toastError ? ImVec4(1.00f, 0.45f, 0.45f, 1.0f)
                                         : ImVec4(0.35f, 0.95f, 0.70f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(U(st.toasts[i]).c_str());
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

} // namespace

// ------------------------------------------------------------------- public ---

namespace aurora::ui {

void InitImGui(void* hwnd, ID3D11Device* device, ID3D11DeviceContext* context) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // The launcher owns its own configuration file.

    const float dpi = DetectDpi(hwnd);
    SetDpi(dpi);

    const ImWchar* ranges = io.Fonts->GetGlyphRangesDefault();
    ImFont* body = LoadUiFont(15.5f * dpi, ranges);
    if (!body) body = io.Fonts->AddFontDefault();
    ImFont* title = LoadUiFont(21.0f * dpi, ranges);
    SetFonts(body, title);
    io.FontDefault = body;

    Apply();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(device, context);
}

void ShutdownImGui() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_bridge = nullptr;
    State().navCentersReady = false;
}

void NewFrameImGui() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ResizeImGui() {}

void SetStartPage(const wchar_t* page) {
    if (!page) return;
    if (wcscmp(page, L"versions") == 0) State().page = L"versions";
    else if (wcscmp(page, L"mods") == 0) State().page = L"mods";
    else if (wcscmp(page, L"servers") == 0) State().page = L"servers";
    else if (wcscmp(page, L"console") == 0) State().page = L"console";
    else if (wcscmp(page, L"settings") == 0) State().page = L"settings";
    else State().page = L"play";
}

const wchar_t* CurrentPage() { return State().page.c_str(); }

bool GetNavButtonCenter(int index, float* x, float* y) {
    UiState& ui = State();
    if (!ui.navCentersReady || index < 0 || index >= 6) return false;
    if (x) *x = ui.navCenters[index].x;
    if (y) *y = ui.navCenters[index].y;
    return true;
}

void RenderImGui(App* app) {
    if (!g_bridge && app) g_bridge = new Bridge(app);
    if (g_bridge) g_bridge->tick();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    ImGui::Begin("##aurora_root", nullptr, flags);
    ImGui::PopStyleVar();

    // Global keyboard shortcuts: Ctrl+1..6 navigate, F5 refreshes the current page.
    {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput && g_bridge) {
            static const ImGuiKey digits[6] = { ImGuiKey_1, ImGuiKey_2, ImGuiKey_3,
                                                ImGuiKey_4, ImGuiKey_5, ImGuiKey_6 };
            static const wchar_t* pages[6] = { L"play", L"versions", L"mods",
                                               L"servers", L"console", L"settings" };
            if (io.KeyCtrl) {
                for (int i = 0; i < 6; ++i) {
                    if (ImGui::IsKeyPressed(digits[i], false)) Navigate(g_bridge, pages[i]);
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) RefreshCurrentPage(g_bridge);
        }
    }

    DrawSidebar(g_bridge);
    ImGui::SameLine();

    ImGui::BeginChild("##content", ImVec2(0, -1), false);
    if (!g_bridge) {
        ImGui::TextUnformatted("Initialisation...");
    } else {
        const std::wstring& page = State().page;
        if (page == L"versions") DrawVersions(g_bridge);
        else if (page == L"mods") DrawMods(g_bridge);
        else if (page == L"servers") DrawServers(g_bridge);
        else if (page == L"console") DrawConsole(g_bridge);
        else if (page == L"settings") DrawSettings(g_bridge);
        else DrawPlay(g_bridge);
    }
    ImGui::EndChild();

    if (g_bridge) {
        DrawModals(g_bridge);
        DrawToasts(g_bridge);
    }
    ImGui::End();
}

} // namespace aurora::ui
