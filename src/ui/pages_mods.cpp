#include "app.h"
#include "ui/ui.h"
#include "core/util.h"

using namespace aurora;
using namespace aurora::ui;

void App::pageMods(Painter& p, const Rect& area) {
    drawPageHeader(p, L"Mods", L"Parcourir et installer des mods via Modrinth", area);
    float y = 72;
    Rect bar(area.x + 24, area.y + y, area.w - 48, 48);
    p.fill(bar, P.card, 12);
    p.stroke(bar, P.borderSoft, 1, 12);
    static std::string mq = "modq";
    bool submitted = false;
    searchField(mq, modsQuery_, Rect(bar.x + 12, bar.y + 7, 340, 34), L"Rechercher des mods...", &submitted);
    if (submitted) searchMods(true);
    if (button("modsearch", L"Rechercher", Rect(bar.x + 362, bar.y + 7, 110, 34), ButtonKind::Secondary)) searchMods(true);

    // instance selector
    std::vector<std::wstring> instNames;
    int sel = 0;
    for (size_t i = 0; i < instances_.size(); i++) {
        instNames.push_back(instances_[i].name + L" (" + instances_[i].mcVersion + L")");
        if (instances_[i].id == modsInstanceId_) sel = (int)i;
    }
    if (!instNames.empty()) {
        if (dropdown("modinst", sel, instNames, Rect(bar.right()-280, bar.y + 7, 268, 34))) {
            if (sel >=0 && sel < (int)instances_.size()) modsInstanceId_ = instances_[sel].id;
        }
    }

    Rect list(area.x + 24, area.y + y + 62, area.w - 48, area.h - y - 82);
    p.fill(list, P.card, 14);
    p.stroke(list, P.borderSoft, 1, 14);
    p.text(L"Résultats Modrinth", Rect(list.x + 24, list.y + 18, list.w - 48, 22), P.text, Font::H3);

    if (modsInstanceId_.empty()) {
        drawEmptyState(p, Rect(list.x, list.y+60, list.w, list.h-120), Icon::Puzzle, L"Aucune instance sélectionnée",
                       L"Sélectionnez une instance Fabric/Quilt pour installer des mods");
    } else if (!modsSearched_ && modHits_.empty()) {
        drawEmptyState(p, Rect(list.x, list.y+60, list.w, list.h-120), Icon::Search, L"Recherchez des mods",
                       L"Entrez un nom de mod et appuyez sur Rechercher");
    } else if (modsLoading_ && modHits_.empty()) {
        drawEmptyState(p, Rect(list.x, list.y+60, list.w, list.h-120), Icon::Refresh, L"Recherche en cours...",
                       L"Contact de Modrinth...");
    } else if (!modsError_.empty() && modHits_.empty()) {
        drawEmptyState(p, Rect(list.x, list.y+60, list.w, list.h-120), Icon::Alert, L"Erreur",
                       modsError_);
    } else if (modHits_.empty()) {
        drawEmptyState(p, Rect(list.x, list.y+60, list.w, list.h-120), Icon::Search, L"Aucun résultat",
                       L"Essayez un autre mot-clé");
    } else {
        float content = (float)modHits_.size() * 76.0f + 20;
        float off = beginScroll("modscroll", Rect(list.x + 12, list.y + 52, list.w - 24, list.h - 70), content);
        float ly = list.y + 52 - off;
        for (auto& hit : modHits_) {
            Rect r(list.x + 12, ly, list.w - 24, 68);
            if (r.bottom() < list.y + 52 || r.y > list.bottom()-10) { ly += 76; continue; }
            p.fill(r, P.inset, 12);
            p.stroke(r, P.borderSoft, 1, 12);
            drawIcon(p, Icon::Puzzle, Rect(r.x + 16, r.cy()-12, 24, 24), P.purple);
            p.text(hit.title, Rect(r.x + 52, r.y + 12, r.w - 200, 18), P.text, Font::BodyStrong);
            p.text(ellipsize(p, hit.description, Font::Small, r.w - 220), Rect(r.x + 52, r.y + 34, r.w - 200, 16), P.textDim, Font::Small);
            p.text(hit.author.empty() ? L"" : L"par " + hit.author, Rect(r.x + 52, r.y + 52, r.w - 200, 14), P.textMuted, Font::Tiny);
            bool installing = installingMod_ == hit.title;
            if (button(("modinst_"+wideToUtf8(hit.slug.empty()?hit.id:hit.slug)).c_str(), installing ? L"Installation..." : L"Installer", Rect(r.right()-130, r.cy()-17, 118, 34), ButtonKind::Secondary, !installing, Icon::Download)) installMod(hit);
            ly += 76;
        }
        endScroll();
        if (button("modmore", L"Charger plus", Rect(list.cx()-90, list.bottom()-56, 180, 36), ButtonKind::Ghost)) { modsOffset_ = (int)modHits_.size(); searchMods(false); }
    }
}
