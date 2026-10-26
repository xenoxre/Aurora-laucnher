#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "config.h"
#include "json.h"
#include "tasks.h"

namespace aurora {

struct VersionEntry {
    std::wstring id;
    std::wstring type;
    std::wstring url;
    std::wstring releaseTime;
};

struct Manifest {
    std::wstring latestRelease;
    std::wstring latestSnapshot;
    std::vector<VersionEntry> versions;
};

struct LibraryFile {
    std::wstring name;
    std::wstring path;
    std::wstring url;
    std::wstring sha1;
    uint64_t size = 0;
    bool isNative = false;
    std::vector<std::wstring> extractExclude;
};

struct ResolvedVersion {
    std::wstring id;
    std::wstring parentId;     // root vanilla version
    std::wstring jarId;        // version owning the client jar
    std::wstring mainClass;
    std::wstring type = L"release";
    std::wstring assetsId = L"legacy";
    std::wstring assetsUrl;
    std::wstring assetsSha1;
    uint64_t assetsSize = 0;
    int javaMajor = 8;
    std::wstring javaComponent = L"jre-legacy";
    std::wstring clientUrl;
    std::wstring clientSha1;
    uint64_t clientSize = 0;
    std::wstring serverUrl;
    std::wstring serverSha1;
    uint64_t serverSize = 0;
    std::wstring logFileId;
    std::wstring logFileUrl;
    std::wstring logFileSha1;
    std::wstring logArg;
    std::vector<LibraryFile> libraries;
    std::vector<std::wstring> gameArgs;
    std::vector<std::wstring> jvmArgs;
    bool legacyArgs = false;
    bool supportsQuickPlay = false;
    Json raw;
};

bool fetchManifest(const Paths& paths, Manifest& manifest, bool force, std::wstring& error);
bool loadVersionJson(const Paths& paths, const std::wstring& id, Json& out,
                     bool allowDownload, std::wstring& error);
bool resolveVersion(const Paths& paths, const std::wstring& id, ResolvedVersion& out,
                    bool allowDownload, std::wstring& error, bool customResolution = true);
bool isVersionInstalled(const Paths& paths, const std::wstring& id);
bool versionSupportsQuickPlay(const ResolvedVersion& version);
std::wstring mavenPathForName(const std::wstring& coordinate);
bool parseMavenCoordinate(const std::wstring& coordinate, std::wstring& group,
                          std::wstring& artifact, std::wstring& version,
                          std::wstring& classifier, std::wstring& ext);
std::wstring assetsDirectory(const Paths& paths, const ResolvedVersion& version);
std::vector<std::wstring> installedVersionIds(const Paths& paths);

// Installers (declare here to keep includes simple)
struct InstallResult {
    std::wstring versionId;
    std::wstring mcVersion;
    std::wstring loader;
    std::wstring loaderVersion;
    bool ok = false;
};

bool installVersion(const Paths& paths, const std::wstring& id, JobContext& job,
                    std::wstring& error);
bool installLoader(const Paths& paths, const std::wstring& mcVersion,
                   const std::wstring& loader, const std::wstring& loaderVersion,
                   JobContext& job, InstallResult& result, std::wstring& error);
bool uninstallVersion(const Paths& paths, const std::wstring& id, std::wstring& error);

} // namespace aurora
