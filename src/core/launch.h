#pragma once
#include <string>
#include <vector>

#include "config.h"
#include "instance.h"
#include "meta.h"
#include "process.h"

namespace aurora {

struct LaunchRequest {
    const Paths* paths = nullptr;
    const Config* config = nullptr;
    const ResolvedVersion* version = nullptr;
    const Instance* instance = nullptr;
    std::wstring javaPath;
    std::wstring accountName;
    std::wstring accountUuid;
    std::wstring accessToken;
    std::wstring userType = L"legacy";
    std::wstring xuid;
    std::wstring clientId;
    std::wstring serverHost;
    int serverPort = 0;
};

bool buildLaunchCommand(const LaunchRequest& request, std::vector<std::wstring>& args,
                        std::wstring& error);

} // namespace aurora
