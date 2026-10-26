#pragma once
// Shared, cross-page UI state.
//
// The sidebar, the modal host and the pages live in different translation
// units, so the handful of values they exchange (current page, dialog flags,
// pending confirmation) are centralised here instead of being duplicated.

#include <string>

#include "imgui.h"

namespace aurora::ui {

class Bridge;

enum ConfirmAction {
    ConfirmNone = 0,
    ConfirmUninstallVersion,
    ConfirmDeleteInstance,
    ConfirmRemoveAccount,
    ConfirmStopServer,
    ConfirmRemoveLocalServer,
};

struct ConfirmState {
    bool request = false;
    int action = ConfirmNone;
    std::wstring payload;
    std::wstring title;
    std::wstring message;
};

struct UiState {
    // Navigation -------------------------------------------------------------
    std::wstring page = L"play";
    ImVec2 navCenters[6] = {};
    bool navCentersReady = false;

    // Dialogs ----------------------------------------------------------------
    bool openNewInstance = false;
    bool openOffline = false;
    bool openMs = false;
    bool openServer = false;

    // Dialog fields ----------------------------------------------------------
    char newName[64] = "Nouvelle instance";
    int newVersion = 0;
    int newLoader = 0;
    char offlineName[32] = "Steve";
    char serverName[64] = "Mon serveur";
    char serverVersion[32] = "1.21.1";
    int serverPort = 25565;
    int serverRam = 2048;

    // Editors with non-trivial lifecycles -----------------------------------
    char modQuery[128] = "";
    bool jvmArgsInit = false;
    char jvmArgs[768] = "";
    double settingsSavedAt = -100.0;

    ConfirmState confirm;
};

UiState& State();

// Navigate both the local UI page and the application model.
void Navigate(Bridge* bridge, const wchar_t* page);

// Ask the modal host to display a destructive-action confirmation.
void RequestConfirm(int action, const std::wstring& payload, const std::wstring& title,
                    const std::wstring& message);

} // namespace aurora::ui
