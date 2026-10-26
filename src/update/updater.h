#pragma once
// Aurora Launcher - module de mise a jour.
//
// Le manifeste (latest.json) est heberge sur GitHub Releases. Il est verifie
// (HTTPS, taille, SHA-256) avant tout telechargement puis avant installation.
// Aucune donnee personnelle n'est envoyee : une seule requete GET.

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

#include "core/http.h"

namespace aurora::update {

namespace fs = std::filesystem;

struct Asset {
    std::wstring url;
    std::wstring sha256;
    uint64_t size = 0;

    bool valid() const;
};

struct Info {
    int schema = 1;
    std::wstring version;
    std::wstring channel = L"stable";
    std::wstring notes;
    std::wstring publishedAt;
    std::wstring minVersion;
    Asset launcher;
    Asset updater;

    bool valid() const { return schema == 1 && !version.empty() && launcher.valid(); }
};

// Versionne "1.10.0 > 1.9.0", "2.0.0-beta < 2.0.0". Retourne <0, 0, >0.
int CompareVersions(const std::wstring& a, const std::wstring& b);
bool IsValidVersion(const std::wstring& version);

// Analyse un manifeste sans acces reseau (testable hors ligne).
bool ParseManifest(const std::string& text, Info& out, std::wstring& error);

// Lit le manifeste depuis une URL HTTPS, un chemin local ou file://.
bool FetchManifest(const std::wstring& source, Info& out, std::wstring& error);

// Source configuree (variable AURORA_UPDATE_MANIFEST sinon constante de build).
std::wstring ManifestUrl();
bool IsConfigured();

// Telecharge et verifie un asset. Accepte HTTPS, localhost en HTTP et les
// chemins locaux (tests). Le fichier final n'apparait qu'apres verification.
bool DownloadAsset(const Asset& asset, const fs::path& destination,
                   const HttpProgress& progress, const HttpCancel& cancel,
                   std::wstring& error);

// Applique le remplacement du lanceur via AuroraUpdater.exe puis quitte.
bool LaunchUpdaterAndExit(const fs::path& targetExe, const fs::path& stagedExe,
                          const fs::path& installedUpdater, const std::wstring& oldVersion,
                          std::wstring& error);

} // namespace aurora::update
