#include "app.h"
#include "ui/ui.h"
#include "core/util.h"
#include "core/meta.h"

using namespace aurora;
using namespace aurora::ui;

void App::pageVersions(Painter& p, const Rect& area) {
    drawPageHeader(p, L"Versions", L"Installer et gérer les versions de Minecraft", area);
    float y = 72;
    Rect bar(area.x + 24, area.y + y, area.w - 48, 48);
    p.fill(bar, P.card, 12);
    p.stroke(bar, P.borderSoft, 1, 12);
    static std::string qid = "verq";
    searchField(qid, versionQuery_, Rect(bar.x + 12, bar.y + 7, 320, 34), L"Rechercher une version...");
    const wchar_t* filters[] = { L"Tous", L"Release", L"Snapshot", L"Anciennes" };
    std::vector<std::wstring> fl(filters, filters+4);
    segmented("verf", versionFilter_, fl, Rect(bar.right()-416, bar.y + 7, 348, 34));
    if (iconButton("verrefresh", Icon::Refresh, Rect(bar.right()-52, bar.y + 10, 32, 28), L"Actualiser")) refreshManifest(true);

    Rect list(area.x + 24, area.y + y + 62, area.w - 48, area.h - y - 82);
    p.fill(list, P.card, 14);
    p.stroke(list, P.borderSoft, 1, 14);
    p.text(L"Versions disponibles", Rect(list.x + 24, list.y + 18, list.w - 48, 22), P.text, Font::H3);

    float content = 0;
    float lh = 54;
    std::vector<const VersionEntry*> items;
    for (auto& v : manifest_.versions) {
        bool old = v.type == L"old_beta" || v.type == L"old_alpha";
        if (versionFilter_ == 1 && v.type != L"release") continue;
        if (versionFilter_ == 2 && v.type != L"snapshot") continue;
        if (versionFilter_ == 3 && !old) continue;
        if (versionFilter_ == 0 && old) continue;
        if (!versionQuery_.empty() && containsNoCase(v.id, versionQuery_) == false) continue;
        items.push_back(&v);
        content += lh;
    }
    if (items.empty()) {
        drawEmptyState(p, Rect(list.x, list.y+60, list.w, list.h-120), Icon::Search, L"Aucun résultat",
                       manifestLoaded_ ? L"Aucune version ne correspond à votre recherche" : L"Chargement du manifeste...");
    } else {
        float off = beginScroll("verscroll", Rect(list.x + 12, list.y + 52, list.w - 24, list.h - 70), content);
        float ly = list.y + 52 - off;
        for (auto* ve : items) {
            Rect r(list.x + 12, ly, list.w - 24, 46);
            if (r.bottom() < list.y + 52 || r.y > list.bottom()-10) { ly += lh; continue; }
            bool inst = isVersionInstalled(paths_, ve->id) || instanceForVersion(ve->id);
            bool busy = busyVersions_.count(ve->id);
            p.fill(r, P.inset, 10);
            p.stroke(r, P.borderSoft, 1, 10);
            Icon ic = ve->type == L"release" ? Icon::Cube : Icon::Sparkle;
            drawIcon(p, ic, Rect(r.x + 14, r.cy()-9, 18, 18), ve->type == L"release" ? P.accent : P.purple);
            p.text(ve->id, Rect(r.x + 42, r.y + 8, 200, 18), P.text, Font::BodyStrong);
            p.text(formatDateFr(ve->releaseTime), Rect(r.x + 42, r.y + 26, 200, 14), P.textMuted, Font::Small);
            if (inst) {
                drawBadge(p, L"Installé", r.right()-240, r.cy()-9, P.success, rgba(70,209,124,26));
                if (button(("playv_"+wideToUtf8(ve->id)).c_str(), L"Jouer", Rect(r.right()-170, r.cy()-16, 78, 32), ButtonKind::Primary, !busy, Icon::Play)) playVersion(ve->id);
                if (button(("delv_"+wideToUtf8(ve->id)).c_str(), L"Supprimer", Rect(r.right()-84, r.cy()-16, 78, 32), ButtonKind::Ghost)) uninstallVersion(ve->id);
            } else {
                if (button(("instv_"+wideToUtf8(ve->id)).c_str(), busy ? L"Installation..." : L"Installer", Rect(r.right()-170, r.cy()-16, 164, 32), ButtonKind::Secondary, !busy, Icon::Download)) installVersion(ve->id);
            }
            ly += lh;
        }
        endScroll();
    }
}
