#include "app.h"
#include "ui/ui.h"
#include "core/util.h"
#include "../version.h"

using namespace aurora;
using namespace aurora::ui;

void App::drawDialogs(Painter& p, float w, float h) {
    if (dialog_ == Dialog::None) return;
    
    if (dialog_ == Dialog::NewInstance) {
        Rect card(w/2 - 230, h/2 - 170, 460, 340);
        drawModalShell(p, w, h, L"Nouvelle instance", L"Créer une instance Minecraft isolée", card);
        p.text(L"Nom de l'instance", Rect(card.x + 28, card.y + 90, card.w - 56, 16), P.text, Font::SmallStrong);
        static std::string nin = "ninstname";
        textField(nin, newName_, Rect(card.x + 28, card.y + 112, card.w - 56, 38), L"Ma nouvelle instance");
        p.text(L"Version Minecraft", Rect(card.x + 28, card.y + 164, card.w - 56, 16), P.text, Font::SmallStrong);
        std::vector<std::wstring> vids;
        for (auto& v : manifest_.versions) if (v.type == L"release") vids.push_back(v.id);
        if (vids.empty()) vids = versionIds();
        dropdown("ninstver", newVersionIndex_, vids, Rect(card.x + 28, card.y + 186, card.w - 56, 38));
        p.text(L"Chargeur de mods", Rect(card.x + 28, card.y + 238, card.w - 56, 16), P.text, Font::SmallStrong);
        const wchar_t* ld[] = { L"Aucun", L"Fabric", L"Quilt" };
        std::vector<std::wstring> lds(ld, ld+3);
        segmented("ninstld", newLoader_, lds, Rect(card.x + 28, card.y + 260, card.w - 56, 34));
        if (button("ninstcancel", L"Annuler", Rect(card.right()-240, card.bottom()-62, 100, 38), ButtonKind::Ghost)) closeDialog();
        if (button("ninstok", L"Créer", Rect(card.right()-128, card.bottom()-62, 100, 38), ButtonKind::Primary)) createInstanceFromDialog();
    } else if (dialog_ == Dialog::Accounts) {
        Rect card(w/2 - 230, h/2 - 190, 460, 380);
        drawModalShell(p, w, h, L"Comptes", L"Gérer vos comptes Minecraft", card);
        p.text(L"Compte Microsoft", Rect(card.x + 28, card.y + 90, card.w - 56, 18), P.text, Font::BodyStrong);
        if (button("accms", L"Se connecter avec Microsoft", Rect(card.x + 28, card.y + 118, 230, 38), ButtonKind::Secondary, true, Icon::Microsoft)) { closeDialog(); openDialog(Dialog::Microsoft); startMicrosoftLogin(); }
        p.text(L"Compte hors ligne", Rect(card.x + 28, card.y + 180, card.w - 56, 18), P.text, Font::BodyStrong);
        p.text(L"Nom d'utilisateur", Rect(card.x + 28, card.y + 208, card.w - 56, 16), P.text, Font::SmallStrong);
        static std::string offn = "offname";
        textField(offn, offlineName_, Rect(card.x + 28, card.y + 230, card.w - 56, 38), L"Steve");
        if (button("offadd", L"Ajouter compte hors ligne", Rect(card.x + 28, card.bottom()-62, 210, 38), ButtonKind::Primary)) addOfflineAccount();
        if (button("acccancel", L"Fermer", Rect(card.right()-128, card.bottom()-62, 100, 38), ButtonKind::Ghost)) closeDialog();
    } else if (dialog_ == Dialog::Microsoft) {
        Rect card(w/2 - 250, h/2 - 150, 500, 300);
        drawModalShell(p, w, h, L"Connexion Microsoft", L"Authentification via code d'appareil", card);
        MsLoginState snap;
        {
            std::lock_guard<std::mutex> lock(ms_.mutex);
            snap.running = ms_.running.load();
            snap.userCode = ms_.userCode;
            snap.verificationUri = ms_.verificationUri;
            snap.status = ms_.status;
            snap.error = ms_.error;
            snap.account = ms_.account;
            snap.ready = ms_.ready;
            snap.done = ms_.done;
            snap.ok = ms_.ok;
        }
        if (snap.done && snap.ok) {
            p.text(L"Connexion réussie !", Rect(card.x + 28, card.y + 90, card.w - 56, 24), P.success, Font::H3);
            p.text(L"Compte : " + snap.account.name, Rect(card.x + 28, card.y + 122, card.w - 56, 18), P.text, Font::Body);
            if (button("msok", L"Fermer", Rect(card.right()-128, card.bottom()-62, 100, 38), ButtonKind::Primary)) { processMicrosoftResult(); closeDialog(); }
        } else if (snap.done && !snap.ok) {
            p.text(L"Échec de connexion", Rect(card.x + 28, card.y + 90, card.w - 56, 24), P.danger, Font::H3);
            p.text(snap.error.empty() ? L"Erreur inconnue" : snap.error, Rect(card.x + 28, card.y + 122, card.w - 56, 40), P.textDim, Font::Body);
            if (button("msretry", L"Réessayer", Rect(card.right()-240, card.bottom()-62, 110, 38), ButtonKind::Secondary)) { closeDialog(); openDialog(Dialog::Microsoft); startMicrosoftLogin(); }
            if (button("msclose", L"Fermer", Rect(card.right()-128, card.bottom()-62, 100, 38), ButtonKind::Ghost)) closeDialog();
        } else if (snap.running && !snap.userCode.empty()) {
            p.text(L"Code à entrer :", Rect(card.x + 28, card.y + 90, card.w - 56, 18), P.textDim, Font::Body);
            p.text(snap.userCode, Rect(card.x + 28, card.y + 118, card.w - 56, 42), P.accent, Font::Display);
            p.text(L"Ouvrez : " + snap.verificationUri, Rect(card.x + 28, card.y + 170, card.w - 56, 18), P.text, Font::Body);
            p.text(L"En attente d'autorisation...", Rect(card.x + 28, card.y + 196, card.w - 56, 18), P.textMuted, Font::Small);
            if (button("mscancel", L"Annuler", Rect(card.right()-128, card.bottom()-62, 100, 38), ButtonKind::Ghost)) { cancelMicrosoftLogin(); closeDialog(); }
        } else if (snap.running) {
            p.text(L"Démarrage de la connexion...", Rect(card.x + 28, card.y + 120, card.w - 56, 24), P.text, Font::H3);
            drawSpinner(p, card.cx(), card.cy() + 20, 18, P.accent);
            if (button("mscancel2", L"Annuler", Rect(card.right()-128, card.bottom()-62, 100, 38), ButtonKind::Ghost)) { cancelMicrosoftLogin(); closeDialog(); }
        } else {
            p.text(L"Connexion avec un compte Microsoft", Rect(card.x + 28, card.y + 90, card.w - 56, 24), P.text, Font::H3);
            p.text(L"Un code d'appareil sera généré et votre navigateur s'ouvrira.", Rect(card.x + 28, card.y + 122, card.w - 56, 18), P.textDim, Font::Body);
            if (button("msstart", L"Démarrer la connexion", Rect(card.x + 28, card.bottom()-62, 210, 38), ButtonKind::Primary)) startMicrosoftLogin();
            if (button("mscc", L"Annuler", Rect(card.right()-128, card.bottom()-62, 100, 38), ButtonKind::Ghost)) closeDialog();
        }
    } else if (dialog_ == Dialog::CreateServer) {
        Rect card(w/2 - 230, h/2 - 170, 460, 340);
        drawModalShell(p, w, h, L"Créer un serveur local", L"Configurer et télécharger le serveur", card);
        p.text(L"Nom du serveur", Rect(card.x + 28, card.y + 90, card.w - 56, 16), P.text, Font::SmallStrong);
        static std::string sn = "srvname";
        textField(sn, serverName_, Rect(card.x + 28, card.y + 112, card.w - 56, 38), L"Mon serveur");
        p.text(L"Version Minecraft", Rect(card.x + 28, card.y + 164, (card.w-84)/2, 16), P.text, Font::SmallStrong);
        std::vector<std::wstring> svids;
        for (auto& v : manifest_.versions) if (v.type == L"release") svids.push_back(v.id);
        static int sver = 0;
        // find
        dropdown("srvver", sver, svids, Rect(card.x + 28, card.y + 186, (card.w-84)/2, 38));
        if (sver >=0 && sver < (int)svids.size()) serverVersion_ = svids[sver];
        p.text(L"Port", Rect(card.x + 28 + (card.w-84)/2 + 28, card.y + 164, (card.w-84)/2, 16), P.text, Font::SmallStrong);
        static std::string sport = "srvport";
        textField(sport, serverPort_, Rect(card.x + 28 + (card.w-84)/2 + 28, card.y + 186, (card.w-84)/2, 38), L"25565");
        p.text(L"Mémoire RAM (Mo)", Rect(card.x + 28, card.y + 238, card.w - 56, 16), P.text, Font::SmallStrong);
        static int sram = 4096;
        sram = serverRam_;
        sliderInt("srvram", sram, 1024, 8192, 512, Rect(card.x + 28, card.y + 262, card.w - 56, 28), L" Mo");
        serverRam_ = sram;
        if (button("srvcancel", L"Annuler", Rect(card.right()-240, card.bottom()-62, 100, 38), ButtonKind::Ghost)) closeDialog();
        if (button("srvok", L"Créer", Rect(card.right()-128, card.bottom()-62, 100, 38), ButtonKind::Primary)) createServerFromDialog();
    } else if (dialog_ == Dialog::About) {
        Rect card(w/2 - 200, h/2 - 120, 400, 240);
        drawModalShell(p, w, h, L"À propos", L"", card);
        drawIcon(p, Icon::Cube, Rect(card.cx()-21, card.y + 90, 42, 42), P.accent);
        p.text(L"Aurora Launcher", Rect(card.x, card.y + 140, card.w, 26), P.text, Font::H3, Align::Center);
        p.text(L"Version " + std::wstring(AURORA_VERSION_W), Rect(card.x, card.y + 168, card.w, 18), P.textDim, Font::Body, Align::Center);
        if (button("aboutclose", L"Fermer", Rect(card.cx()-50, card.bottom()-62, 100, 38), ButtonKind::Primary)) closeDialog();
    }
}
