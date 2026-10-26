#include "app.h"
#include "ui/ui.h"
#include "core/util.h"
#include "core/java.h"
#include "../version.h"

using namespace aurora;
using namespace aurora::ui;

void App::pageSettings(Painter& p, const Rect& area) {
    drawPageHeader(p, L"Paramètres", L"Java, mémoire, comptes Microsoft et personnalisation", area);
    float y = 72;
    
    // sidebar tabs
    Rect tabs(area.x + 24, area.y + y, 200, area.h - y - 20);
    p.fill(tabs, P.card, 14);
    p.stroke(tabs, P.borderSoft, 1, 14);
    const wchar_t* tnames[] = { L"Général", L"Java", L"Comptes", L"Réseau", L"À propos" };
    float ty = tabs.y + 12;
    for (int i = 0; i < 5; i++) {
        Rect tr(tabs.x + 10, ty, tabs.w - 20, 36);
        bool act = settingsSection_ == i;
        if (act) p.fill(tr, P.cardHover, 10);
        p.text(tnames[i], Rect(tr.x + 12, tr.y, tr.w - 24, tr.h), act ? P.text : P.textDim, act ? Font::BodyStrong : Font::Body);
        if (in.pressed && in.hover(tr)) settingsSection_ = i;
        ty += 42;
    }
    if (settingsSection_ < 0) settingsSection_ = 0;

    Rect panel(tabs.right()+10, area.y + y, area.w - tabs.w - 58, area.h - y - 20);
    p.fill(panel, P.card, 14);
    p.stroke(panel, P.borderSoft, 1, 14);

    if (settingsSection_ == 0) {
        p.text(L"Général", Rect(panel.x + 28, panel.y + 22, panel.w - 56, 26), P.text, Font::H3);
        p.text(L"Mémoire RAM allouée (JVM)", Rect(panel.x + 28, panel.y + 72, 300, 18), P.text, Font::BodyStrong);
        static int ramv = 4096;
        ramv = config_.ramMb;
        sliderInt("ram", ramv, 1024, 16384, 512, Rect(panel.x + 28, panel.y + 96, panel.w - 56, 28), L" Mo");
        config_.ramMb = ramv;
        p.text(L"Fermer le lanceur au lancement du jeu", Rect(panel.x + 28, panel.y + 150, 320, 18), P.text, Font::BodyStrong);
        toggle("closelaunch", config_.closeOnLaunch, Rect(panel.x + 28, panel.y + 174, 44, 24));
        p.text(L"Résolution du jeu", Rect(panel.x + 28, panel.y + 220, 200, 18), P.text, Font::BodyStrong);
        const wchar_t* ress[] = { L"854 x 480", L"1024 x 576", L"1280 x 720", L"1366 x 768", L"1600 x 900", L"1920 x 1080" };
        std::vector<std::wstring> rlist(ress, ress+6);
        dropdown("res", resolutionChoice_, rlist, Rect(panel.x + 28, panel.y + 244, 240, 38));
        struct R { int w,h; } rv[] = {{854,480},{1024,576},{1280,720},{1366,768},{1600,900},{1920,1080}};
        if (resolutionChoice_ >=0 && resolutionChoice_<6) { config_.width = rv[resolutionChoice_].w; config_.height = rv[resolutionChoice_].h; }
        if (button("savegen", L"Enregistrer", Rect(panel.x + 28, panel.bottom()-62, 140, 38), ButtonKind::Primary)) { saveConfigNow(); notify(L"Paramètres enregistrés", false); }
    } else if (settingsSection_ == 1) {
        p.text(L"Java", Rect(panel.x + 28, panel.y + 22, panel.w - 56, 26), P.text, Font::H3);
        p.text(L"Java détecté", Rect(panel.x + 28, panel.y + 72, 200, 18), P.text, Font::BodyStrong);
        if (javaLoading_) p.text(L"Recherche en cours...", Rect(panel.x + 28, panel.y + 96, 200, 16), P.textMuted, Font::Small);
        else if (javaList_.empty()) p.text(L"Aucune installation Java détectée", Rect(panel.x + 28, panel.y + 96, 300, 16), P.textMuted, Font::Small);
        else {
            for (size_t i = 0; i < javaList_.size() && i < 4; i++) {
                Rect jr(panel.x + 28, panel.y + 96 + (float)i*44, panel.w - 56, 36);
                p.fill(jr, P.inset, 10);
                p.stroke(jr, P.borderSoft, 1, 10);
                drawIcon(p, Icon::Java, Rect(jr.x + 12, jr.cy()-9, 18, 18), P.warn);
                p.text(L"Java " + std::to_wstring(javaList_[i].major) + L" - " + javaList_[i].path, Rect(jr.x + 38, jr.y, jr.w - 60, jr.h), P.textDim, Font::Small, Align::Left, VAlign::Middle, false, true);
            }
        }
        if (button("javarefresh", L"Actualiser", Rect(panel.x + 28, panel.y + 280, 140, 38), ButtonKind::Secondary)) detectJava();
        if (button("javasave", L"Enregistrer", Rect(panel.x + 180, panel.y + 280, 140, 38), ButtonKind::Primary)) { saveConfigNow(); notify(L"Paramètres enregistrés", false); }
    } else if (settingsSection_ == 2) {
        p.text(L"Comptes", Rect(panel.x + 28, panel.y + 22, panel.w - 56, 26), P.text, Font::H3);
        p.text(L"Comptes ajoutés", Rect(panel.x + 28, panel.y + 72, 200, 18), P.text, Font::BodyStrong);
        float cy = panel.y + 96;
        for (size_t i = 0; i < config_.accounts.size(); i++) {
            Rect ar(panel.x + 28, cy, panel.w - 56, 52);
            bool act = (int)i == config_.accountIndex;
            p.fill(ar, act ? P.cardHover : P.inset, 12);
            p.stroke(ar, P.borderSoft, 1, 12);
            drawAvatar(p, config_.accounts[i].name, ar.x + 22, ar.cy(), 16);
            p.text(config_.accounts[i].name, Rect(ar.x + 48, ar.y + 10, ar.w - 120, 16), P.text, Font::BodyStrong);
            p.text(config_.accounts[i].isMicrosoft() ? L"Microsoft" : L"Hors ligne", Rect(ar.x + 48, ar.y + 30, ar.w - 120, 14), P.textMuted, Font::Small);
            if (button(("acct_"+std::to_string(i)).c_str(), act ? L"Actif" : L"Utiliser", Rect(ar.right()-170, ar.cy()-16, 80, 32), act ? ButtonKind::Success : ButtonKind::Ghost)) { config_.accountIndex = (int)i; saveConfigNow(); }
            if (!act && button(("acctrm_"+std::to_string(i)).c_str(), L"Supprimer", Rect(ar.right()-82, ar.cy()-16, 76, 32), ButtonKind::Ghost)) removeAccount((int)i);
            cy += 60;
        }
        if (button("addoff", L"Ajouter compte hors ligne", Rect(panel.x + 28, cy + 10, 200, 38), ButtonKind::Secondary, true, Icon::Plus)) openDialog(Dialog::Accounts);
        if (button("addms", L"Se connecter avec Microsoft", Rect(panel.x + 240, cy + 10, 220, 38), ButtonKind::Secondary, true, Icon::Microsoft)) openDialog(Dialog::Microsoft);
        p.text(L"Client ID Azure (public client)", Rect(panel.x + 28, panel.y + 360, 260, 18), P.text, Font::BodyStrong);
        textField("msid", msClientDraft_, Rect(panel.x + 28, panel.y + 384, panel.w - 56, 38), L"xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx");
        if (button("savesid", L"Enregistrer Client ID", Rect(panel.x + 28, panel.y + 434, 190, 38), ButtonKind::Primary)) { config_.msClientId = msClientDraft_; saveConfigNow(); notify(L"Client ID enregistré", false); }
    } else if (settingsSection_ == 3) {
        p.text(L"Réseau", Rect(panel.x + 28, panel.y + 22, panel.w - 56, 26), P.text, Font::H3);
        p.text(L"Utilisation de WinHTTP pour les téléchargements et API.", Rect(panel.x + 28, panel.y + 72, panel.w - 56, 18), P.textDim, Font::Body);
        p.text(L"User-Agent : AuroraLauncher/" + std::wstring(AURORA_VERSION_W), Rect(panel.x + 28, panel.y + 96, panel.w - 56, 18), P.textDim, Font::Body);
    } else if (settingsSection_ == 4) {
        p.text(L"À propos", Rect(panel.x + 28, panel.y + 22, panel.w - 56, 26), P.text, Font::H3);
        drawIcon(p, Icon::Cube, Rect(panel.x + 28, panel.y + 72, 42, 42), P.accent);
        p.text(L"Aurora Launcher", Rect(panel.x + 82, panel.y + 74, 200, 24), P.text, Font::H2);
        p.text(L"Version " + std::wstring(AURORA_VERSION_W), Rect(panel.x + 82, panel.y + 102, 200, 18), P.textDim, Font::Body);
        p.text(L"Lanceur Minecraft natif en C++ pour Windows", Rect(panel.x + 28, panel.y + 140, panel.w - 56, 18), P.textDim, Font::Body);
        p.text(L"Fabric/Quilt • Microsoft OAuth • Modrinth • WinHTTP • GDI+", Rect(panel.x + 28, panel.y + 164, panel.w - 56, 18), P.textMuted, Font::Small);
        if (button("aboutsite", L"Documentation", Rect(panel.x + 28, panel.y + 210, 150, 38), ButtonKind::Secondary)) openUrl(L"https://github.com/AuroraLauncher/AuroraLauncher");
    }
}
