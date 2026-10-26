#include "server.h"
#include "http.h"
#include "json.h"
#include "log.h"
#include "meta.h"
#include "nbt.h"
#include "util.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windns.h>

#include <algorithm>
#include <chrono>

namespace aurora {

namespace {

struct WsaInit {
    WsaInit() { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
    ~WsaInit() { WSACleanup(); }
};
WsaInit g_wsa;

void writeVarInt(Bytes& out, int32_t value) {
    uint32_t v = (uint32_t)value;
    do {
        uint8_t b = v & 0x7F;
        v >>= 7;
        if (v) b |= 0x80;
        out.push_back(b);
    } while (v);
}

int32_t readVarInt(const uint8_t*& p, const uint8_t* end) {
    int32_t value = 0;
    int shift = 0;
    while (p < end) {
        uint8_t b = *p++;
        value |= (int32_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) return value;
        shift += 7;
        if (shift > 35) return -1;
    }
    return -1;
}

void writeString(Bytes& out, const std::string& s) {
    writeVarInt(out, (int32_t)s.size());
    out.insert(out.end(), s.begin(), s.end());
}

bool sendAll(SOCKET s, const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int n = send(s, (const char*)data + sent, (int)(len - sent), 0);
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

bool recvExact(SOCKET s, uint8_t* data, size_t len) {
    size_t got = 0;
    while (got < len) {
        int n = recv(s, (char*)data + got, (int)(len - got), 0);
        if (n <= 0) return false;
        got += (size_t)n;
    }
    return true;
}

bool recvVarInt(SOCKET s, int32_t& value) {
    value = 0;
    for (int shift = 0; shift <= 35; shift += 7) {
        uint8_t b = 0;
        if (!recvExact(s, &b, 1)) return false;
        value |= (int32_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) return true;
    }
    return false;
}

std::wstring socketError(int code) {
    switch (code) {
        case WSAECONNREFUSED: return L"Connexion refusée";
        case WSAETIMEDOUT: return L"Délai de connexion dépassé";
        case WSAENETUNREACH: return L"Réseau injoignable";
        case WSAEHOSTUNREACH: return L"Hôte injoignable";
        case WSAHOST_NOT_FOUND:
        case WSATRY_AGAIN: return L"Nom d'hôte introuvable";
        case WSAECONNRESET: return L"Connexion réinitialisée";
        default: return formatW(L"Erreur réseau (%d)", code);
    }
}

std::wstring flattenMotd(const Json& desc) {
    if (desc.isString()) return desc.asWString();
    if (!desc.isObject()) return L"";
    std::wstring out = desc["text"].asWString();
    for (auto& e : desc["extra"].items()) out += flattenMotd(e);
    return out;
}

bool querySrv(const std::wstring& host, std::wstring& target, int& port) {
    std::wstring name = L"_minecraft._tcp." + host;
    PDNS_RECORDW record = nullptr;
    DNS_STATUS status = DnsQuery_W(name.c_str(), DNS_TYPE_SRV, DNS_QUERY_STANDARD, nullptr,
                                   &record, nullptr);
    if (status != 0 || !record) return false;
    bool found = false;
    for (PDNS_RECORDW r = record; r; r = r->pNext) {
        if (r->wType == DNS_TYPE_SRV && r->Data.SRV.pNameTarget) {
            target = r->Data.SRV.pNameTarget;
            port = (int)r->Data.SRV.wPort;
            while (!target.empty() && target.back() == L'.') target.pop_back();
            found = !target.empty();
            break;
        }
    }
    DnsRecordListFree(record, DnsFreeRecordList);
    return found;
}

} // namespace

bool parseServerAddress(const std::wstring& address, std::wstring& host, int& port) {
    std::wstring a = trim(address);
    host.clear();
    port = 0;
    if (a.empty()) return false;
    if (a[0] == L'[') {
        size_t close = a.find(L']');
        if (close == std::wstring::npos) return false;
        host = a.substr(1, close - 1);
        if (close + 1 < a.size() && a[close + 1] == L':') {
            port = (int)wcstol(a.c_str() + close + 2, nullptr, 10);
        }
        return !host.empty();
    }
    size_t first = a.find(L':');
    size_t last = a.rfind(L':');
    if (last != std::wstring::npos && first == last) {
        std::wstring portPart = a.substr(last + 1);
        if (!portPart.empty() && portPart.find_first_not_of(L"0123456789") == std::wstring::npos) {
            host = a.substr(0, last);
            port = (int)wcstol(portPart.c_str(), nullptr, 10);
        } else {
            host = a;
        }
    } else {
        host = a;
    }
    host = trim(host);
    if (port < 0 || port > 65535) port = 0;
    return !host.empty();
}

std::wstring serverAddressString(const std::wstring& host, int port) {
    if (port <= 0 || port == 25565) return host;
    return host + L":" + std::to_wstring(port);
}

ServerStatus pingServer(const std::wstring& address, int timeoutMs) {
    ServerStatus st;
    st.motd = L"";
    std::wstring host;
    int port = 0;
    if (!parseServerAddress(address, host, port)) {
        st.error = L"Adresse invalide";
        return st;
    }
    std::wstring connectHost = host;
    int connectPort = port;
    if (connectPort == 0) {
        std::wstring srvTarget;
        int srvPort = 0;
        if (querySrv(host, srvTarget, srvPort)) {
            connectHost = srvTarget;
            connectPort = srvPort;
        } else {
            connectPort = 25565;
        }
    }
    std::string hostUtf8 = wideToUtf8(connectHost);
    std::string portStr = std::to_string(connectPort);
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* result = nullptr;
    int gai = getaddrinfo(hostUtf8.c_str(), portStr.c_str(), &hints, &result);
    if (gai != 0 || !result) {
        st.error = L"Nom d'hôte introuvable";
        return st;
    }
    SOCKET sock = INVALID_SOCKET;
    int lastError = 0;
    for (addrinfo* ai = result; ai; ai = ai->ai_next) {
        SOCKET s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == INVALID_SOCKET) { lastError = WSAGetLastError(); continue; }
        u_long nonBlocking = 1;
        ioctlsocket(s, FIONBIO, &nonBlocking);
        int rc = connect(s, ai->ai_addr, (int)ai->ai_addrlen);
        bool connected = rc == 0;
        if (!connected && WSAGetLastError() == WSAEWOULDBLOCK) {
            fd_set wf, ef;
            FD_ZERO(&wf); FD_SET(s, &wf);
            FD_ZERO(&ef); FD_SET(s, &ef);
            timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
            int sel = select(0, nullptr, &wf, &ef, &tv);
            if (sel > 0 && FD_ISSET(s, &wf)) {
                int err = 0;
                int len = sizeof(err);
                getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
                connected = err == 0;
                if (!connected) lastError = err;
            } else {
                lastError = WSAETIMEDOUT;
            }
        } else if (!connected) {
            lastError = WSAGetLastError();
        }
        if (connected) {
            nonBlocking = 0;
            ioctlsocket(s, FIONBIO, &nonBlocking);
            sock = s;
            break;
        }
        closesocket(s);
    }
    freeaddrinfo(result);
    if (sock == INVALID_SOCKET) {
        st.error = socketError(lastError ? lastError : WSAETIMEDOUT);
        return st;
    }
    DWORD timeout = (DWORD)timeoutMs;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    auto started = std::chrono::steady_clock::now();
    Bytes payload;
    writeVarInt(payload, 0);
    writeVarInt(payload, 767);
    writeString(payload, wideToUtf8(host));
    payload.push_back((uint8_t)(connectPort >> 8));
    payload.push_back((uint8_t)(connectPort & 0xFF));
    writeVarInt(payload, 1);
    Bytes frame;
    writeVarInt(frame, (int32_t)payload.size());
    frame.insert(frame.end(), payload.begin(), payload.end());
    frame.push_back(0x01); // status request, length 1
    frame.push_back(0x00);
    if (!sendAll(sock, frame.data(), frame.size())) {
        st.error = L"Envoi impossible";
        closesocket(sock);
        return st;
    }
    int32_t len = 0;
    if (!recvVarInt(sock, len) || len <= 0 || len > 4 * 1024 * 1024) {
        st.error = L"Réponse invalide";
        closesocket(sock);
        return st;
    }
    Bytes response((size_t)len);
    if (!recvExact(sock, response.data(), response.size())) {
        st.error = L"Réponse tronquée";
        closesocket(sock);
        return st;
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    st.pingMs = (int)elapsed;
    const uint8_t* p = response.data();
    const uint8_t* end = response.data() + response.size();
    int32_t packetId = readVarInt(p, end);
    if (packetId != 0) {
        st.error = L"Réponse inattendue";
        closesocket(sock);
        return st;
    }
    int32_t strLen = readVarInt(p, end);
    if (strLen < 0 || p + strLen > end) {
        st.error = L"Réponse JSON invalide";
        closesocket(sock);
        return st;
    }
    std::string jsonText((const char*)p, (size_t)strLen);
    closesocket(sock);
    Json json;
    std::string perr;
    if (!Json::parse(jsonText, json, &perr)) {
        st.error = L"Réponse JSON invalide";
        return st;
    }
    st.ok = true;
    st.motd = stripMinecraftColors(flattenMotd(json["description"]));
    st.motd = trim(st.motd);
    if (st.motd.empty()) st.motd = L"—";
    st.online = (int)json["players"]["online"].asInt(0);
    st.maxPlayers = (int)json["players"]["max"].asInt(0);
    st.versionName = json["version"]["name"].asWString();
    st.protocol = (int)json["version"]["protocol"].asInt(0);
    std::wstring favicon = json["favicon"].asWString();
    size_t comma = favicon.find(L"base64,");
    if (comma != std::wstring::npos) {
        bool ok = false;
        Bytes icon = base64Decode(wideToUtf8(favicon.substr(comma + 7)), &ok);
        if (ok && icon.size() > 8) st.favicon = std::move(icon);
    }
    return st;
}

bool readServersDat(const fs::path& path, std::vector<ServerEntry>& out, std::wstring& error) {
    out.clear();
    if (!pathExists(path)) return true;
    NbtTag root;
    if (!nbtParseFile(path, root, error)) return false;
    const NbtTag* list = root.find("servers");
    if (!list || list->type != NbtTag::Type::List) return true;
    for (auto& child : list->children) {
        if (child.type != NbtTag::Type::Compound) continue;
        ServerEntry e;
        e.name = utf8ToWide(child.getString("name"));
        e.ip = utf8ToWide(child.getString("ip"));
        e.acceptTextures = child.getInt("acceptTextures", 1) != 0;
        std::string iconB64 = child.getString("icon");
        if (!iconB64.empty()) {
            bool ok = false;
            Bytes icon = base64Decode(iconB64, &ok);
            if (ok) e.icon = std::move(icon);
        }
        if (!e.ip.empty()) out.push_back(std::move(e));
    }
    return true;
}

bool writeServersDat(const fs::path& path, const std::vector<ServerEntry>& entries,
                     std::wstring& error) {
    NbtTag root;
    root.type = NbtTag::Type::Compound;
    root.name = "";
    NbtTag servers;
    servers.type = NbtTag::Type::List;
    servers.listType = NbtTag::Type::Compound;
    servers.name = "servers";
    for (auto& e : entries) {
        NbtTag child;
        child.type = NbtTag::Type::Compound;
        NbtTag name;
        name.type = NbtTag::Type::String;
        name.name = "name";
        name.str = wideToUtf8(e.name);
        NbtTag ip;
        ip.type = NbtTag::Type::String;
        ip.name = "ip";
        ip.str = wideToUtf8(e.ip);
        NbtTag accept;
        accept.type = NbtTag::Type::Byte;
        accept.name = "acceptTextures";
        accept.number = e.acceptTextures ? 1 : 0;
        child.children.push_back(std::move(name));
        child.children.push_back(std::move(ip));
        if (!e.icon.empty()) {
            NbtTag icon;
            icon.type = NbtTag::Type::String;
            icon.name = "icon";
            icon.str = base64Encode(e.icon);
            child.children.push_back(std::move(icon));
        }
        child.children.push_back(std::move(accept));
        servers.children.push_back(std::move(child));
    }
    root.children.push_back(std::move(servers));
    return nbtWriteFile(path, root, error);
}

bool createLocalServer(const Paths& paths, LocalServerConfig& config, JobContext& job,
                       std::wstring& error) {
    if (config.version.empty()) { error = L"Version serveur manquante"; return false; }
    if (config.dir.empty()) {
        std::wstring dir = config.name;
        for (auto& c : dir) {
            if (!iswalnum(c) && c != L'-' && c != L'_') c = L'-';
        }
        if (dir.empty()) dir = L"serveur";
        config.dir = dir;
    }
    fs::path dir = paths.serversDir / config.dir;
    ensureDir(dir);
    job.report(L"Résolution de la version " + config.version + L"…", 0.02f);
    ResolvedVersion ver;
    if (!resolveVersion(paths, config.version, ver, true, error)) return false;
    if (ver.serverUrl.empty()) {
        error = L"Cette version de Minecraft ne fournit pas de serveur.jar téléchargeable.";
        return false;
    }
    fs::path jar = dir / L"server.jar";
    job.report(L"Téléchargement du serveur (" + humanBytes(ver.serverSize) + L")…", 0.05f);
    std::wstring derr;
    bool ok = downloadFile(ver.serverUrl, jar, ver.serverSha1, ver.serverSize,
        [&](uint64_t received, uint64_t total) {
            if (!total) total = ver.serverSize;
            float p = total ? 0.05f + 0.80f * (float)received / (float)total : 0.4f;
            job.report(L"Serveur — " + humanBytes(received) +
                       (total ? L" / " + humanBytes(total) : L""), p);
        }, [&]() { return job.cancelled(); }, derr);
    if (!ok) { error = L"Serveur : " + derr; return false; }

    fs::path eula = dir / L"eula.txt";
    if (!pathExists(eula)) {
        writeFileText(eula, "# Accepté via Aurora Launcher\n# https://aka.ms/MinecraftEULA\neula=true\n");
    }
    fs::path props = dir / L"server.properties";
    bool onlineMode = config.onlineMode;
    if (!pathExists(props)) {
        std::string p;
        p += "server-port=" + std::to_string(config.port) + "\r\n";
        p += "motd=Aurora Launcher - " + wideToUtf8(config.name) + "\r\n";
        p += "max-players=20\r\n";
        p += std::string("online-mode=") + (onlineMode ? "true" : "false") + "\r\n";
        p += "enforce-secure-profile=false\r\n";
        p += "level-name=world\r\n";
        p += "gamemode=survival\r\n";
        p += "difficulty=easy\r\n";
        p += "view-distance=10\r\n";
        p += "simulation-distance=10\r\n";
        p += "spawn-protection=0\r\n";
        p += "allow-flight=false\r\n";
        p += "white-list=false\r\n";
        p += "pvp=true\r\n";
        p += "enable-command-block=false\r\n";
        p += "network-compression-threshold=256\r\n";
        if (!writeFileText(props, p)) { error = L"Écriture server.properties impossible"; return false; }
    }
    Json meta = Json::object();
    meta.set("name", config.name);
    meta.set("version", config.version);
    meta.set("port", config.port);
    meta.set("ramMb", config.ramMb);
    meta.set("onlineMode", config.onlineMode);
    meta.set("createdAt", nowStampUtf8());
    writeFileText(dir / L"aurora-server.json", meta.dump(2));
    job.report(L"Serveur prêt", 1.0f);
    logSuccess(L"Serveur créé : " + config.name + L" (" + config.version + L")", L"Serveur");
    return true;
}

} // namespace aurora
