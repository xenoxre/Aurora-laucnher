#include "core/uninstall.h"
#include "app.h"

#include <windows.h>
#include <shellapi.h>

namespace aurora {
int runImGuiMain(HINSTANCE instance, App* app);
}

extern "C" int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring dataDir;
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--data-dir") == 0 && i + 1 < argc) dataDir = argv[i + 1];
    }
    std::wstring log;
    aurora::uninstallAurora(dataDir, true, log);
    MessageBoxW(nullptr, L"Aurora Launcher a été désinstallé.\n\nL'exécutable sera supprimé après fermeture.", L"Aurora Uninstaller", MB_OK | MB_ICONINFORMATION);
    if (argv) LocalFree(argv);
    return 0;
}
