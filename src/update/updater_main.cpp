// Aurora Launcher - assistant de mise a jour.
//
// Ce binaire est volontairement autonome : il ne depend ni du lanceur ni de
// la bibliotheque standard C++ du projet. Il attend la fermeture du lanceur,
// remplace son executable, supprime l'ancienne version puis le relance.
//
// Usage (interne, lance par AuroraLauncher.exe) :
//   AuroraUpdater.exe --pid <pid> --target <exe> --source <nouvel exe>
//                     [--updated-from <version>] [--no-restart] [--quiet]
//
// Mode test :
//   AuroraUpdater.exe --pid 0 --target <copie> --source <nouvel exe> --no-restart
#include <windows.h>
#include <shellapi.h>

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

namespace {

FILE* g_log = nullptr;

void closeLog() {
    if (g_log) {
        std::fclose(g_log);
        g_log = nullptr;
    }
}

void logLine(const std::wstring& text) {
    if (!g_log) return;
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::fwprintf(g_log, L"[%04u-%02u-%02u %02u:%02u:%02u] %ls\n", st.wYear, st.wMonth,
                  st.wDay, st.wHour, st.wMinute, st.wSecond, text.c_str());
    std::fflush(g_log);
}

std::wstring joinPath(const std::wstring& dir, const wchar_t* name) {
    if (dir.empty()) return name;
    wchar_t last = dir.back();
    if (last == L'\\' || last == L'/') return dir + name;
    return dir + L"\\" + name;
}

std::wstring parentDir(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

std::wstring fileName(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

std::wstring quoteArg(const std::wstring& arg) {
    if (arg.find_first_of(L" \"\t") == std::wstring::npos) return arg;
    std::wstring out = L"\"";
    for (wchar_t c : arg) {
        if (c == L'"') out += L'\\';
        out += c;
    }
    out += L'"';
    return out;
}

bool fileExists(const std::wstring& path) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool isPortableExecutable(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    unsigned char header[64] = {};
    DWORD read = 0;
    bool ok = ReadFile(file, header, (DWORD)sizeof(header), &read, nullptr) &&
              read == (DWORD)sizeof(header) && header[0] == 'M' && header[1] == 'Z';
    if (ok) {
        int32_t peOffset = 0;
        std::memcpy(&peOffset, header + 0x3C, sizeof(peOffset));
        ok = peOffset > 0 && peOffset < 0x1000000;
        if (ok) {
            unsigned char signature[4] = {};
            ok = SetFilePointer(file, peOffset, nullptr, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
                 ReadFile(file, signature, 4, &read, nullptr) && read == 4 &&
                 signature[0] == 'P' && signature[1] == 'E' && signature[2] == 0 &&
                 signature[3] == 0;
        }
    }
    CloseHandle(file);
    return ok;
}

void sleepMs(DWORD ms) { Sleep(ms); }

bool waitForProcess(DWORD pid, DWORD timeoutMs) {
    if (pid == 0) return true;
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!process) {
        // Le processus est peut-etre deja termine ou inaccessible : on laisse
        // les tentatives de remplacement decider.
        return true;
    }
    const DWORD result = WaitForSingleObject(process, timeoutMs);
    CloseHandle(process);
    if (result == WAIT_OBJECT_0) return true;
    if (result == WAIT_TIMEOUT) return false;
    return false;
}

bool moveWithRetry(const std::wstring& from, const std::wstring& to, bool replace,
                   int attempts, DWORD delayMs) {
    for (int i = 0; i < attempts; ++i) {
        const DWORD flags = replace ? MOVEFILE_REPLACE_EXISTING : 0;
        if (MoveFileExW(from.c_str(), to.c_str(), flags | MOVEFILE_COPY_ALLOWED)) return true;
        const DWORD err = GetLastError();
        logLine(L"Deplacement echoue (" + from + L" -> " + to + L"), erreur " +
                std::to_wstring(err) + L", tentative " + std::to_wstring(i + 1));
        sleepMs(delayMs);
    }
    return false;
}

bool deleteWithRetry(const std::wstring& path, int attempts, DWORD delayMs) {
    for (int i = 0; i < attempts; ++i) {
        if (DeleteFileW(path.c_str())) return true;
        const DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) return true;
        sleepMs(delayMs);
    }
    return false;
}

bool relaunch(const std::wstring& target, const std::wstring& updatedFrom) {
    if (!fileExists(target)) {
        logLine(L"Relance impossible : executable introuvable");
        return false;
    }
    std::wstring cmd = quoteArg(target);
    if (!updatedFrom.empty()) cmd += L" --updated-from " + quoteArg(updatedFrom);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');
    const std::wstring workDir = parentDir(target);
    if (!CreateProcessW(target.c_str(), mutableCmd.data(), nullptr, nullptr, FALSE,
                        CREATE_UNICODE_ENVIRONMENT, nullptr,
                        workDir.empty() ? nullptr : workDir.c_str(), &si, &pi)) {
        logLine(L"CreateProcess a echoue, erreur " + std::to_wstring(GetLastError()));
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    logLine(L"Nouveau lanceur demarre");
    return true;
}

struct Options {
    DWORD pid = 0;
    std::wstring target;
    std::wstring source;
    std::wstring restart;
    std::wstring updatedFrom;
    std::wstring logPath;
    bool noRestart = false;
    bool quiet = false;
};

bool parseOptions(int argc, wchar_t** argv, Options& out, std::wstring& error) {
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        auto value = [&](std::wstring& dst) {
            if (i + 1 >= argc) {
                error = L"Valeur manquante pour " + arg;
                return false;
            }
            dst = argv[++i];
            return true;
        };
        if (arg == L"--pid") {
            std::wstring v;
            if (!value(v)) return false;
            out.pid = (DWORD)_wtoi(v.c_str());
        } else if (arg == L"--target") {
            if (!value(out.target)) return false;
        } else if (arg == L"--source") {
            if (!value(out.source)) return false;
        } else if (arg == L"--restart") {
            if (i + 1 < argc && std::wstring(argv[i + 1]).rfind(L"--", 0) != 0) {
                out.restart = argv[++i];
            }
        } else if (arg == L"--updated-from") {
            if (!value(out.updatedFrom)) return false;
        } else if (arg == L"--log") {
            if (!value(out.logPath)) return false;
        } else if (arg == L"--no-restart") {
            out.noRestart = true;
        } else if (arg == L"--quiet") {
            out.quiet = true;
        } else {
            error = L"Argument inconnu : " + arg;
            return false;
        }
    }
    if (out.target.empty()) {
        error = L"--target est obligatoire";
        return false;
    }
    if (out.source.empty()) {
        error = L"--source est obligatoire";
        return false;
    }
    if (out.restart.empty()) out.restart = out.target;
    return true;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 2;

    Options opt;
    std::wstring error;
    const bool parsed = parseOptions(argc, argv, opt, error);
    LocalFree(argv);

    if (!parsed) {
        if (!opt.quiet)
            MessageBoxW(nullptr, error.c_str(), L"Aurora - mise a jour", MB_OK | MB_ICONERROR);
        return 2;
    }

    const std::wstring logPath =
        opt.logPath.empty() ? joinPath(parentDir(opt.target), L"update.log") : opt.logPath;
    _wfopen_s(&g_log, logPath.c_str(), L"a, ccs=UTF-8");

    logLine(L"--- AuroraUpdater demarre ---");
    logLine(L"PID a attendre : " + std::to_wstring(opt.pid));
    logLine(L"Cible : " + opt.target);
    logLine(L"Source : " + opt.source);
    logLine(L"Version precedente : " + (opt.updatedFrom.empty() ? L"inconnue" : opt.updatedFrom));

    auto fail = [&](const std::wstring& message, bool restartOld) {
        logLine(L"ECHEC : " + message);
        if (restartOld && !opt.noRestart) {
            logLine(L"Tentative de relance de l'ancienne version");
            relaunch(opt.target, L"");
        }
        closeLog();
        if (!opt.quiet) {
            const std::wstring text = message + L"\n\nJournal : " + logPath;
            MessageBoxW(nullptr, text.c_str(), L"Aurora - mise a jour", MB_OK | MB_ICONERROR);
        }
        return 1;
    };

    if (!fileExists(opt.source)) return fail(L"Le nouveau lanceur telecharge est introuvable.", true);
    if (!isPortableExecutable(opt.source)) {
        return fail(L"Le nouveau lanceur n'est pas un executable Windows valide.", false);
    }
    if (!fileExists(opt.target)) return fail(L"Le lanceur a remplacer est introuvable.", false);

    logLine(L"Attente de la fermeture du lanceur...");
    if (!waitForProcess(opt.pid, 180000)) {
        return fail(L"Le lanceur ne s'est pas ferme a temps.", false);
    }
    logLine(L"Lanceur ferme");

    const std::wstring backup = opt.target + L".old";
    if (fileExists(backup) && !deleteWithRetry(backup, 20, 150)) {
        logLine(L"Ancienne sauvegarde impossible a supprimer, poursuite");
    }

    // Windows autorise le renommage d'un executable en cours d'execution, mais
    // pas son ecrasement. On deplace donc l'ancien avant de copier le nouveau.
    bool movedOld = false;
    for (int attempt = 0; attempt < 40; ++attempt) {
        if (MoveFileExW(opt.target.c_str(), backup.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
            movedOld = true;
            break;
        }
        const DWORD err = GetLastError();
        logLine(L"Renommage de l'ancien lanceur echoue, erreur " + std::to_wstring(err));
        sleepMs(250);
    }
    if (!movedOld) {
        return fail(L"Impossible de mettre l'ancien lanceur de cote (fichier verrouille ?).",
                    true);
    }
    logLine(L"Ancien lanceur deplace vers " + fileName(backup));

    if (!moveWithRetry(opt.source, opt.target, false, 40, 250)) {
        logLine(L"Installation du nouveau lanceur echouee, restauration");
        if (MoveFileExW(backup.c_str(), opt.target.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
            logLine(L"Ancienne version restauree");
            return fail(L"Le nouveau lanceur n'a pas pu etre installe. Ancienne version restauree.",
                        true);
        }
        return fail(L"Le nouveau lanceur n'a pas pu etre installe et la restauration a echoue.",
                    false);
    }
    logLine(L"Nouveau lanceur installe");

    if (!opt.noRestart) {
        if (!relaunch(opt.restart, opt.updatedFrom)) {
            logLine(L"Ancienne version conservee dans " + fileName(backup));
            return fail(L"Le nouveau lanceur est installe mais n'a pas pu etre relance. "
                        L"L'ancienne version est conservee dans " + fileName(backup) + L".",
                        false);
        }
    }

    if (!deleteWithRetry(backup, 20, 150)) {
        logLine(L"Ancienne version conservee dans " + fileName(backup) +
                L" (suppression impossible)");
    } else {
        logLine(L"Ancienne version supprimee");
    }

    logLine(L"--- AuroraUpdater termine ---");
    closeLog();
    return 0;
}
