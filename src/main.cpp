// Aurora Launcher - entry point (CLI + GUI launcher)
#include "app.h"
#include "core/auth.h"
#include "core/config.h"
#include "core/http.h"
#include "core/java.h"
#include "core/launch.h"
#include "core/log.h"
#include "core/meta.h"
#include "core/process.h"
#include "core/server.h"
#include "core/util.h"
#include "version.h"

namespace aurora {
int runSelfTests(bool verbose);
int runImGuiMain(HINSTANCE instance, App* app);
}

#include <shellapi.h>
#include <string>
#include <vector>

using namespace aurora;

static void printUsage() {
    wprintf(L"Aurora Launcher %ls\n\n", AURORA_VERSION_W);
    wprintf(L"Usage:\n");
    wprintf(L"  AuroraLauncher.exe [options]\n\n");
    wprintf(L"Options:\n");
    wprintf(L"  --help                Afficher cette aide\n");
    wprintf(L"  --version             Afficher la version\n");
    wprintf(L"  --verbose             Journal plus verbeux\n");
    wprintf(L"  --data-dir <chemin>   Repertoire de donnees\n");
    wprintf(L"  --selftest            Executer les tests internes\n");
    wprintf(L"  --list-versions       Lister les versions disponibles\n");
    wprintf(L"  --install <version>   Installer une version\n");
    wprintf(L"  --launch <instance>   Lancer une instance\n");
    wprintf(L"  --ping <hote[:port]>  Pinger un serveur\n");
    wprintf(L"  --create-instance <nom>:<version> Creer une instance\n");
}

static std::wstring findArg(int argc, wchar_t** argv, const std::wstring& name) {
    for (int i = 1; i < argc - 1; i++) {
        if (iequals(argv[i], name)) return argv[i + 1];
    }
    return L"";
}

static bool hasArg(int argc, wchar_t** argv, const std::wstring& name) {
    for (int i = 1; i < argc; i++) {
        if (iequals(argv[i], name)) return true;
    }
    return false;
}

#ifdef AURORA_CLI
int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    if (hasArg(argc, argv, L"--help")) { printUsage(); return 0; }
    if (hasArg(argc, argv, L"--version")) { wprintf(L"%ls\n", AURORA_VERSION_W); return 0; }
    if (hasArg(argc, argv, L"--selftest")) { return runSelfTests(true); }

    Paths paths;
    {
        std::wstring p = findArg(argc, argv, L"--data-dir");
        if (p.empty()) {
            if (pathExists(executableDir() / L"portable.txt")) p = executableDir().wstring();
            else if (const wchar_t* env = _wgetenv(L"AURORA_DATA_DIR")) p = env;
        }
        paths.init(p.empty() ? defaultDataDir() : fs::path(p));
    }
    Config config;
    std::wstring cerr;
    loadConfig(paths, config, cerr);
    Logger::instance().init(paths.logs / L"aurora.log", hasArg(argc, argv, L"--verbose"));

    if (hasArg(argc, argv, L"--list-versions")) {
        Manifest m;
        std::wstring err;
        if (!fetchManifest(paths, m, false, err)) { wprintf(L"Erreur: %ls\n", err.c_str()); return 1; }
        for (auto& v : m.versions) wprintf(L"%ls\t%ls\t%ls\n", v.id.c_str(), v.type.c_str(), v.releaseTime.c_str());
        return 0;
    }
    if (hasArg(argc, argv, L"--ping")) {
        std::wstring addr = findArg(argc, argv, L"--ping");
        ServerStatus st = pingServer(addr);
        if (!st.ok) { wprintf(L"Erreur: %ls\n", st.error.c_str()); return 1; }
        wprintf(L"OK %dms joueurs %d/%d version %ls\n", st.pingMs, st.online, st.maxPlayers, st.versionName.c_str());
        return 0;
    }
    if (hasArg(argc, argv, L"--create-instance")) {
        std::wstring spec = findArg(argc, argv, L"--create-instance");
        size_t colon = spec.find(L':');
        if (colon == std::wstring::npos) { wprintf(L"Format attendu nom:version\n"); return 1; }
        Instance inst;
        inst.name = spec.substr(0, colon);
        inst.mcVersion = spec.substr(colon + 1);
        inst.id = slugify(inst.name);
        inst.loader = L"none";
        inst.versionId = inst.mcVersion;
        inst.dir = paths.instanceDir(inst.id);
        std::wstring err;
        if (!createInstance(paths, inst, err)) { wprintf(L"Erreur: %ls\n", err.c_str()); return 1; }
        wprintf(L"Instance creee: %ls\n", inst.id.c_str());
        return 0;
    }
    if (hasArg(argc, argv, L"--install")) {
        std::wstring ver = findArg(argc, argv, L"--install");
        JobQueue jq;
        std::wstring err;
        bool ok = false;
        jq.submit(L"Installation " + ver, [&](JobContext& ctx) { ok = installVersion(paths, ver, ctx, err); }, {});
        for (int i = 0; i < 600; i++) {
            jq.poll();
            auto infos = jq.infos();
            if (!infos.empty()) wprintf(L"\r%ls", infos[0].status.c_str());
            if (infos.empty()) break;
            Sleep(200);
        }
        wprintf(L"\n");
        if (!ok) { wprintf(L"Erreur: %ls\n", err.c_str()); return 1; }
        wprintf(L"Installe\n");
        return 0;
    }
    wprintf(L"Aucun mode CLI specifique. Lancez sans arguments pour l'interface graphique.\n");
    return 0;
}
#else
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (hasArg(argc, argv, L"--help")) { printUsage(); if (argv) LocalFree(argv); return 0; }
    if (hasArg(argc, argv, L"--version")) { MessageBoxW(nullptr, AURORA_VERSION_W, L"Aurora Launcher", MB_OK); if (argv) LocalFree(argv); return 0; }

    App app;
    AppOptions opt;
    {
        std::wstring p = findArg(argc, argv, L"--data-dir");
        if (p.empty()) {
            if (pathExists(executableDir() / L"portable.txt")) p = executableDir().wstring();
            else if (const wchar_t* env = _wgetenv(L"AURORA_DATA_DIR")) p = env;
        }
        opt.dataDir = p.empty() ? defaultDataDir() : fs::path(p);
    }
    opt.verbose = hasArg(argc, argv, L"--verbose");
    opt.screenshotPath = findArg(argc, argv, L"--screenshot");
    opt.startPage = findArg(argc, argv, L"--page");
    opt.uiTest = hasArg(argc, argv, L"--uitest");
    {
        std::wstring delay = findArg(argc, argv, L"--screenshot-delay");
        if (!delay.empty()) opt.screenshotDelayMs = _wtoi(delay.c_str());
    }

    std::wstring err;
    if (!app.init(hInstance, opt, err)) {
        MessageBoxW(nullptr, err.c_str(), L"Aurora Launcher - Erreur", MB_OK | MB_ICONERROR);
        if (argv) LocalFree(argv);
        return 1;
    }

    int code = runImGuiMain(hInstance, &app);
    if (argv) LocalFree(argv);
    return code;
}
#endif
