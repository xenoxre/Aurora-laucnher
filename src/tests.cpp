// Aurora Launcher - self tests for the core (no UI, no network required).
#include "core/auth.h"
#include "core/config.h"
#include "core/json.h"
#include "core/meta.h"
#include "core/nbt.h"
#include "core/server.h"
#include "core/sha1.h"
#include "core/util.h"
#include "core/zip.h"
#include "update/updater.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace aurora {

namespace {

int g_pass = 0;
int g_fail = 0;

void check(bool condition, const wchar_t* name, const std::wstring& detail = L"") {
    if (condition) {
        g_pass++;
        std::wprintf(L"  [OK]   %ls\n", name);
    } else {
        g_fail++;
        std::wprintf(L"  [FAIL] %ls", name);
        if (!detail.empty()) std::wprintf(L"  -> %ls", detail.c_str());
        std::wprintf(L"\n");
    }
}

void section(const wchar_t* name) { std::wprintf(L"\n%ls\n", name); }

std::wstring tempTestDir() {
    fs::path p = fs::temp_directory_path() / (L"aurora-selftest-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(p, ec);
    ensureDir(p);
    return p.wstring();
}

void testText() {
    section(L"Texte et encodage");
    std::wstring original = L"éàçœ€ — Aurora";
    std::wstring round = utf8ToWide(wideToUtf8(original));
    check(round == original, L"UTF-8 aller-retour", round);

    check(trim(std::wstring(L"  bonjour \t")) == L"bonjour", L"trim");
    check(toLower(L"AbC") == L"abc", L"minuscules");
    check(replaceAll(L"a-b-c", L"-", L" / ") == L"a / b / c", L"remplacement");
    check(join(split(L"a,b,c", L','), L"|") == L"a|b|c", L"split / join");
    check(stripMinecraftColors(L"§aBonjour §l§cmonde") == L"Bonjour monde", L"couleurs Minecraft");
    check(initials(L"Steve") == L"S", L"initiale simple");
    check(initials(L"Notch Le Grand") == L"NL", L"initiales composées");
}

void testHashAndBase64() {
    section(L"Hachage et Base64");
    check(Sha1::hexDigest("abc", 3) == "a9993e364706816aba3e25717850c26c9cd0d89d",
          L"SHA-1 de « abc »", utf8ToWide(Sha1::hexDigest("abc", 3)));
    check(Sha1::hexDigest("", 0) == "da39a3ee5e6b4b0d3255bfef95601890afd80709",
          L"SHA-1 vide", utf8ToWide(Sha1::hexDigest("", 0)));

    std::string enc = base64Encode((const uint8_t*)"Hello", 5);
    check(enc == "SGVsbG8=", L"Base64 encodage", utf8ToWide(enc));
    bool ok = false;
    Bytes dec = base64Decode(enc, &ok);
    check(ok && std::string(dec.begin(), dec.end()) == "Hello", L"Base64 décodage");

    std::wstring uuid = offlineUuid(L"Notch");
    check(uuid == L"b50ad385-829d-3141-a216-7e7d7539ba7f", L"UUID hors-ligne", uuid);
    check(formatUuid(L"b50ad385829d3141a2167e7d7539ba7f") ==
              L"b50ad385-829d-3141-a216-7e7d7539ba7f",
          L"formatage UUID");

    std::string digest;
    check(sha256Bytes("abc", 3, digest) &&
              digest == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          L"SHA-256 de « abc »", utf8ToWide(digest));
}

void testMaven() {
    section(L"Coordonnées Maven");
    std::wstring path = mavenPathForName(L"net.java.jinput:jinput-platform:2.0.5:natives-windows@jar");
    check(path == L"net/java/jinput/jinput-platform/2.0.5/jinput-platform-2.0.5-natives-windows.jar",
          L"chemin natif", path);
    std::wstring group, artifact, version, classifier, ext;
    check(parseMavenCoordinate(L"com.google.code.gson:gson:2.10.1", group, artifact, version,
                               classifier, ext) &&
              group == L"com.google.code.gson" && artifact == L"gson" && version == L"2.10.1" &&
              classifier.empty() && ext == L"jar",
          L"coordonnée simple");
}

void testJson() {
    section(L"JSON");
    Json root;
    std::string err;
    std::string text = "{\"a\":[1,2,{\"b\":\"caf\\u00e9\"}],\"ok\":true,\"n\":-12.5}";
    check(Json::parse(text, root, &err), L"analyse", utf8ToWide(err));
    check(root["ok"].asBool(false), L"booléen");
    check(root["n"].asInt(0) == -12, L"nombre");
    check(root["a"].size() == 3, L"tableau");
    check(root["a"].at(2)["b"].asWString() == L"café", L"échappement unicode",
          root["a"].at(2)["b"].asWString());

    Json rebuilt;
    std::string dumped = root.dump(2);
    check(Json::parse(dumped, rebuilt, &err), L"réanalyse du dump", utf8ToWide(err));
    check(rebuilt.dump() == root.dump(), L"dump stable");

    Json escaped = Json::object();
    escaped.set("t", L"ligne\n\"guillemets\"\\");
    Json again;
    check(Json::parse(escaped.dump(), again, &err) && again["t"].asWString() == L"ligne\n\"guillemets\"\\",
          L"échappement sortant");
}

void testZip() {
    section(L"Archives ZIP");
    std::wstring dirStr = tempTestDir();
    fs::path dir(dirStr);
    fs::path archive = dir / L"test.zip";
    std::vector<std::pair<std::string, Bytes>> entries;
    std::string hello = "Bonjour Aurora";
    entries.push_back({ "hello.txt", Bytes(hello.begin(), hello.end()) });
    Bytes binary;
    for (int i = 0; i < 512; i++) binary.push_back((uint8_t)(i * 7));
    entries.push_back({ "assets/data.bin", binary });

    std::wstring err;
    check(zipCreate(archive, entries, err), L"création", err);
    Bytes read;
    check(zipReadEntry(archive, "hello.txt", read, err) &&
              std::string(read.begin(), read.end()) == hello,
          L"lecture d'une entrée", err);
    std::vector<std::string> names;
    check(zipList(archive, names, err) && names.size() == 2, L"liste des entrées", err);

    fs::path out = dir / L"extract";
    check(zipExtractAll(archive, out, err), L"extraction", err);
    std::string extracted;
    check(readFileText(out / L"hello.txt", extracted) && extracted == hello, L"contenu extrait");

    std::wstring badErr;
    fs::path bad = dir / L"bad.zip";
    check(!zipReadEntry(bad, "x", read, badErr), L"archive absente rejetée");
    removeAll(dir);
}

void testNbt() {
    section(L"NBT");
    NbtTag root;
    root.type = NbtTag::Type::Compound;
    NbtTag name;
    name.type = NbtTag::Type::String;
    name.name = "LevelName";
    name.str = "Monde d'Aurora";
    root.children.push_back(name);
    NbtTag seed;
    seed.type = NbtTag::Type::Long;
    seed.name = "RandomSeed";
    seed.number = 42424242;
    root.children.push_back(seed);
    NbtTag list;
    list.type = NbtTag::Type::List;
    list.listType = NbtTag::Type::Int;
    list.name = "Values";
    for (int i = 0; i < 3; i++) {
        NbtTag v;
        v.type = NbtTag::Type::Int;
        v.number = i * 10;
        list.children.push_back(v);
    }
    root.children.push_back(list);

    Bytes data = nbtWrite(root);
    NbtTag parsed;
    std::wstring err;
    check(nbtParse(data, parsed, err), L"écriture puis lecture", err);
    check(parsed.getString("LevelName") == "Monde d'Aurora", L"chaîne UTF-8");
    check(parsed.getInt("RandomSeed") == 42424242, L"entier long");
    const NbtTag* found = parsed.find("Values");
    check(found && found->children.size() == 3 && found->children[2].number == 20, L"liste");

    std::wstring dirStr = tempTestDir();
    fs::path file = fs::path(dirStr) / L"level.dat";
    check(nbtWriteFile(file, root, err), L"écriture fichier", err);
    NbtTag fromFile;
    check(nbtParseFile(file, fromFile, err) && fromFile.getString("LevelName") == "Monde d'Aurora",
          L"lecture fichier", err);
    removeAll(fs::path(dirStr));
}

void testServersAndFormatting() {
    section(L"Serveurs et formatage");
    std::wstring host;
    int port = 0;
    check(parseServerAddress(L"play.exemple.fr:25566", host, port) && host == L"play.exemple.fr" &&
              port == 25566,
          L"adresse avec port", host + L":" + std::to_wstring(port));
    check(parseServerAddress(L"exemple.fr", host, port) && host == L"exemple.fr" && port == 0,
          L"adresse simple");
    check(parseServerAddress(L"[::1]:25565", host, port) && host == L"::1" && port == 25565,
          L"adresse IPv6");
    check(serverAddressString(L"exemple.fr", 25566) == L"exemple.fr:25566", L"reformatage");
    check(humanBytes(1536) == L"1.5 Ko", L"octets lisibles", humanBytes(1536));
    check(humanCount(1234567) == L"1 234 567", L"grand nombre", humanCount(1234567));
    check(quoteWinArg(L"C:\\Jeux\\Minecraft") == L"C:\\Jeux\\Minecraft", L"argument simple");
    check(quoteWinArg(L"C:\\Mon Jeu\\mc") == L"\"C:\\Mon Jeu\\mc\"", L"argument avec espace",
          quoteWinArg(L"C:\\Mon Jeu\\mc"));
    check(isValidPlayerName(L"Steve_123") && !isValidPlayerName(L"ab") &&
              !isValidPlayerName(L"nom avec espace"),
          L"validation pseudo");
}

void testUpdates() {
    section(L"Mises a jour");
    using namespace aurora::update;

    check(CompareVersions(L"1.10.0", L"1.9.9") > 0, L"comparaison 1.10 > 1.9");
    check(CompareVersions(L"2.0.0", L"2.0.0") == 0, L"comparaison egale");
    check(CompareVersions(L"2.0.0-beta", L"2.0.0") < 0, L"prerelease < release");
    check(CompareVersions(L"2.1", L"2.0.9") > 0, L"comparaison 2.1 > 2.0.9");
    check(IsValidVersion(L"2.0.0") && !IsValidVersion(L"deux") && !IsValidVersion(L""),
          L"validation de version");

    std::wstring dirStr = tempTestDir();
    fs::path dir(dirStr);
    fs::path source = dir / L"new-launcher.bin";
    fs::path manifestPath = dir / L"update-version.json";
    std::string payload = "Aurora update payload";
    check(writeFileBytes(source, Bytes(payload.begin(), payload.end())), L"ecriture source");

    std::string hash;
    check(sha256File(source, hash), L"empreinte source");
    std::wstring url = L"file:///" + source.wstring();
    std::replace(url.begin(), url.end(), L'\\', L'/');
    const std::string size = std::to_string(fileSize(source));

    std::string manifest =
        "{\"schema\":1,\"version\":\"3.1.4\",\"channel\":\"stable\","
        "\"notes\":\"Notes de test\","
        "\"launcher\":{\"url\":\"" + wideToUtf8(url) + "\",\"size\":" + size +
        ",\"sha256\":\"" + hash + "\"},"
        "\"updater\":{\"url\":\"" + wideToUtf8(url) + "\",\"size\":" + size +
        ",\"sha256\":\"" + hash + "\"}}";
    check(writeFileText(manifestPath, manifest), L"ecriture manifeste");

    Info info;
    std::wstring err;
    check(ParseManifest(manifest, info, err), L"analyse manifeste", err);
    check(info.version == L"3.1.4" && info.launcher.valid() && info.updater.valid(),
          L"champs du manifeste");
    check(info.notes == L"Notes de test", L"notes du manifeste");

    Info fetched;
    check(FetchManifest(manifestPath.wstring(), fetched, err), L"lecture manifeste local", err);
    check(fetched.version == info.version && fetched.launcher.sha256 == info.launcher.sha256,
          L"manifeste local coherent");

    fs::path dest = dir / L"downloaded.bin";
    check(DownloadAsset(info.launcher, dest, {}, {}, err), L"telechargement local verifie", err);
    std::string copied;
    check(readFileText(dest, copied) && copied == payload, L"contenu telecharge identique");

    Info corrupt = info;
    corrupt.launcher.sha256 = std::wstring(64, L'0');
    fs::path badDest = dir / L"bad.bin";
    check(!DownloadAsset(corrupt.launcher, badDest, {}, {}, err),
          L"empreinte invalide refusee");
    check(!pathExists(badDest), L"fichier corrompu non conserve");

    const std::string badManifest =
        "{\"schema\":1,\"version\":\"3.1.4\",\"launcher\":{\"url\":\"https://x.test/a\","
        "\"size\":10,\"sha256\":\"abc\"}}";
    Info bad;
    check(!ParseManifest(badManifest, bad, err), L"manifeste incomplet refuse");

    removeAll(dir);
}

void testConfig() {
    section(L"Configuration");
    std::wstring dirStr = tempTestDir();
    Paths paths;
    paths.init(fs::path(dirStr));
    Config cfg;
    cfg.ensureAccount();
    cfg.accounts[0].name = L"Testeur";
    cfg.accounts[0].uuid = offlineUuid(L"Testeur");
    cfg.ramMb = 6144;
    cfg.checkUpdates = false;
    cfg.skippedUpdateVersion = L"9.9.9";
    cfg.localServers.push_back({ L"Mon serveur", L"1.20.4", L"mon-serveur", 25566, 2048, false });
    std::wstring err;
    check(saveConfig(paths, cfg, err), L"écriture config.json", err);
    Config loaded;
    check(loadConfig(paths, loaded, err), L"lecture config.json", err);
    check(loaded.ramMb == 6144 && loaded.accounts.size() == 1 &&
              loaded.accounts[0].name == L"Testeur",
          L"valeurs conservées");
    check(loaded.localServers.size() == 1 && loaded.localServers[0].port == 25566,
          L"serveurs locaux conservés");
    check(!loaded.checkUpdates && loaded.skippedUpdateVersion == L"9.9.9",
          L"préférences de mise à jour conservées");
    removeAll(fs::path(dirStr));
}

} // namespace

int runSelfTests(bool verbose) {
    (void)verbose;
    g_pass = 0;
    g_fail = 0;
    std::wprintf(L"Aurora Launcher — auto-tests du noyau\n");
    std::wprintf(L"====================================\n");
    testText();
    testHashAndBase64();
    testJson();
    testMaven();
    testZip();
    testNbt();
    testServersAndFormatting();
    testUpdates();
    testConfig();
    std::wprintf(L"\n====================================\n");
    std::wprintf(L"Résultat : %d réussi(s), %d échec(s)\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

} // namespace aurora
