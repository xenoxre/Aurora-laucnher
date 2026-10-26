#include "app.h"
#include "ui/ui.h"
#include "core/log.h"

using namespace aurora;
using namespace aurora::ui;

void App::pageConsole(Painter& p, const Rect& area) {
    drawPageHeader(p, L"Console", L"Journal du lanceur et sortie du jeu", area);
    float y = 72;
    Rect bar(area.x + 24, area.y + y, area.w - 48, 48);
    p.fill(bar, P.card, 12);
    p.stroke(bar, P.borderSoft, 1, 12);
    const wchar_t* fl[] = { L"Tout", L"Info", L"Avertissement", L"Erreur", L"Jeu" };
    std::vector<std::wstring> f(fl, fl+5);
    segmented("logf", logFilter_, f, Rect(bar.x + 12, bar.y + 7, 320, 34));
    if (toggle("autoscroll", autoScroll_, Rect(bar.right()-172, bar.y + 12, 44, 24))) { /* toggle deja applique */ }
    p.text(L"Défilement auto", Rect(bar.right()-300, bar.y, 120, 48), P.textDim, Font::Small, Align::Right);
    if (iconButton("logclear", Icon::Trash, Rect(bar.right()-48, bar.y + 10, 28, 28), L"Effacer affichage")) { Logger::instance().clearUi(); logCache_.clear(); lastLogCount_ = 0; }
    if (iconButton("logrefresh", Icon::Refresh, Rect(bar.right()-84, bar.y + 10, 28, 28), L"Rafraîchir")) refreshLogs(true);

    Rect list(area.x + 24, area.y + y + 62, area.w - 48, area.h - y - 82);
    p.fill(list, P.inset, 14);
    p.stroke(list, P.borderSoft, 1, 14);
    refreshLogs(false);
    std::vector<const LogEntry*> items;
    for (auto& e : logCache_) {
        if (logFilter_ == 1 && e.level != LogLevel::Info && e.level != LogLevel::Success) continue;
        if (logFilter_ == 2 && e.level != LogLevel::Warn) continue;
        if (logFilter_ == 3 && e.level != LogLevel::Error) continue;
        if (logFilter_ == 4 && e.level != LogLevel::Game) continue;
        items.push_back(&e);
    }
    float content = (float)items.size() * 22.0f + 20;
    float off = beginScroll("logscroll", Rect(list.x + 10, list.y + 10, list.w - 20, list.h - 20), content);
    float ly = list.y + 10 - off;
    if (autoScroll_ && !items.empty()) scrollTo("logscroll", std::max(0.0f, content - (list.h - 20)));
    for (auto* e : items) {
        Rect r(list.x + 10, ly, list.w - 20, 20);
        if (r.bottom() < list.y + 10 || r.y > list.bottom()-10) { ly += 22; continue; }
        Gdiplus::Color c = P.textDim;
        if (e->level == LogLevel::Success) c = P.success;
        else if (e->level == LogLevel::Warn) c = P.warn;
        else if (e->level == LogLevel::Error) c = P.danger;
        else if (e->level == LogLevel::Game) c = P.accent;
        else if (e->level == LogLevel::Debug) c = P.textMuted;
        p.text(e->time, Rect(r.x, r.y, 90, r.h), P.textMuted, Font::MonoSmall);
        p.text(L"[" + std::wstring(logLevelName(e->level)) + L"]", Rect(r.x + 94, r.y, 70, r.h), c, Font::MonoSmall);
        p.text(e->source, Rect(r.x + 168, r.y, 90, r.h), P.textMuted, Font::MonoSmall);
        p.text(e->text, Rect(r.x + 262, r.y, r.w - 272, r.h), c, Font::MonoSmall, Align::Left, VAlign::Middle, false, true);
        ly += 22;
    }
    endScroll();
}
