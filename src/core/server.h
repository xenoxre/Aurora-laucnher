#pragma once
#include <string>
#include <vector>

#include "config.h"
#include "tasks.h"
#include "util.h"

namespace aurora {

struct ServerEntry {
    std::wstring name;
    std::wstring ip;
    Bytes icon; // PNG bytes
    bool acceptTextures = true;
};

struct ServerStatus {
    bool ok = false;
    std::wstring error;
    std::wstring motd;
    int online = -1;
    int maxPlayers = -1;
    std::wstring versionName;
    int protocol = -1;
    int pingMs = -1;
    Bytes favicon;
};

bool parseServerAddress(const std::wstring& address, std::wstring& host, int& port);
std::wstring serverAddressString(const std::wstring& host, int port);
ServerStatus pingServer(const std::wstring& address, int timeoutMs = 5000);

bool readServersDat(const std::filesystem::path& path, std::vector<ServerEntry>& out,
                    std::wstring& error);
bool writeServersDat(const std::filesystem::path& path, const std::vector<ServerEntry>& entries,
                     std::wstring& error);

bool createLocalServer(const Paths& paths, LocalServerConfig& config, JobContext& job,
                       std::wstring& error);

} // namespace aurora
