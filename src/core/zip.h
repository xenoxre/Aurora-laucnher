#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "util.h"

namespace aurora {

bool zipExtract(const fs::path& zipPath, const fs::path& dest,
                const std::function<bool(const std::string& name)>& filter,
                std::wstring& error);

bool zipExtractAll(const fs::path& zipPath, const fs::path& dest, std::wstring& error);

bool zipReadEntry(const fs::path& zipPath, const std::string& entryName, Bytes& out,
                  std::wstring& error);

bool zipList(const fs::path& zipPath, std::vector<std::string>& names, std::wstring& error);

bool zipCreate(const fs::path& zipPath,
               const std::vector<std::pair<std::string, Bytes>>& entries,
               std::wstring& error);

} // namespace aurora
