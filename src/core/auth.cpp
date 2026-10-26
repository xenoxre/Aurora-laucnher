#include "auth.h"
#include "http.h"
#include "json.h"
#include "log.h"
#include "util.h"
#include "../version.h"

#include <algorithm>

namespace aurora {

static const wchar_t* kTokenUrl = L"https://login.microsoftonline.com/consumers/oauth2/v2.0/token";
static const wchar_t* kDeviceUrl = L"https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode";
static const wchar_t* kXblUrl = L"https://user.auth.xboxlive.com/user/authenticate";
static const wchar_t* kXstsUrl = L"https://xsts.auth.xboxlive.com/xsts/authorize";
static const wchar_t* kMcLoginUrl = L"https://api.minecraftservices.com/authentication/login_with_xbox";
static const wchar_t* kMcProfileUrl = L"https://api.minecraftservices.com/minecraft/profile";

std::wstring formatUuid(const std::wstring& hex32) {
    if (hex32.size() != 32) return hex32;
    return hex32.substr(0, 8) + L"-" + hex32.substr(8, 4) + L"-" + hex32.substr(12, 4) + L"-" +
           hex32.substr(16, 4) + L"-" + hex32.substr(20, 12);
}

static bool rawPost(const std::wstring& url, const std::string& body, const std::wstring& contentType,
                    const std::vector<std::pair<std::wstring, std::wstring>>& extraHeaders,
                    Json& out, int& status, std::wstring& error) {
    HttpRequest req;
    req.url = url;
    req.method = L"POST";
    req.body = body;
    req.contentType = contentType;
    req.headers.push_back({ L"Accept", L"application/json" });
    req.headers.push_back({ L"User-Agent", L"AuroraLauncher/" AURORA_VERSION_W });
    for (auto& h : extraHeaders) req.headers.push_back(h);
    HttpResponse res;
    if (!httpRequest(req, res)) {
        error = res.error.empty() ? L"Échec réseau" : res.error;
        return false;
    }
    status = res.status;
    std::string perr;
    if (!Json::parse(res.body, out, &perr)) {
        if (res.status >= 400) {
            error = formatW(L"HTTP %d", res.status);
        } else {
            error = L"Réponse invalide du service d'authentification";
        }
        return false;
    }
    return true;
}

static bool rawGet(const std::wstring& url, const std::wstring& bearer, Json& out, int& status,
                   std::wstring& error) {
    HttpRequest req;
    req.url = url;
    req.headers.push_back({ L"Authorization", L"Bearer " + bearer });
    req.headers.push_back({ L"Accept", L"application/json" });
    req.headers.push_back({ L"User-Agent", L"AuroraLauncher/" AURORA_VERSION_W });
    HttpResponse res;
    if (!httpRequest(req, res)) {
        error = res.error.empty() ? L"Échec réseau" : res.error;
        return false;
    }
    status = res.status;
    std::string perr;
    if (!Json::parse(res.body, out, &perr)) {
        error = formatW(L"HTTP %d", res.status);
        return false;
    }
    return true;
}

bool msStartDeviceFlow(const std::wstring& clientId, DeviceCodeInfo& info, std::wstring& error) {
    if (clientId.empty()) {
        error = L"Client ID Microsoft manquant";
        return false;
    }
    std::string body = "client_id=" + urlEncode(wideToUtf8(clientId)) +
                       "&scope=" + urlEncode("XboxLive.signin offline_access");
    Json j;
    int status = 0;
    if (!rawPost(kDeviceUrl, body, L"application/x-www-form-urlencoded", {}, j, status, error))
        return false;
    if (status < 200 || status >= 300) {
        error = j["error_description"].asWString(j["error"].asWString(L"Connexion Microsoft refusée"));
        return false;
    }
    info.userCode = j["user_code"].asWString();
    info.deviceCode = j["device_code"].asWString();
    info.verificationUri = j["verification_uri"].asWString();
    info.message = j["message"].asWString();
    info.interval = std::max(2, (int)j["interval"].asInt(5));
    info.expiresIn = (int)j["expires_in"].asInt(900);
    if (info.userCode.empty() || info.deviceCode.empty()) {
        error = L"Réponse Microsoft incomplète";
        return false;
    }
    return true;
}

static std::wstring xstsMessage(long long xerr, const std::wstring& fallback) {
    switch (xerr) {
        case 2148916233: return L"Ce compte Microsoft n'a pas de profil Xbox. Créez-en un sur xbox.com, puis réessayez.";
        case 2148916235: return L"Xbox Live n'est pas disponible dans votre pays/région.";
        case 2148916236:
        case 2148916237: return L"Vérification d'âge requise par Xbox Live. Connectez-vous sur xbox.com.";
        case 2148916238: return L"Compte enfant : une autorisation parentale est nécessaire.";
        default: break;
    }
    if (!fallback.empty()) return fallback;
    return L"Authentification Xbox refusée (code " + std::to_wstring(xerr) + L")";
}

static bool exchangeMinecraft(const std::wstring& msAccessToken, const std::wstring& refreshToken,
                              Account& account, std::wstring& error) {
    Json xblBody = Json::object();
    Json props = Json::object();
    props.set("AuthMethod", L"RPS");
    props.set("SiteName", L"user.auth.xboxlive.com");
    props.set("RpsTicket", L"d=" + msAccessToken);
    xblBody.set("Properties", props);
    xblBody.set("RelyingParty", L"http://auth.xboxlive.com");
    xblBody.set("TokenType", L"JWT");
    Json xbl;
    int status = 0;
    if (!rawPost(kXblUrl, xblBody.dump(), L"application/json",
                 { { L"x-xbl-contract-version", L"1" } }, xbl, status, error))
        return false;
    if (status != 200) {
        error = L"Xbox Live : " + xstsMessage(xbl["XErr"].asInt(0), xbl["Message"].asWString());
        return false;
    }
    std::wstring xblToken = xbl["Token"].asWString();
    std::wstring uhs = xbl["DisplayClaims"]["xui"].at(0)["uhs"].asWString();
    std::wstring xuid = xbl["DisplayClaims"]["xui"].at(0)["xid"].asWString();
    if (xblToken.empty() || uhs.empty()) { error = L"Xbox Live : réponse incomplète"; return false; }

    Json xstsBody = Json::object();
    Json xprops = Json::object();
    xprops.set("SandboxId", L"RETAIL");
    Json tokens = Json::array();
    tokens.push(xblToken);
    xprops.set("UserTokens", tokens);
    xstsBody.set("Properties", xprops);
    xstsBody.set("RelyingParty", L"rp://api.minecraftservices.com/");
    xstsBody.set("TokenType", L"JWT");
    Json xsts;
    if (!rawPost(kXstsUrl, xstsBody.dump(), L"application/json",
                 { { L"x-xbl-contract-version", L"1" } }, xsts, status, error))
        return false;
    if (status != 200) {
        error = xstsMessage(xsts["XErr"].asInt(0), xsts["Message"].asWString());
        return false;
    }
    std::wstring xstsToken = xsts["Token"].asWString();
    std::wstring xstsUhs = xsts["DisplayClaims"]["xui"].at(0)["uhs"].asWString();
    if (!xstsUhs.empty()) uhs = xstsUhs;
    if (xstsToken.empty()) { error = L"XSTS : jeton absent"; return false; }

    Json mcBody = Json::object();
    mcBody.set("identityToken", L"XBL3.0 x=" + uhs + L";" + xstsToken);
    Json mc;
    if (!rawPost(kMcLoginUrl, mcBody.dump(), L"application/json", {}, mc, status, error))
        return false;
    if (status != 200) {
        error = L"Minecraft : authentification refusée (" + std::to_wstring(status) + L")";
        return false;
    }
    std::wstring mcToken = mc["access_token"].asWString();
    long long expiresIn = mc["expires_in"].asInt(86400);
    if (mcToken.empty()) { error = L"Minecraft : jeton absent"; return false; }

    Json profile;
    if (!rawGet(kMcProfileUrl, mcToken, profile, status, error)) return false;
    if (status == 404) {
        error = L"Ce compte Microsoft ne possède pas Minecraft: Java Edition.";
        return false;
    }
    if (status != 200) {
        error = formatW(L"Profil Minecraft indisponible (HTTP %d)", status);
        return false;
    }
    std::wstring name = profile["name"].asWString();
    std::wstring id = profile["id"].asWString();
    if (name.empty() || id.empty()) { error = L"Profil Minecraft incomplet"; return false; }

    account.type = L"microsoft";
    account.name = name;
    account.uuid = formatUuid(id);
    account.accessToken = mcToken;
    account.refreshToken = refreshToken;
    account.xuid = xuid;
    account.expiresAt = nowUnix() + std::max<long long>(0, expiresIn - 120);
    return true;
}

MsPollResult msPollDeviceFlow(const std::wstring& clientId, const DeviceCodeInfo& info,
                              Account& account, int& interval, std::wstring& error,
                              const std::atomic<bool>* cancel) {
    if (cancel && cancel->load()) { error = L"Connexion annulée"; return MsPollResult::Error; }
    std::string body = "client_id=" + urlEncode(wideToUtf8(clientId)) +
                       "&grant_type=" + urlEncode("urn:ietf:params:oauth:grant-type:device_code") +
                       "&device_code=" + urlEncode(wideToUtf8(info.deviceCode));
    Json j;
    int status = 0;
    if (!rawPost(kTokenUrl, body, L"application/x-www-form-urlencoded", {}, j, status, error)) {
        return MsPollResult::Error;
    }
    if (status == 200) {
        std::wstring access = j["access_token"].asWString();
        std::wstring refresh = j["refresh_token"].asWString();
        if (access.empty()) { error = L"Jeton Microsoft absent"; return MsPollResult::Error; }
        if (!exchangeMinecraft(access, refresh, account, error)) return MsPollResult::Error;
        return MsPollResult::Success;
    }
    std::wstring err = j["error"].asWString();
    if (err == L"authorization_pending") return MsPollResult::Pending;
    if (err == L"slow_down") {
        interval += 5;
        return MsPollResult::SlowDown;
    }
    if (err == L"expired_token") { error = L"Le code a expiré, relancez la connexion."; return MsPollResult::Expired; }
    if (err == L"authorization_declined") { error = L"Connexion refusée par l'utilisateur."; return MsPollResult::Error; }
    error = j["error_description"].asWString(err.empty() ? L"Connexion Microsoft impossible" : err);
    return MsPollResult::Error;
}

bool msRefreshAccount(const std::wstring& clientId, Account& account, std::wstring& error) {
    if (account.refreshToken.empty()) {
        error = L"Jeton de rafraîchissement absent — reconnectez le compte Microsoft.";
        return false;
    }
    if (clientId.empty()) {
        error = L"Client ID Microsoft manquant dans les paramètres.";
        return false;
    }
    std::string body = "client_id=" + urlEncode(wideToUtf8(clientId)) +
                       "&grant_type=refresh_token&refresh_token=" +
                       urlEncode(wideToUtf8(account.refreshToken)) +
                       "&scope=" + urlEncode("XboxLive.signin offline_access");
    Json j;
    int status = 0;
    if (!rawPost(kTokenUrl, body, L"application/x-www-form-urlencoded", {}, j, status, error))
        return false;
    if (status != 200) {
        error = j["error_description"].asWString(L"Rafraîchissement du compte impossible");
        return false;
    }
    std::wstring access = j["access_token"].asWString();
    std::wstring refresh = j["refresh_token"].asWString();
    if (refresh.empty()) refresh = account.refreshToken;
    if (!exchangeMinecraft(access, refresh, account, error)) return false;
    logSuccess(L"Compte Microsoft reconnecté : " + account.name, L"Compte");
    return true;
}

bool accountTokenValid(const Account& account) {
    if (account.isOffline()) return true;
    if (account.accessToken.empty()) return false;
    if (account.expiresAt == 0) return false;
    return nowUnix() < account.expiresAt;
}

} // namespace aurora
