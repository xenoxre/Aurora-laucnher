#pragma once

#include <string>
#include <filesystem>

namespace aurora {

bool uninstallAurora(const std::wstring& dataDir, bool removeExe, std::wstring& log);

} // namespace aurora
