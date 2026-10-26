#pragma once
#include <string>
#include <vector>

#include "config.h"
#include "meta.h"
#include "tasks.h"

namespace aurora {

struct JavaInfo {
    std::wstring path;
    int major = 0;
    std::wstring version;
};

std::vector<JavaInfo> detectJavaInstallations(const Paths& paths, const Config& config);
bool inspectJava(const std::wstring& exePath, int& major, std::wstring& versionLine);
std::wstring chooseJava(const Paths& paths, const Config& config, const ResolvedVersion& version,
                        JobContext& job, std::wstring& error, bool allowDownload = true);
bool downloadJavaRuntime(const Paths& paths, const ResolvedVersion& version, JobContext& job,
                         std::wstring& javaPath, std::wstring& error);
bool javaSatisfies(int major, int required);

} // namespace aurora
