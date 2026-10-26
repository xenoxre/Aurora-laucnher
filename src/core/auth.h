#pragma once
#include <atomic>
#include <string>

#include "config.h"

namespace aurora {

struct DeviceCodeInfo {
    std::wstring userCode;
    std::wstring deviceCode;
    std::wstring verificationUri;
    std::wstring message;
    int interval = 5;
    int expiresIn = 900;
};

enum class MsPollResult { Success, Pending, SlowDown, Expired, Error };

bool msStartDeviceFlow(const std::wstring& clientId, DeviceCodeInfo& info, std::wstring& error);
MsPollResult msPollDeviceFlow(const std::wstring& clientId, const DeviceCodeInfo& info,
                              Account& account, int& interval, std::wstring& error,
                              const std::atomic<bool>* cancel);
bool msRefreshAccount(const std::wstring& clientId, Account& account, std::wstring& error);
bool accountTokenValid(const Account& account);
std::wstring formatUuid(const std::wstring& hex32);

} // namespace aurora
