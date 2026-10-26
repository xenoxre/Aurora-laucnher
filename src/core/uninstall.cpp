#include "uninstall.h"
#include "util.h"
#include "log.h"

#include <windows.h>
#include <shlobj.h>

namespace fs = std::filesystem;

namespace aurora {

bool uninstallAurora(const std::wstring& dataDir, bool removeExe, std::wstring& log) {
    log.clear();
    auto append = [&](const std::wstring& s) { log += s + L"\r\n"; };

    try {
        append(L"[Aurora] Démarrage de la désinstallation...");
        if (!dataDir.empty() && fs::exists(dataDir)) {
            append(L"[Aurora] Suppression des données : " + dataDir);
            std::error_code ec;
            fs::remove_all(dataDir, ec);
            if (ec) append(L"[Warn] Impossible de supprimer tout le dossier données : " + utf8ToWide(ec.message()));
            else append(L"[OK] Données supprimées");
        }

        if (removeExe) {
            wchar_t exePath[MAX_PATH];
            if (GetModuleFileNameW(nullptr, exePath, MAX_PATH)) {
                append(L"[Aurora] Planification de la suppression de l'exécutable...");
                std::wstring cmd = L"cmd /c ping 127.0.0.1 -n 2 > nul & del /f /q \"" + std::wstring(exePath) + L"\"";
                STARTUPINFOW si = { sizeof(si) };
                PROCESS_INFORMATION pi = {};
                if (CreateProcessW(nullptr, const_cast<wchar_t*>(cmd.c_str()), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    append(L"[OK] Suppression planifiée");
                }
            }
        }

        append(L"[Aurora] Désinstallation terminée.");
        return true;
    } catch (...) {
        append(L"[ERR] Erreur lors de la désinstallation");
        return false;
    }
}

} // namespace aurora
