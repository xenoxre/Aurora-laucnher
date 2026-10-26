#include "app.h"
#include "ui/ui.h"
#include "core/util.h"

using namespace aurora;
using namespace aurora::ui;

void App::pageServers(Painter& p, const Rect& area) {
    drawPageHeader(p, L"Serveurs", L"Rejoindre, pinger et créer des serveurs locaux", area);
    float y = 72;

    // ---------------------------------------------------------------- quick join
    Rect card(area.x + 24, area.y + y, (area.w - 56) / 2.0f, 150);
    p.fill(card, P.card, 14);
    p.stroke(card, P.borderSoft, 1, 14);
    p.text(L"Rejoindre un serveur", Rect(card.x + 24, card.y + 18, card.w - 48, 22), P.text, Font::H3);
    p.text(L"Adresse", Rect(card.x + 24, card.y + 54, card.w - 48, 16), P.textDim, Font::Small);
    static std::string pid = "pingaddr";
    textField(pid, pingAddress_, Rect(card.x + 24, card.y + 74, card.w - 260, 38),
              L"mc.exemple.fr:25565", Icon::Globe);
    if (button("pingbtn", pingLoading_ ? L"Ping..." : L"Pinger",
               Rect(card.right() - 224, card.y + 74, 100, 38), ButtonKind::Secondary, !pingLoading_))
        pingServerAddress(pingAddress_);
    if (button("joinbtn", L"Rejoindre", Rect(card.right() - 116, card.y + 74, 92, 38),
               ButtonKind::Primary, !pingAddress_.empty(), Icon::Play))
        joinServer(pingAddress_);
    if (pingStatus_.ok) {
        p.text(L"En ligne - " + std::to_wstring(pingStatus_.online) + L"/" +
                   std::to_wstring(pingStatus_.maxPlayers) + L" joueurs - " +
                   std::to_wstring(pingStatus_.pingMs) + L"ms",
               Rect(card.x + 24, card.y + 122, card.w - 48, 16), P.success, Font::Small);
    } else if (!pingError_.empty()) {
        p.text(pingError_, Rect(card.x + 24, card.y + 122, card.w - 48, 16), P.danger,
               Font::Small);
    } else {
        p.text(L"Astuce : ajoutez :25565 si le port n'est pas indiqué",
               Rect(card.x + 24, card.y + 122, card.w - 48, 16), P.textMuted, Font::Small);
    }

    // ---------------------------------------------------------------- local server status
    Rect card2(card.right() + 8, area.y + y, card.w, 150);
    p.fill(card2, P.card, 14);
    p.stroke(card2, P.borderSoft, 1, 14);
    p.text(L"Serveur local", Rect(card2.x + 24, card2.y + 18, card2.w - 48, 22), P.text, Font::H3);
    if (localServer_ && localServer_->running()) {
        p.text(L"En cours - " + localServerName_,
               Rect(card2.x + 24, card2.y + 54, card2.w - 48, 18), P.success, Font::Body);
        p.text(L"Dossier : " + localServerDir_,
               Rect(card2.x + 24, card2.y + 76, card2.w - 48, 16), P.textMuted, Font::Small);
        if (button("stoplocal", L"Arrêter", Rect(card2.x + 24, card2.bottom() - 50, 110, 38),
                   ButtonKind::Danger))
            stopLocalServer();
        if (button("openlocal", L"Ouvrir dossier",
                   Rect(card2.x + 142, card2.bottom() - 50, 150, 38), ButtonKind::Ghost))
            openInExplorer(paths_.serversDir / localServerDir_);
    } else {
        p.text(L"Créez et démarrez un serveur Minecraft local",
               Rect(card2.x + 24, card2.y + 54, card2.w - 48, 18), P.textDim, Font::Body);
        if (button("createlocal", L"Créer un serveur local",
                   Rect(card2.x + 24, card2.bottom() - 50, 210, 38), ButtonKind::Primary, true,
                   Icon::Plus))
            openDialog(Dialog::CreateServer);
    }

    // ---------------------------------------------------------------- saved servers
    Rect list(area.x + 24, area.y + y + 164, area.w - 48, area.h - y - 184);
    p.fill(list, P.card, 14);
    p.stroke(list, P.borderSoft, 1, 14);
    p.text(L"Serveurs enregistrés", Rect(list.x + 24, list.y + 18, list.w - 220, 22), P.text,
           Font::H3);
    drawBadge(p, std::to_wstring(config_.localServers.size()).c_str(), list.right() - 60,
              list.y + 17, P.textDim, P.cardActive);

    if (config_.localServers.empty()) {
        bool create = false;
        drawEmptyState(p, Rect(list.x, list.y + 50, list.w, list.h - 70), Icon::Server,
                       L"Aucun serveur local",
                       L"Créez un serveur pour jouer entre amis sur votre réseau", L"Créer un serveur",
                       &create);
        if (create) openDialog(Dialog::CreateServer);
        return;
    }

    float lh = 76;
    float content = (float)config_.localServers.size() * lh + 12;
    float off = beginScroll("srvscroll", Rect(list.x + 12, list.y + 52, list.w - 24, list.h - 64),
                            content);
    float ly = list.y + 52 - off + 6;
    for (size_t i = 0; i < config_.localServers.size(); i++) {
        LocalServerConfig& s = config_.localServers[i];
        Rect r(list.x + 12, ly, list.w - 24, 64);
        if (r.bottom() < list.y + 52 || r.y > list.bottom() - 8) {
            ly += lh;
            continue;
        }
        bool running = localServer_ && localServer_->running() && localServerDir_ == s.dir;
        p.fill(r, P.inset, 10);
        p.stroke(r, running ? withAlpha(P.success, 90) : P.borderSoft, 1, 10);

        Rect iconRect(r.x + 16, r.cy() - 12, 24, 24);
        p.fill(iconRect, running ? rgba(70, 209, 124, 28) : P.cardActive, 7);
        drawIcon(p, Icon::Server, iconRect.inset(4), running ? P.success : P.textDim);

        std::wstring name = s.name.empty() ? s.dir : s.name;
        p.text(ellipsize(p, name, Font::BodyStrong, r.w - 340),
               Rect(r.x + 52, r.y + 12, r.w - 340, 18), P.text, Font::BodyStrong);
        std::wstring meta = (s.version.empty() ? L"? " : s.version) + L" - 127.0.0.1:" +
                            std::to_wstring(s.port) + L" - " + std::to_wstring(s.ramMb) + L" Mo";
        p.text(meta, Rect(r.x + 52, r.y + 34, r.w - 340, 16),
               running ? P.success : P.textMuted, Font::Small);
        if (running)
            drawBadge(p, L"En ligne", r.x + 52 + p.measure(meta, Font::Small) + 10, r.y + 35,
                      P.success, rgba(70, 209, 124, 26));

        std::string sid = wideToUtf8(s.dir);
        if (iconButton(("srvopen_" + sid).c_str(), Icon::Folder,
                       Rect(r.right() - 48, r.cy() - 16, 32, 32), L"Ouvrir le dossier"))
            openInExplorer(paths_.serversDir / s.dir);
        if (running) {
            if (button(("srvjoin_" + sid).c_str(), L"Rejoindre",
                       Rect(r.right() - 156, r.cy() - 16, 100, 32), ButtonKind::Primary, true,
                       Icon::Play))
                joinServer(L"127.0.0.1:" + std::to_wstring(s.port), s.version);
            if (button(("srvstop_" + sid).c_str(), L"Arrêter",
                       Rect(r.right() - 268, r.cy() - 16, 104, 32), ButtonKind::Danger))
                stopLocalServer();
        } else {
            bool free = !(localServer_ && localServer_->running());
            if (button(("srvstart_" + sid).c_str(), L"Démarrer",
                       Rect(r.right() - 156, r.cy() - 16, 100, 32), ButtonKind::Secondary, free,
                       Icon::Power))
                startLocalServer(s);
            if (button(("srvjoinoff_" + sid).c_str(), L"Rejoindre",
                       Rect(r.right() - 268, r.cy() - 16, 104, 32), ButtonKind::Ghost, false,
                       Icon::Play))
                joinServer(L"127.0.0.1:" + std::to_wstring(s.port), s.version);
        }
        ly += lh;
    }
    endScroll();
}
