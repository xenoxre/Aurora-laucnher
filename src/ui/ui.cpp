// Aurora Launcher - custom drawn UI toolkit implementation.
#include "ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace Gdiplus;

namespace aurora::ui {

Palette P;
Input in;

namespace {

struct FontEntry {
    Gdiplus::Font* font = nullptr;
};

std::map<Font, Gdiplus::Font*> g_fonts;
FontFamily* g_uiFamily = nullptr;
FontFamily* g_monoFamily = nullptr;

FontFamily* uiFamily() {
    if (!g_uiFamily) {
        g_uiFamily = new FontFamily(L"Segoe UI");
        if (g_uiFamily->GetLastStatus() != Ok) {
            delete g_uiFamily;
            g_uiFamily = new FontFamily(L"Tahoma");
        }
    }
    return g_uiFamily;
}
FontFamily* monoFamily() {
    if (!g_monoFamily) {
        g_monoFamily = new FontFamily(L"Consolas");
        if (g_monoFamily->GetLastStatus() != Ok) {
            delete g_monoFamily;
            g_monoFamily = new FontFamily(L"Courier New");
        }
    }
    return g_monoFamily;
}

Gdiplus::Font* fontFor(Font f) {
    auto it = g_fonts.find(f);
    if (it != g_fonts.end()) return it->second;
    float size = 13.5f;
    int style = FontStyleRegular;
    FontFamily* family = uiFamily();
    switch (f) {
        case Font::Display: size = 32; style = FontStyleBold; break;
        case Font::H1: size = 25; style = FontStyleBold; break;
        case Font::H2: size = 18.5f; style = FontStyleBold; break;
        case Font::H3: size = 15; style = FontStyleBold; break;
        case Font::Body: size = 13.5f; break;
        case Font::BodyStrong: size = 13.5f; style = FontStyleBold; break;
        case Font::Small: size = 11.5f; break;
        case Font::SmallStrong: size = 11.5f; style = FontStyleBold; break;
        case Font::Tiny: size = 10; style = FontStyleRegular; break;
        case Font::Button: size = 13; style = FontStyleBold; break;
        case Font::Mono: size = 11.5f; family = monoFamily(); break;
        case Font::MonoSmall: size = 10.2f; family = monoFamily(); break;
        case Font::Numeral: size = 30; style = FontStyleRegular; break;
    }
    auto* font = new Gdiplus::Font(family, size, style, UnitPixel);
    g_fonts[f] = font;
    return font;
}

struct EditState {
    bool focused = false;
    int cursor = 0;
    int anchor = 0;
    float scrollX = 0;
    bool dragging = false;
    double blinkStart = 0;
};

struct ScrollState {
    float offset = 0;
    float target = 0;
    float content = 0;
    float viewH = 0;
    Rect view;
    bool dragging = false;
    float dragStartY = 0;
    float dragStartOffset = 0;
    float thumbY = 0;
    float thumbH = 0;
};

struct FrameState {
    float w = 0, h = 0, scale = 1;
    double now = 0;
    bool hand = false;
    bool beam = false;
    bool blockClicks = false;
    bool fieldClicked = false;
    bool popupOpen = false;
    std::string popupId;
    Rect popupTrigger;
    Rect popupRect;
    std::vector<std::wstring> popupItems;
    int popupIndex = -1;
    int popupHover = -1;
    std::wstring tooltipText;
    Gdiplus::PointF tooltipPos;
    bool textFieldSeen = false;
    std::string tooltipId;
} F;

std::map<std::string, float> g_hoverAnim;
std::map<std::string, EditState> g_edits;
std::map<std::string, ScrollState> g_scrolls;
std::string g_dragSlider;
std::string g_dragScroll;
std::string g_enterField;
Painter* g_activePainter = nullptr;
bool g_hadPress = false;


LARGE_INTEGER g_freq{};
LARGE_INTEGER g_start{};

double clockSeconds() {
    if (!g_freq.QuadPart) {
        QueryPerformanceFrequency(&g_freq);
        QueryPerformanceCounter(&g_start);
        return 0;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - g_start.QuadPart) / (double)g_freq.QuadPart;
}

static float animateLocal(const std::string& id, bool active, float speed) {
    float& v = g_hoverAnim[id];
    v += ((active ? 1.0f : 0.0f) - v) * speed;
    if (v < 0.001f) v = 0;
    if (v > 0.999f) v = 1;
    return v;
}

namespace aurora { namespace ui {
float animate(const std::string& id, bool active, float speed) {
    float& v = g_hoverAnim[id];
    v += ((active ? 1.0f : 0.0f) - v) * speed;
    if (v < 0.001f) v = 0;
    if (v > 0.999f) v = 1;
    return v;
}
}} // namespace aurora::ui

extern "C" float _ZN6aurora2ui7animateERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEbf(const std::string& id, bool active, float speed) {
    return aurora::ui::animate(id, active, speed);
}

void buildRoundRect(const Rect& r, float radius, Gdiplus::GraphicsPath& path) {
    path.Reset();
    float d = std::min(radius * 2, std::min(r.w, r.h));
    if (d <= 0.5f) {
        path.AddRectangle(RectF(r.x, r.y, r.w, r.h));
        return;
    }
    path.AddArc(r.x, r.y, d, d, 180, 90);
    path.AddArc(r.right() - d, r.y, d, d, 270, 90);
    path.AddArc(r.right() - d, r.bottom() - d, d, d, 0, 90);
    path.AddArc(r.x, r.bottom() - d, d, d, 90, 90);
    path.CloseFigure();
}

void fillPath(Graphics& g, GraphicsPath& path, const Gdiplus::Color& c) {
    SolidBrush b(c);
    g.FillPath(&b, &path);
}
void strokePath(Graphics& g, GraphicsPath& path, const Gdiplus::Color& c, float width) {
    Pen pen(c, width);
    pen.SetLineJoin(LineJoinRound);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    g.DrawPath(&pen, &path);
}

std::wstring bulletize(const std::wstring& s) {
    std::wstring out;
    for (size_t i = 0; i < s.size(); i++) out.push_back(L'\u2022');
    return out;
}

float measureText(Graphics& g, const std::wstring& text, Gdiplus::Font* font) {
    if (text.empty()) return 0;
    StringFormat fmt(StringFormat::GenericTypographic());
    fmt.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsMeasureTrailingSpaces |
                       StringFormatFlagsNoClip);
    RectF layout(0, 0, 20000, 400);
    RectF bounds;
    g.MeasureString(text.c_str(), (INT)text.size(), font, layout, &fmt, &bounds);
    return bounds.Width;
}

} // namespace

// ------------------------------------------------------------------ frame
float width() { return F.w; }
float height() { return F.h; }
double time() { return F.now; }
bool Input::key(int vk) const { return std::find(keys.begin(), keys.end(), vk) != keys.end(); }
void setCursorHand(bool hand) { if (hand) F.hand = true; }
void setCursorIBeam(bool beam) { if (beam) F.beam = true; }
bool wantsHandCursor() { return F.hand; }
bool wantsBeamCursor() { return F.beam; }
void setClickBlocked(bool blocked) { F.blockClicks = blocked; }
bool clickBlocked() { return F.blockClicks; }

void beginFrame(float w, float h, float dpiScale) {
    F.w = w;
    F.h = h;
    F.scale = dpiScale;
    F.now = clockSeconds();
    F.hand = false;
    F.beam = false;
    F.blockClicks = false;
    F.fieldClicked = false;
    F.popupHover = -1;
    F.tooltipText.clear();
    g_enterField.clear();
    if (in.pressed) g_hadPress = true;
}

void endFrame(Painter& p) {
    // Popup layer ------------------------------------------------------
    if (F.popupOpen) {
        Gdiplus::Graphics* g = p.g();
        Rect pr = F.popupRect;
        p.shadow(pr, 10, 120, 14);
        p.fill(pr, rgba(18, 24, 36, 252), 10);
        p.stroke(pr, P.border, 1, 10);
        float itemH = 32;
        int hover = -1;
        if (pr.contains(in.mouse)) hover = (int)((in.mouse.Y - pr.y - 4) / itemH);
        F.popupHover = hover;
        for (size_t i = 0; i < F.popupItems.size(); i++) {
            Rect item(pr.x + 4, pr.y + 4 + (float)i * itemH, pr.w - 8, itemH);
            if ((int)i == hover) p.fill(item, P.cardHover, 7);
            if ((int)i == F.popupIndex) {
                drawIcon(p, Icon::Check, Rect(item.x + 10, item.cy() - 7, 14, 14), P.accent, 1.8f);
            }
            p.text(F.popupItems[i], Rect(item.x + 32, item.y, item.w - 42, item.h),
                   (int)i == F.popupIndex ? P.accent : P.text, Font::Body, Align::Left, VAlign::Middle,
                   false, true);
        }
        (void)g;
    }

    // Tooltip ----------------------------------------------------------
    if (!F.tooltipText.empty()) {
        float tw = p.measure(F.tooltipText, Font::Small) + 20;
        float th = 28;
        float tx = in.mouse.X + 14, ty = in.mouse.Y + 18;
        if (tx + tw > F.w - 8) tx = in.mouse.X - tw - 12;
        if (ty + th > F.h - 8) ty = in.mouse.Y - th - 12;
        Rect tr(tx, ty, tw, th);
        p.fill(tr, rgba(30, 38, 53, 250), 7);
        p.stroke(tr, rgba(64, 79, 104), 1, 7);
        p.text(F.tooltipText, tr.inset(10, 0), P.text, Font::Small, Align::Left, VAlign::Middle,
               false, true);
    }

    // Clear text field focus when clicking elsewhere.
    if (g_hadPress && !F.fieldClicked && !F.popupOpen) {
        for (auto& kv : g_edits) kv.second.focused = false;
    }
    if (in.escape) {
        for (auto& kv : g_edits) kv.second.focused = false;
        F.popupOpen = false;
    }

    // Consume frame input.
    in.pressed = false;
    in.released = false;
    in.rightPressed = false;
    in.doubleClick = false;
    in.wheel = 0;
    in.keys.clear();
    in.chars.clear();
    in.escape = false;
    g_hadPress = false;
}

void focusNextField() {
    std::vector<std::string> ids;
    for (auto& kv : g_edits) if (kv.second.focused) ids.push_back(kv.first);
    if (ids.empty()) return;
    bool next = false;
    std::string first;
    for (auto& kv : g_edits) {
        if (first.empty()) first = kv.first;
        if (next) {
            for (auto& e : g_edits) e.second.focused = false;
            kv.second.focused = true;
            return;
        }
        if (kv.second.focused) next = true;
    }
    for (auto& e : g_edits) e.second.focused = false;
    if (!first.empty()) g_edits[first].focused = true;
}

bool anyFieldFocused() {
    for (auto& kv : g_edits) if (kv.second.focused) return true;
    return false;
}

bool animating() {
    for (auto& kv : g_hoverAnim) {
        if (kv.second > 0.01f && kv.second < 0.99f) return true;
    }
    for (auto& kv : g_scrolls) {
        if (std::fabs(kv.second.offset - kv.second.target) > 0.4f) return true;
    }
    return anyFieldFocused();
}

// ------------------------------------------------------------------ painter
void Painter::begin(Graphics* g) {
    g_ = g;
    g_activePainter = this;
    states_.clear();
    clip_ = Rect(0, 0, 99999, 99999);
    g_->SetSmoothingMode(SmoothingModeAntiAlias);
    g_->SetPixelOffsetMode(PixelOffsetModeHalf);
    g_->SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g_->SetInterpolationMode(InterpolationModeHighQualityBicubic);
}

void Painter::fill(const Rect& r, const Gdiplus::Color& c) { fill(r, c, 0); }

void Painter::fill(const Rect& r, const Gdiplus::Color& c, float radius) {
    if (!g_ || r.w <= 0 || r.h <= 0 || c.GetA() == 0) return;
    SolidBrush b(c);
    if (radius <= 0.5f) {
        g_->FillRectangle(&b, r.x, r.y, r.w, r.h);
    } else {
        GraphicsPath path;
        buildRoundRect(r, radius, path);
        g_->FillPath(&b, &path);
    }
}

void Painter::fillGradient(const Rect& r, const Gdiplus::Color& a, const Gdiplus::Color& b,
                           bool vertical, float radius) {
    if (!g_ || r.w <= 0 || r.h <= 0) return;
    LinearGradientBrush brush(RectF(r.x, r.y, r.w, r.h), a, b,
                              vertical ? LinearGradientModeVertical : LinearGradientModeHorizontal);
    if (radius <= 0.5f) {
        g_->FillRectangle(&brush, r.x, r.y, r.w, r.h);
    } else {
        GraphicsPath path;
        buildRoundRect(r, radius, path);
        g_->FillPath(&brush, &path);
    }
}

void Painter::stroke(const Rect& r, const Gdiplus::Color& c, float w, float radius) {
    if (!g_ || r.w <= 0 || r.h <= 0) return;
    Pen pen(c, w);
    pen.SetAlignment(PenAlignmentInset);
    if (radius <= 0.5f) {
        g_->DrawRectangle(&pen, r.x, r.y, r.w, r.h);
    } else {
        GraphicsPath path;
        buildRoundRect(r, radius, path);
        g_->DrawPath(&pen, &path);
    }
}

void Painter::line(float x1, float y1, float x2, float y2, const Gdiplus::Color& c, float w) {
    if (!g_) return;
    Pen pen(c, w);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    g_->DrawLine(&pen, x1, y1, x2, y2);
}

void Painter::circle(float cx, float cy, float radius, const Gdiplus::Color& c) {
    if (!g_ || radius <= 0) return;
    SolidBrush b(c);
    g_->FillEllipse(&b, cx - radius, cy - radius, radius * 2, radius * 2);
}

void Painter::circleStroke(float cx, float cy, float radius, const Gdiplus::Color& c, float w) {
    if (!g_ || radius <= 0) return;
    Pen pen(c, w);
    g_->DrawEllipse(&pen, cx - radius, cy - radius, radius * 2, radius * 2);
}

void Painter::shadow(const Rect& r, float radius, int alpha, float spread) {
    if (!g_) return;
    for (int i = 5; i >= 1; i--) {
        float grow = spread * (float)i / 5.0f;
        int a = (int)(alpha * (1.0f - (float)i / 6.0f));
        Rect rr(r.x - grow, r.y - grow / 2 + 3, r.w + grow * 2, r.h + grow);
        fill(rr, rgba(0, 0, 0, a), radius + grow);
    }
}

void Painter::glow(float cx, float cy, float radius, const Gdiplus::Color& c) {
    if (!g_ || radius <= 0) return;
    GraphicsPath path;
    path.AddEllipse(cx - radius, cy - radius, radius * 2, radius * 2);
    PathGradientBrush brush(&path);
    brush.SetCenterColor(c);
    Color surround(c.GetR(), c.GetG(), c.GetB(), 0);
    int count = 1;
    brush.SetSurroundColors(&surround, &count);
    g_->FillPath(&brush, &path);
}

void Painter::setupText(StringFormat& fmt, Align a, VAlign v, bool wrap, bool ellipsis) const {
    fmt.SetAlignment(a == Align::Left ? StringAlignmentNear
                                      : (a == Align::Center ? StringAlignmentCenter : StringAlignmentFar));
    fmt.SetLineAlignment(v == VAlign::Top ? StringAlignmentNear
                                          : (v == VAlign::Middle ? StringAlignmentCenter : StringAlignmentFar));
    int flags = StringFormatFlagsNoClip;
    if (!wrap) flags |= StringFormatFlagsNoWrap;
    fmt.SetFormatFlags(flags);
    if (ellipsis && !wrap) fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    else if (wrap) fmt.SetTrimming(StringTrimmingEllipsisWord);
}

void Painter::text(const std::wstring& s, const Rect& r, const Gdiplus::Color& c, Font f,
                   Align align, VAlign valign, bool wrap, bool ellipsis) {
    if (!g_ || s.empty() || r.w <= 0) return;
    Gdiplus::Font* font = fontFor(f);
    StringFormat fmt(StringFormat::GenericTypographic());
    setupText(fmt, align, valign, wrap, ellipsis);
    SolidBrush brush(c);
    g_->DrawString(s.c_str(), (INT)s.size(), font, r.g(), &fmt, &brush);
}

void Painter::textPoint(const std::wstring& s, float x, float y, const Gdiplus::Color& c, Font f) {
    if (!g_ || s.empty()) return;
    Gdiplus::Font* font = fontFor(f);
    SolidBrush brush(c);
    g_->DrawString(s.c_str(), (INT)s.size(), font, PointF(x, y), StringFormat::GenericTypographic(),
                   &brush);
}

float Painter::measure(const std::wstring& s, Font f) const {
    if (!g_ || s.empty()) return 0;
    return measureText(*g_, s, fontFor(f));
}

float Painter::lineHeight(Font f) const {
    Gdiplus::Font* font = fontFor(f);
    return font->GetHeight(g_);
}

void Painter::clip(const Rect& r) {
    if (!g_) return;
    states_.push_back(g_->Save());
    g_->SetClip(r.g(), CombineModeIntersect);
    clip_ = r;
}

void Painter::restore() {
    if (!g_ || states_.empty()) return;
    g_->Restore(states_.back());
    states_.pop_back();
    clip_ = Rect(0, 0, 99999, 99999);
}

void Painter::push() {
    if (!g_) return;
    states_.push_back(g_->Save());
}

void Painter::translate(float dx, float dy) {
    if (g_) g_->TranslateTransform(dx, dy);
}

// ------------------------------------------------------------------ icons
float iconSize() { return 18; }

void drawIcon(Painter& p, Icon icon, const Rect& r, const Gdiplus::Color& c, float stroke) {
    Graphics* g = p.g();
    if (!g || icon == Icon::None) return;
    auto X = [&](float v) { return r.x + v / 24.0f * r.w; };
    auto Y = [&](float v) { return r.y + v / 24.0f * r.h; };
    auto L = [&](float x1, float y1, float x2, float y2) {
        p.line(X(x1), Y(y1), X(x2), Y(y2), c, stroke);
    };
    auto joinPath = [&](std::initializer_list<PointF> pts, bool close, bool fillIt) {
        GraphicsPath path;
        bool first = true;
        PointF prev;
        for (auto& pt : pts) {
            if (first) { path.StartFigure(); prev = pt; first = false; }
            else { path.AddLine(prev, pt); prev = pt; }
        }
        (void)close;
        path.CloseFigure();
        if (fillIt) fillPath(*g, path, c);
        else strokePath(*g, path, c, stroke);
    };
    auto poly = [&](std::initializer_list<PointF> pts, bool close) {
        GraphicsPath path;
        bool first = true;
        PointF prev;
        for (auto& pt : pts) {
            if (first) { path.StartFigure(); prev = pt; first = false; }
            else { path.AddLine(prev, pt); prev = pt; }
        }
        if (close) path.CloseFigure();
        strokePath(*g, path, c, stroke);
    };
    auto rr = [&](float x, float y, float w, float h, float rad, bool fillIt) {
        Rect box(X(x), Y(y), w / 24.0f * r.w, h / 24.0f * r.h);
        if (fillIt) p.fill(box, c, rad / 24.0f * r.w);
        else p.stroke(box, c, stroke, rad / 24.0f * r.w);
    };
    auto fillCircle = [&](float cx, float cy, float rad) {
        p.circle(X(cx), Y(cy), rad / 24.0f * r.w, c);
    };
    auto circleOutline = [&](float cx, float cy, float rad) {
        p.circleStroke(X(cx), Y(cy), rad / 24.0f * r.w, c, stroke);
    };
    auto arrowHead = [&](float x, float y, float dir, float size) {
        // dir: 0 down, 1 up, 2 left, 3 right
        if (dir == 0) poly({ PointF(X(x - size), Y(y - size)), PointF(X(x), Y(y)), PointF(X(x + size), Y(y - size)) }, false);
        if (dir == 1) poly({ PointF(X(x - size), Y(y + size)), PointF(X(x), Y(y)), PointF(X(x + size), Y(y + size)) }, false);
        if (dir == 2) poly({ PointF(X(x + size), Y(y - size)), PointF(X(x), Y(y)), PointF(X(x + size), Y(y + size)) }, false);
        if (dir == 3) poly({ PointF(X(x - size), Y(y - size)), PointF(X(x), Y(y)), PointF(X(x - size), Y(y + size)) }, false);
    };

    switch (icon) {
        case Icon::Play:
            joinPath({ PointF(X(7), Y(4)), PointF(X(19.5f), Y(12)), PointF(X(7), Y(20)) }, true, true);
            break;
        case Icon::Stop:
            rr(6, 6, 12, 12, 2, true);
            break;
        case Icon::Pause:
            rr(6.5f, 5, 3.8f, 14, 1, true);
            rr(13.7f, 5, 3.8f, 14, 1, true);
            break;
        case Icon::Cube:
            poly({ PointF(X(12), Y(3)), PointF(X(20), Y(7.5f)), PointF(X(20), Y(16.5f)),
                   PointF(X(12), Y(21)), PointF(X(4), Y(16.5f)), PointF(X(4), Y(7.5f)) }, true);
            L(4, 7.5f, 12, 12); L(20, 7.5f, 12, 12); L(12, 12, 12, 21);
            break;
        case Icon::Puzzle:
            rr(4, 4, 7, 7, 2, false);
            rr(13, 4, 7, 7, 2, false);
            rr(4, 13, 7, 7, 2, false);
            rr(13, 13, 7, 7, 2, false);
            fillCircle(16.5f, 16.5f, 2);
            break;
        case Icon::Server:
            rr(3.5f, 4, 17, 4.6f, 1.5f, false);
            rr(3.5f, 9.7f, 17, 4.6f, 1.5f, false);
            rr(3.5f, 15.4f, 17, 4.6f, 1.5f, false);
            fillCircle(6.8f, 6.3f, 0.9f); fillCircle(6.8f, 12, 0.9f); fillCircle(6.8f, 17.7f, 0.9f);
            break;
        case Icon::Terminal:
            rr(3, 4.5f, 18, 15, 2, false);
            poly({ PointF(X(7.5f), Y(10)), PointF(X(10.5f), Y(12.2f)), PointF(X(7.5f), Y(14.4f)) }, false);
            L(13, 14.6f, 17, 14.6f);
            break;
        case Icon::Settings: {
            circleOutline(12, 12, 3.3f);
            for (int i = 0; i < 8; i++) {
                float a = (float)i * 3.14159265f / 4.0f;
                float cx = 12 + std::cos(a) * 6.6f, cy = 12 + std::sin(a) * 6.6f;
                float ex = 12 + std::cos(a) * 9.0f, ey = 12 + std::sin(a) * 9.0f;
                L(cx, cy, ex, ey);
            }
            break;
        }
        case Icon::User:
            circleOutline(12, 8, 3.6f);
            poly({ PointF(X(4.8f), Y(20)), PointF(X(6.2f), Y(15.6f)), PointF(X(9.5f), Y(13.6f)),
                   PointF(X(14.5f), Y(13.6f)), PointF(X(17.8f), Y(15.6f)), PointF(X(19.2f), Y(20)) }, false);
            break;
        case Icon::Search:
            circleOutline(10.5f, 10.5f, 5.6f);
            L(14.8f, 14.8f, 20, 20);
            break;
        case Icon::Plus:
            L(12, 5, 12, 19); L(5, 12, 19, 12);
            break;
        case Icon::Minus:
            L(5, 12, 19, 12);
            break;
        case Icon::Close:
            L(6, 6, 18, 18); L(18, 6, 6, 18);
            break;
        case Icon::Minimize:
            L(6, 12, 18, 12);
            break;
        case Icon::Maximize:
            rr(6, 6, 12, 12, 1.5f, false);
            break;
        case Icon::Restore:
            rr(6, 8, 10, 10, 1.5f, false);
            poly({ PointF(X(9), Y(6)), PointF(X(18), Y(6)), PointF(X(18), Y(15)) }, false);
            break;
        case Icon::ChevronDown:
            poly({ PointF(X(7), Y(9.5f)), PointF(X(12), Y(14.5f)), PointF(X(17), Y(9.5f)) }, false);
            break;
        case Icon::ChevronRight:
            poly({ PointF(X(9.5f), Y(7)), PointF(X(14.5f), Y(12)), PointF(X(9.5f), Y(17)) }, false);
            break;
        case Icon::ChevronLeft:
            poly({ PointF(X(14.5f), Y(7)), PointF(X(9.5f), Y(12)), PointF(X(14.5f), Y(17)) }, false);
            break;
        case Icon::Download:
            L(12, 3.5f, 12, 15); arrowHead(12, 15, 0, 3.6f);
            poly({ PointF(X(5), Y(17)), PointF(X(5), Y(20)), PointF(X(19), Y(20)), PointF(X(19), Y(17)) }, false);
            break;
        case Icon::Trash:
            L(4.5f, 6.8f, 19.5f, 6.8f);
            L(9, 4, 15, 4);
            poly({ PointF(X(6.5f), Y(7)), PointF(X(7.5f), Y(20)), PointF(X(16.5f), Y(20)), PointF(X(17.5f), Y(7)) }, false);
            L(10, 10, 10.4f, 17); L(14, 10, 13.6f, 17);
            break;
        case Icon::Folder:
            poly({ PointF(X(3.5f), Y(18.5f)), PointF(X(3.5f), Y(7)), PointF(X(6), Y(5)),
                   PointF(X(9.5f), Y(5)), PointF(X(11.5f), Y(8)), PointF(X(18.5f), Y(8)),
                   PointF(X(20.5f), Y(10)), PointF(X(20.5f), Y(18.5f)), PointF(X(19), Y(20)),
                   PointF(X(5), Y(20)) }, true);
            break;
        case Icon::Refresh: {
            GraphicsPath path;
            path.AddArc(X(4), Y(4), X(16) - X(4), Y(16) - Y(4), 40, 280);
            strokePath(*g, path, c, stroke);
            L(19.5f, 4, 19.5f, 9); L(19.5f, 9, 14.5f, 9);
            break;
        }
        case Icon::Check:
            poly({ PointF(X(4.5f), Y(12.5f)), PointF(X(9.5f), Y(17.5f)), PointF(X(19.5f), Y(7)) }, false);
            break;
        case Icon::Alert:
            poly({ PointF(X(12), Y(3.5f)), PointF(X(21.5f), Y(20)), PointF(X(2.5f), Y(20)) }, true);
            L(12, 9, 12, 14); fillCircle(12, 17, 1.05f);
            break;
        case Icon::Info:
            circleOutline(12, 12, 8.5f);
            fillCircle(12, 7.7f, 1.1f); L(12, 11, 12, 16.8f);
            break;
        case Icon::External:
            rr(4, 8, 12, 12, 2, false);
            poly({ PointF(X(11), Y(13)), PointF(X(20), Y(4)) }, false);
            L(14, 4, 20, 4); L(20, 4, 20, 10);
            break;
        case Icon::Microsoft:
            rr(3.5f, 3.5f, 7.2f, 7.2f, 0, true);
            rr(13.3f, 3.5f, 7.2f, 7.2f, 0, true);
            rr(3.5f, 13.3f, 7.2f, 7.2f, 0, true);
            rr(13.3f, 13.3f, 7.2f, 7.2f, 0, true);
            break;
        case Icon::Java:
            poly({ PointF(X(5.5f), Y(9)), PointF(X(6.5f), Y(17.5f)), PointF(X(8), Y(20)),
                   PointF(X(16), Y(20)), PointF(X(17.5f), Y(17.5f)), PointF(X(18.5f), Y(9)) }, false);
            L(18.5f, 10.5f, 21, 11.5f); L(21, 11.5f, 19.8f, 14.5f);
            L(9, 4, 8, 6.5f); L(12.5f, 3, 11.5f, 6.5f); L(16, 4, 15, 6.5f);
            break;
        case Icon::Copy:
            rr(8, 8, 12, 12, 2, false);
            poly({ PointF(X(16), Y(8)), PointF(X(16), Y(4.5f)), PointF(X(4.5f), Y(4.5f)),
                   PointF(X(4.5f), Y(16)), PointF(X(8), Y(16)) }, false);
            break;
        case Icon::LogOut:
            poly({ PointF(X(10), Y(4)), PointF(X(4.5f), Y(4)), PointF(X(4.5f), Y(20)),
                   PointF(X(10), Y(20)) }, false);
            L(9.5f, 12, 20, 12); arrowHead(20, 12, 3, 3.6f);
            break;
        case Icon::Link: {
            GraphicsPath a;
            a.AddArc(X(3), Y(8.5f), X(10) - X(3), Y(15.5f) - Y(8.5f), 90, 180);
            strokePath(*g, a, c, stroke);
            GraphicsPath b;
            b.AddArc(X(14), Y(8.5f), X(21) - X(14), Y(15.5f) - Y(8.5f), 270, 180);
            strokePath(*g, b, c, stroke);
            L(8.5f, 12, 15.5f, 12);
            break;
        }
        case Icon::Globe:
            circleOutline(12, 12, 8.6f);
            {
                GraphicsPath e;
                e.AddEllipse(X(8), Y(3.4f), X(16) - X(8), Y(20.6f) - Y(3.4f));
                strokePath(*g, e, c, stroke);
            }
            L(4, 12, 20, 12);
            break;
        case Icon::Shield:
            poly({ PointF(X(12), Y(3)), PointF(X(20), Y(6.2f)), PointF(X(19), Y(14)),
                   PointF(X(15.5f), Y(18.5f)), PointF(X(12), Y(21)), PointF(X(8.5f), Y(18.5f)),
                   PointF(X(5), Y(14)), PointF(X(4), Y(6.2f)) }, true);
            poly({ PointF(X(8.5f), Y(12)), PointF(X(11), Y(14.5f)), PointF(X(15.8f), Y(9.5f)) }, false);
            break;
        case Icon::Sparkle: {
            GraphicsPath s;
            s.StartFigure();
            s.AddLine(PointF(X(12), Y(3)), PointF(X(14.2f), Y(9.8f)));
            s.AddLine(PointF(X(14.2f), Y(9.8f)), PointF(X(21), Y(12)));
            s.AddLine(PointF(X(21), Y(12)), PointF(X(14.2f), Y(14.2f)));
            s.AddLine(PointF(X(14.2f), Y(14.2f)), PointF(X(12), Y(21)));
            s.AddLine(PointF(X(12), Y(21)), PointF(X(9.8f), Y(14.2f)));
            s.AddLine(PointF(X(9.8f), Y(14.2f)), PointF(X(3), Y(12)));
            s.AddLine(PointF(X(3), Y(12)), PointF(X(9.8f), Y(9.8f)));
            s.CloseFigure();
            fillPath(*g, s, c);
            break;
        }
        case Icon::Edit:
            poly({ PointF(X(4), Y(20)), PointF(X(5.2f), Y(15.3f)), PointF(X(16.8f), Y(3.7f)),
                   PointF(X(20.3f), Y(7.2f)), PointF(X(8.7f), Y(18.8f)) }, true);
            L(5.2f, 15.3f, 8.7f, 18.8f);
            break;
        case Icon::Power: {
            GraphicsPath path;
            path.AddArc(X(4), Y(4.5f), X(20) - X(4), Y(20.5f) - Y(4.5f), 120, 300);
            strokePath(*g, path, c, stroke);
            L(12, 3, 12, 11.5f);
            break;
        }
        case Icon::Clock:
            circleOutline(12, 12, 8.6f);
            L(12, 7, 12, 12.5f); L(12, 12.5f, 16, 14.5f);
            break;
        case Icon::Grid:
            rr(4, 4, 6.4f, 6.4f, 1.6f, false);
            rr(13.6f, 4, 6.4f, 6.4f, 1.6f, false);
            rr(4, 13.6f, 6.4f, 6.4f, 1.6f, false);
            rr(13.6f, 13.6f, 6.4f, 6.4f, 1.6f, false);
            break;
        case Icon::Star: {
            GraphicsPath s;
            PointF prev;
            for (int i = 0; i < 10; i++) {
                float ang = -3.14159265f / 2 + i * 3.14159265f / 5;
                float rad = (i % 2 == 0) ? 9.0f : 3.9f;
                float px = 12 + std::cos(ang) * rad, py = 12 + std::sin(ang) * rad;
                PointF pt(X(px), Y(py));
                if (i == 0) { s.StartFigure(); prev = pt; }
                else { s.AddLine(prev, pt); prev = pt; }
            }
            s.CloseFigure();
            fillPath(*g, s, c);
            break;
        }
        case Icon::More:
            fillCircle(6, 12, 1.4f); fillCircle(12, 12, 1.4f); fillCircle(18, 12, 1.4f);
            break;
        case Icon::Eye:
            poly({ PointF(X(2.5f), Y(12)), PointF(X(7), Y(6.5f)), PointF(X(12), Y(5)),
                   PointF(X(17), Y(6.5f)), PointF(X(21.5f), Y(12)), PointF(X(17), Y(17.5f)),
                   PointF(X(12), Y(19)), PointF(X(7), Y(17.5f)) }, true);
            circleOutline(12, 12, 3.2f);
            fillCircle(12, 12, 1.2f);
            break;
        case Icon::CheckCircle:
            circleOutline(12, 12, 8.8f);
            poly({ PointF(X(7.5f), Y(12.3f)), PointF(X(10.6f), Y(15.3f)), PointF(X(16.8f), Y(8.8f)) }, false);
            break;
        case Icon::XCircle:
            circleOutline(12, 12, 8.8f);
            L(8.5f, 8.5f, 15.5f, 15.5f); L(15.5f, 8.5f, 8.5f, 15.5f);
            break;
        case Icon::Wifi: {
            GraphicsPath a;
            a.AddArc(X(3.5f), Y(7), X(20.5f) - X(3.5f), Y(22) - Y(7), 200, 140);
            strokePath(*g, a, c, stroke);
            GraphicsPath b;
            b.AddArc(X(7), Y(11), X(17) - X(7), Y(22) - Y(11), 200, 140);
            strokePath(*g, b, c, stroke);
            fillCircle(12, 18.5f, 1.3f);
            break;
        }
        case Icon::Package:
            poly({ PointF(X(12), Y(3)), PointF(X(20), Y(7.5f)), PointF(X(20), Y(16.5f)),
                   PointF(X(12), Y(21)), PointF(X(4), Y(16.5f)), PointF(X(4), Y(7.5f)) }, true);
            L(4, 7.5f, 12, 12); L(20, 7.5f, 12, 12); L(12, 12, 12, 21);
            L(8, 5.2f, 16, 9.7f);
            break;
        case Icon::Sliders:
            L(4, 7, 20, 7); L(4, 12, 20, 12); L(4, 17, 20, 17);
            p.circle(X(9), Y(7), r.w / 24.0f * 2.2f, c);
            p.circle(X(15), Y(12), r.w / 24.0f * 2.2f, c);
            p.circle(X(8), Y(17), r.w / 24.0f * 2.2f, c);
            break;
        case Icon::Book:
            poly({ PointF(X(4), Y(5)), PointF(X(10), Y(5)), PointF(X(12), Y(7)),
                   PointF(X(14), Y(5)), PointF(X(20), Y(5)), PointF(X(20), Y(19)),
                   PointF(X(14), Y(19)), PointF(X(12), Y(17)), PointF(X(10), Y(19)),
                   PointF(X(4), Y(19)) }, true);
            L(12, 7, 12, 17);
            break;
        case Icon::Heart:
            poly({ PointF(X(12), Y(20)), PointF(X(4), Y(12.5f)), PointF(X(4), Y(8)),
                   PointF(X(6), Y(5.5f)), PointF(X(9.5f), Y(5.5f)), PointF(X(12), Y(8)),
                   PointF(X(14.5f), Y(5.5f)), PointF(X(18), Y(5.5f)), PointF(X(20), Y(8)),
                   PointF(X(20), Y(12.5f)) }, true);
            break;
        default:
            break;
    }
}

// ------------------------------------------------------------------ widgets
bool button(const char* id, const std::wstring& label, const Rect& r, ButtonKind kind,
            bool enabled, Icon icon) {
    if (!g_activePainter) return false;
    Painter& Pnt = *g_activePainter;
    std::string key = id;
    bool hovered = enabled && !F.blockClicks && r.contains(in.mouse);
    if (hovered) setCursorHand(true);
    float& anim = g_hoverAnim[key];
    anim += ((hovered ? 1.0f : 0.0f) - anim) * 0.28f;
    float pressOffset = (hovered && in.down && enabled) ? 0.5f : 0.0f;
    Rect rr = r.offset(0, pressOffset);
    if (!enabled) {
        // disabled
    }
    switch (kind) {
        case ButtonKind::Primary: {
            Gdiplus::Color a = mix(P.accent, P.white, 0.10f * anim);
            Gdiplus::Color b = mix(P.accent2, P.white, 0.12f * anim);
            if (!enabled) { a = rgba(45, 57, 74); b = rgba(38, 48, 63); }
            Pnt.shadow(Rect(rr.x, rr.y + 2, rr.w, rr.h), 9, 60, 8);
            Pnt.fillGradient(rr, a, b, false, 9);
            Gdiplus::Color fg = enabled ? rgba(7, 18, 20) : P.textMuted;
            float tw = Pnt.measure(label, Font::Button);
            float iw = icon != Icon::None ? 17 + 8 : 0;
            float startX = rr.x + (rr.w - (tw + iw)) / 2;
            if (icon != Icon::None) {
                drawIcon(Pnt, icon, Rect(startX, rr.cy() - 8.5f, 17, 17), fg, 2.0f);
                startX += iw;
            }
            Pnt.text(label, Rect(startX, rr.y, tw + 4, rr.h), fg, Font::Button,
                     Align::Left, VAlign::Middle, false, false);
            break;
        }
        case ButtonKind::Danger: {
            Pnt.fill(rr, mix(rgba(245, 100, 112, 20), rgba(245, 100, 112, 42), anim), 9);
            Pnt.stroke(rr, mix(rgba(245, 100, 112, 110), P.danger, anim), 1, 9);
            Pnt.text(label, rr, P.danger, Font::Button, Align::Center, VAlign::Middle, false, false);
            break;
        }
        case ButtonKind::Success: {
            Pnt.fill(rr, mix(rgba(70, 209, 124, 18), rgba(70, 209, 124, 40), anim), 9);
            Pnt.stroke(rr, mix(rgba(70, 209, 124, 100), P.success, anim), 1, 9);
            Pnt.text(label, rr, P.success, Font::Button, Align::Center, VAlign::Middle, false, false);
            break;
        }
        case ButtonKind::Secondary: {
            Pnt.fill(rr, mix(P.card, P.cardHover, anim), 9);
            Pnt.stroke(rr, mix(P.border, rgba(90, 110, 138), anim), 1, 9);
            Gdiplus::Color fg = enabled ? mix(P.textDim, P.text, anim) : P.textMuted;
            float tw = Pnt.measure(label, Font::Button);
            float iw = icon != Icon::None ? 17 + 8 : 0;
            float startX = rr.x + (rr.w - (tw + iw)) / 2;
            if (icon != Icon::None) {
                drawIcon(Pnt, icon, Rect(startX, rr.cy() - 8.5f, 17, 17), fg, 1.9f);
                startX += iw;
            }
            Pnt.text(label, Rect(startX, rr.y, tw + 4, rr.h), fg, Font::Button,
                     Align::Left, VAlign::Middle, false, false);
            break;
        }
        case ButtonKind::Subtle: {
            Pnt.fill(rr, mix(P.inset, P.cardHover, anim), 9);
            Pnt.stroke(rr, mix(P.borderSoft, P.border, anim), 1, 9);
            Pnt.text(label, rr, enabled ? mix(P.textDim, P.text, anim) : P.textMuted, Font::Button,
                     Align::Center, VAlign::Middle, false, false);
            break;
        }
        case ButtonKind::Ghost:
        default: {
            Pnt.fill(rr, rgba(255, 255, 255, (int)(10 * anim)), 9);
            Gdiplus::Color fg = enabled ? mix(P.textDim, P.text, anim) : P.textMuted;
            float tw = Pnt.measure(label, Font::Button);
            float iw = icon != Icon::None ? 17 + 8 : 0;
            float startX = rr.x + (rr.w - (tw + iw)) / 2;
            if (icon != Icon::None) {
                drawIcon(Pnt, icon, Rect(startX, rr.cy() - 8.5f, 17, 17), fg, 1.9f);
                startX += iw;
            }
            Pnt.text(label, Rect(startX, rr.y, tw + 4, rr.h), fg, Font::Button,
                     Align::Left, VAlign::Middle, false, false);
            break;
        }
    }
    bool clicked = enabled && hovered && in.pressed;
    if (clicked) { in.pressed = false; F.blockClicks = true; return true; }
    return false;
}

bool iconButton(const char* id, Icon icon, const Rect& r, const std::wstring& tip, bool active,
                bool enabled) {
    if (!g_activePainter) return false;
    Painter& p = *g_activePainter;
    std::string key = id;
    bool hovered = enabled && !F.blockClicks && r.contains(in.mouse);
    if (hovered) setCursorHand(true);
    float& anim = g_hoverAnim[key];
    anim += ((hovered ? 1.0f : 0.0f) - anim) * 0.3f;
    if (hovered && !tip.empty()) tooltip(tip);
    if (active) p.fill(r, P.accentSoft, 8);
    else if (anim > 0.01f) p.fill(r, rgba(255, 255, 255, (int)(14 * anim)), 8);
    Gdiplus::Color fg = !enabled ? P.textMuted
                                 : (active ? P.accent : mix(P.textDim, P.text, anim));
    drawIcon(p, icon, Rect(r.cx() - 8.5f, r.cy() - 8.5f, 17, 17), fg, 1.9f);
    bool clicked = enabled && hovered && in.pressed;
    if (clicked) { in.pressed = false; F.blockClicks = true; return true; }
    return false;
}

void tooltip(const std::wstring& text) {
    if (text.empty()) return;
    F.tooltipText = text;
}

void drawBadge(Painter& p, const std::wstring& text, float x, float y, const Gdiplus::Color& fg,
               const Gdiplus::Color& bg, float* outWidth, Font font) {
    float tw = p.measure(text, font);
    Rect r(x, y, tw + 14, 19);
    p.fill(r, bg, 6);
    p.text(text, r, fg, font, Align::Center, VAlign::Middle, false, false);
    if (outWidth) *outWidth = r.w;
}

void drawSectionTitle(Painter& p, const std::wstring& title, const std::wstring& subtitle,
                      const Rect& r) {
    p.text(title, Rect(r.x, r.y, r.w, 30), P.text, Font::H1, Align::Left, VAlign::Middle, false, true);
    if (!subtitle.empty())
        p.text(subtitle, Rect(r.x, r.y + 30, r.w, 22), P.textDim, Font::Body, Align::Left,
               VAlign::Middle, false, true);
}

Gdiplus::Color colorFromName(const std::wstring& name) {
    static const Gdiplus::Color colors[] = {
        rgba(88, 140, 255), rgba(74, 222, 190), rgba(166, 120, 255), rgba(245, 160, 90),
        rgba(245, 100, 130), rgba(80, 200, 140), rgba(120, 170, 255), rgba(235, 190, 80)
    };
    uint32_t h = 2166136261u;
    for (wchar_t c : name) { h ^= (uint32_t)c; h *= 16777619u; }
    return colors[h % (sizeof(colors) / sizeof(colors[0]))];
}

void drawAvatar(Painter& p, const std::wstring& name, float cx, float cy, float radius,
                const Gdiplus::Color& ring) {
    Gdiplus::Color base = colorFromName(name);
    p.glow(cx, cy, radius * 2.2f, withAlpha(base, 45));
    p.circle(cx, cy, radius, mix(base, P.black, 0.15f));
    p.circle(cx, cy, radius - 1, base);
    p.circleStroke(cx, cy, radius, ring, 1.5f);
    p.text(initials(name), Rect(cx - radius, cy - radius, radius * 2, radius * 2), P.white,
           radius > 24 ? Font::H2 : Font::SmallStrong, Align::Center, VAlign::Middle, false, false);
}

void drawProgress(Painter& p, const Rect& r, float progress, const Gdiplus::Color& fill) {
    p.fill(r, rgba(255, 255, 255, 16), r.h / 2);
    if (progress < 0) return;
    progress = std::max(0.0f, std::min(1.0f, progress));
    if (progress <= 0) return;
    Rect f(r.x, r.y, std::max(r.h, r.w * progress), r.h);
    p.fillGradient(f, fill, mix(fill, P.white, 0.15f), false, r.h / 2);
}

std::wstring ellipsize(Painter& p, const std::wstring& text, Font f, float maxWidth) {
    if (p.measure(text, f) <= maxWidth) return text;
    std::wstring out = text;
    while (!out.empty() && p.measure(out + L"…", f) > maxWidth) out.pop_back();
    return out + L"…";
}

void drawSpinner(Painter& p, float cx, float cy, float radius, const Gdiplus::Color& c) {
    GraphicsPath path;
    float start = (float)(fmod(time() * 260.0, 360.0));
    path.AddArc(cx - radius, cy - radius, radius * 2, radius * 2, start, 250);
    strokePath(*p.g(), path, c, 2.2f);
}

// ------------------------------------------------------------------ editable widgets
namespace {

float textAreaWidth(const Rect& r, bool hasIcon) {
    return r.w - 24 - (hasIcon ? 26 : 0) - 26;
}

int indexFromX(Painter& p, const std::wstring& display, Font f, float localX) {
    int best = (int)display.size();
    for (size_t i = 0; i <= display.size(); i++) {
        float w = p.measure(display.substr(0, i), f);
        if (w >= localX) { best = (int)i; break; }
    }
    return best;
}

void insertText(std::wstring& text, EditState& e, const std::wstring& insert) {
    int a = std::min(e.cursor, e.anchor);
    int b = std::max(e.cursor, e.anchor);
    if (a < 0) a = 0;
    if (b > (int)text.size()) b = (int)text.size();
    text.erase((size_t)a, (size_t)(b - a));
    text.insert((size_t)a, insert);
    e.cursor = a + (int)insert.size();
    e.anchor = e.cursor;
}

} // namespace

bool textField(const std::string& id, std::wstring& text, const Rect& r,
               const std::wstring& placeholder, Icon icon, bool password, bool enabled) {
    if (!g_activePainter) return false;
    Painter& p = *g_activePainter;
    EditState& e = g_edits[id];
    bool hasIcon = icon != Icon::None;
    bool hovered = enabled && !F.blockClicks && r.contains(in.mouse);
    if (hovered) {
        setCursorIBeam(true);
        if (!enabled) setCursorHand(true);
    }
    float& anim = g_hoverAnim[id];
    anim += ((hovered ? 1.0f : 0.0f) - anim) * 0.3f;

    // clamp state
    if (e.cursor > (int)text.size()) e.cursor = (int)text.size();
    if (e.anchor > (int)text.size()) e.anchor = (int)text.size();
    if (e.cursor < 0) e.cursor = 0;
    if (e.anchor < 0) e.anchor = 0;

    float pad = 12;
    float textX = r.x + pad + (hasIcon ? 26 : 0);
    float width = textAreaWidth(r, hasIcon);

    bool changed = false;

    // clear button
    Rect clearRect(r.right() - 32, r.cy() - 11, 22, 22);
    bool clearHover = !text.empty() && enabled && !F.blockClicks && clearRect.contains(in.mouse);
    if (clearHover) setCursorHand(true);
    if (clearHover && in.pressed) {
        text.clear();
        e.cursor = e.anchor = 0;
        e.scrollX = 0;
        changed = true;
        in.pressed = false;
        F.blockClicks = true;
        e.focused = true;
        F.fieldClicked = true;
    }

    if (in.pressed && hovered && !clearHover && !F.blockClicks) {
        e.focused = true;
        F.fieldClicked = true;
        in.pressed = false;
        float local = in.mouse.X - textX + e.scrollX;
        int idx = indexFromX(p, password ? bulletize(text) : text, Font::Body, local);
        e.cursor = e.anchor = idx;
        e.dragging = true;
        e.blinkStart = F.now;
    }
    if (in.released) e.dragging = false;
    if (e.focused && e.dragging && in.down && hovered) {
        float local = in.mouse.X - textX + e.scrollX;
        e.cursor = indexFromX(p, password ? bulletize(text) : text, Font::Body, local);
    }

    if (e.focused && enabled) {
        std::wstring display = password ? bulletize(text) : text;
        int selA = std::min(e.cursor, e.anchor), selB = std::max(e.cursor, e.anchor);
        auto eraseSelection = [&]() {
            if (selA == selB) return false;
            text.erase((size_t)selA, (size_t)(selB - selA));
            e.cursor = e.anchor = selA;
            return true;
        };
        bool ctrl = in.ctrl;
        if (ctrl && in.key('A')) { e.anchor = 0; e.cursor = (int)text.size(); }
        if (ctrl && in.key('C') && selA != selB) {
            setClipboardText(text.substr((size_t)selA, (size_t)(selB - selA)));
        }
        if (ctrl && in.key('X') && selA != selB) {
            setClipboardText(text.substr((size_t)selA, (size_t)(selB - selA)));
            if (eraseSelection()) changed = true;
        }
        if (ctrl && in.key('V')) {
            std::wstring clip = clipboardText();
            std::wstring filtered;
            for (wchar_t c : clip) if (c >= 32 && c != 127) filtered.push_back(c);
            if (!filtered.empty()) { insertText(text, e, filtered); changed = true; }
        }
        if (in.key(VK_BACK)) {
            if (selA != selB) { if (eraseSelection()) changed = true; }
            else if (e.cursor > 0) {
                text.erase((size_t)(e.cursor - 1), 1);
                e.cursor--; e.anchor = e.cursor;
                changed = true;
            }
        }
        if (in.key(VK_DELETE)) {
            if (selA != selB) { if (eraseSelection()) changed = true; }
            else if (e.cursor < (int)text.size()) {
                text.erase((size_t)e.cursor, 1);
                changed = true;
            }
        }
        if (in.key(VK_LEFT)) {
            if (e.cursor > 0) e.cursor--;
            if (!in.shift) e.anchor = e.cursor;
        }
        if (in.key(VK_RIGHT)) {
            if (e.cursor < (int)text.size()) e.cursor++;
            if (!in.shift) e.anchor = e.cursor;
        }
        if (in.key(VK_HOME)) { e.cursor = 0; if (!in.shift) e.anchor = 0; }
        if (in.key(VK_END)) { e.cursor = (int)text.size(); if (!in.shift) e.anchor = e.cursor; }
        if (in.key(VK_TAB)) focusNextField();
        if (in.key(VK_RETURN)) {
            g_enterField = id;
            e.focused = false;
        }
        for (wchar_t c : in.chars) {
            if (c < 32 || c == 127) continue;
            std::wstring one(1, c);
            insertText(text, e, one);
            changed = true;
        }
        if (changed) e.blinkStart = F.now;
    }

    // ensure caret visible
    std::wstring display = password ? bulletize(text) : text;
    if (e.focused) {
        float caretX = p.measure(display.substr(0, (size_t)e.cursor), Font::Body);
        if (caretX - e.scrollX > width - 4) e.scrollX = caretX - width + 4;
        if (caretX - e.scrollX < 0) e.scrollX = caretX;
        if (text.empty()) e.scrollX = 0;
    } else {
        e.scrollX = 0;
    }

    // background
    Gdiplus::Color bg = mix(P.inset, P.card, anim * 0.4f);
    p.fill(r, bg, 9);
    p.stroke(r, e.focused ? withAlpha(P.accent, 180) : mix(P.borderSoft, P.border, anim), 1, 9);
    if (e.focused) p.stroke(r.inset(-1, -1), withAlpha(P.accent, 45), 1, 10);

    if (hasIcon) {
        drawIcon(p, icon, Rect(r.x + 12, r.cy() - 8, 16, 16),
                 e.focused ? P.accent : P.textMuted, 1.7f);
    }

    // text area
    Rect area(textX, r.y + 2, width, r.h - 4);
    p.clip(area);
    if (text.empty() && !e.focused) {
        p.text(placeholder, Rect(area.x, area.y, area.w + 40, area.h), P.textMuted, Font::Body,
               Align::Left, VAlign::Middle, false, true);
    } else {
        int a = std::min(e.cursor, e.anchor), b = std::max(e.cursor, e.anchor);
        if (e.focused && b > a) {
            float x1 = p.measure(display.substr(0, (size_t)a), Font::Body) - e.scrollX;
            float x2 = p.measure(display.substr(0, (size_t)b), Font::Body) - e.scrollX;
            p.fill(Rect(area.x + x1, area.y + 3, x2 - x1, area.h - 6),
                   withAlpha(P.accent, 40), 3);
        }
        p.text(display, Rect(area.x - e.scrollX, area.y, area.w + e.scrollX + 2, area.h),
               enabled ? P.text : P.textDim, Font::Body, Align::Left, VAlign::Middle, false, false);
        if (e.focused && fmod(F.now - e.blinkStart, 1.1) < 0.6) {
            float caretX = area.x + p.measure(display.substr(0, (size_t)e.cursor), Font::Body) -
                           e.scrollX;
            float h = p.lineHeight(Font::Body);
            p.fill(Rect(caretX, r.cy() - h / 2 + 1, 1.4f, h - 2), P.accent);
        }
    }
    p.restore();

    if (clearHover) {
        p.circle(clearRect.cx(), clearRect.cy(), 9, rgba(255, 255, 255, 18));
        drawIcon(p, Icon::Close, Rect(clearRect.cx() - 6, clearRect.cy() - 6, 12, 12), P.text, 1.7f);
    }
    return changed;
}

bool searchField(const std::string& id, std::wstring& text, const Rect& r,
                 const std::wstring& placeholder, bool* submitted) {
    bool changed = textField(id, text, r, placeholder, Icon::Search);
    if (submitted && g_enterField == id) *submitted = true;
    return changed;
}

bool checkbox(const char* id, bool& value, const Rect& r, const std::wstring& label) {
    if (!g_activePainter) return false;
    Painter& p = *g_activePainter;
    bool hovered = !F.blockClicks && r.contains(in.mouse);
    if (hovered) setCursorHand(true);
    float anim = animate(id, hovered, 0.3f);
    float s = 19;
    Rect box(r.x, r.cy() - s / 2, s, s);
    p.fill(box, value ? P.accent : mix(P.inset, P.cardHover, anim), 5);
    if (!value) p.stroke(box, mix(P.border, withAlpha(P.accent, 160), anim), 1.2f, 5);
    if (value) drawIcon(p, Icon::Check, box.inset(3), rgba(7, 20, 20), 2.4f);
    p.text(label, Rect(box.right() + 11, r.y, r.w - s - 11, r.h),
           mix(P.textDim, P.text, anim), Font::Body, Align::Left, VAlign::Middle, false, true);
    if (hovered && in.pressed) {
        value = !value;
        in.pressed = false;
        F.blockClicks = true;
        return true;
    }
    return false;
}

bool toggle(const char* id, bool& value, const Rect& r) {
    if (!g_activePainter) return false;
    Painter& p = *g_activePainter;
    bool hovered = !F.blockClicks && r.contains(in.mouse);
    if (hovered) setCursorHand(true);
    float anim = animate(id, hovered, 0.3f);
    float on = animate(std::string(id) + "#on", value, 0.35f);
    Rect track(r.x, r.cy() - 12, 44, 24);
    p.fill(track, mix(mix(P.cardActive, P.border, 0.4f), P.accent, on), 12);
    if (on < 0.5f) p.stroke(track, mix(P.border, P.textMuted, anim * 0.4f), 1, 12);
    float kx = track.x + 3 + on * (track.w - 22);
    p.circle(kx + 8, track.cy(), 8, mix(P.textDim, P.white, on));
    if (hovered && in.pressed) {
        value = !value;
        in.pressed = false;
        F.blockClicks = true;
        return true;
    }
    return false;
}

bool sliderInt(const char* id, int& value, int minValue, int maxValue, int step, const Rect& r,
               const std::wstring& suffix) {
    if (!g_activePainter) return false;
    Painter& p = *g_activePainter;
    bool hovered = !F.blockClicks && r.contains(in.mouse);
    if (hovered) setCursorHand(true);
    std::string key = id;
    float anim = animate(key, hovered || g_dragSlider == key, 0.3f);
    float trackX = r.x + 9, trackW = r.w - 18;
    float t = maxValue > minValue ? (float)(value - minValue) / (float)(maxValue - minValue) : 0;
    Rect track(trackX, r.cy() - 3, trackW, 6);
    p.fill(track, rgba(255, 255, 255, 14), 3);
    p.fillGradient(Rect(track.x, track.y, track.w * t, track.h), P.accent, P.accent2, false, 3);
    float kx = trackX + trackW * t;
    p.circle(kx, r.cy(), 8 + anim * 2.5f, withAlpha(P.accent, 45));
    p.circle(kx, r.cy(), 7, P.white);

    bool changed = false;
    bool overTrack = Rect(r.x, r.cy() - 12, r.w, 24).contains(in.mouse);
    if (overTrack && in.pressed && !F.blockClicks) {
        g_dragSlider = key;
        in.pressed = false;
        F.blockClicks = true;
    }
    if (g_dragSlider == key) {
        if (in.down) {
            float local = std::max(0.0f, std::min(trackW, in.mouse.X - trackX));
            int nv = minValue + (int)std::lround(local / std::max(1.0f, trackW) * (maxValue - minValue));
            if (step > 1) nv = minValue + (int)std::lround((double)(nv - minValue) / step) * step;
            nv = std::max(minValue, std::min(maxValue, nv));
            if (nv != value) { value = nv; changed = true; }
        } else {
            g_dragSlider.clear();
        }
    }
    if (!suffix.empty()) {
        p.text(suffix, Rect(r.x, r.y - 2, r.w, 18), P.textDim, Font::Small, Align::Right,
               VAlign::Middle, false, false);
    }
    return changed;
}

bool dropdown(const char* id, int& index, const std::vector<std::wstring>& items, const Rect& r,
              const std::wstring& placeholder) {
    if (!g_activePainter) return false;
    Painter& p = *g_activePainter;
    std::string key = id;
    bool open = F.popupOpen && F.popupId == key;
    bool hovered = !F.blockClicks && r.contains(in.mouse);
    if (hovered) setCursorHand(true);
    float anim = animate(key, hovered, 0.3f);

    if (open) {
        float itemH = 32;
        float ph = items.size() * itemH + 8;
        float py = r.bottom() + 6;
        if (py + ph > F.h - 10) py = std::max(10.0f, r.y - ph - 6);
        F.popupRect = Rect(r.x, py, std::max(r.w, 180.0f), ph);
        F.popupItems = items;
        F.popupIndex = index;
        if (in.pressed) {
            if (F.popupRect.contains(in.mouse)) {
                int sel = (int)((in.mouse.Y - F.popupRect.y - 4) / itemH);
                if (sel >= 0 && sel < (int)items.size()) index = sel;
                F.popupOpen = false;
                in.pressed = false;
                F.blockClicks = true;
                return true;
            }
            if (!r.contains(in.mouse)) F.popupOpen = false;
            in.pressed = false;
            F.blockClicks = true;
        }
        setClickBlocked(true);
    }

    p.fill(r, mix(P.inset, P.card, anim * 0.4f), 9);
    p.stroke(r, open ? withAlpha(P.accent, 180) : mix(P.borderSoft, P.border, anim), 1, 9);
    std::wstring label = (index >= 0 && index < (int)items.size()) ? items[(size_t)index] : placeholder;
    p.text(label, Rect(r.x + 12, r.y, r.w - 42, r.h),
           (index >= 0 && index < (int)items.size()) ? P.text : P.textMuted, Font::Body,
           Align::Left, VAlign::Middle, false, true);
    drawIcon(p, Icon::ChevronDown, Rect(r.right() - 26, r.cy() - 8, 16, 16),
             open ? P.accent : P.textDim, 1.8f);

    if (!open && hovered && in.pressed && !F.blockClicks) {
        F.popupOpen = true;
        F.popupId = key;
        F.popupTrigger = r;
        in.pressed = false;
        F.blockClicks = true;
        F.fieldClicked = true;
    }
    return false;
}

bool segmented(const char* id, int& index, const std::vector<std::wstring>& labels, const Rect& r) {
    if (!g_activePainter) return false;
    Painter& p = *g_activePainter;
    p.fill(r, P.inset, 9);
    p.stroke(r, P.borderSoft, 1, 9);
    if (labels.empty()) return false;
    float segW = (r.w - 8) / (float)labels.size();
    bool changed = false;
    for (size_t i = 0; i < labels.size(); i++) {
        Rect seg(r.x + 4 + segW * (float)i, r.y + 4, segW, r.h - 8);
        bool hov = !F.blockClicks && seg.contains(in.mouse);
        if (hov) setCursorHand(true);
        float anim = animate(std::string(id) + "#" + std::to_string(i), hov, 0.3f);
        bool selected = (int)i == index;
        if (selected) {
            p.fill(seg, P.cardActive, 7);
            p.stroke(seg, P.border, 1, 7);
        } else if (anim > 0.01f) {
            p.fill(seg, rgba(255, 255, 255, (int)(8 * anim)), 7);
        }
        p.text(labels[i], seg, selected ? P.text : mix(P.textDim, P.text, anim), Font::SmallStrong,
               Align::Center, VAlign::Middle, false, false);
        if (!selected && hov && in.pressed) {
            index = (int)i;
            changed = true;
            in.pressed = false;
            F.blockClicks = true;
        }
    }
    return changed;
}

float beginScroll(const char* id, const Rect& view, float contentHeight) {
    if (!g_activePainter) return 0;
    Painter& p = *g_activePainter;
    ScrollState& s = g_scrolls[id];
    s.view = view;
    s.viewH = view.h;
    s.content = contentHeight;
    float maxOff = std::max(0.0f, contentHeight - view.h);
    if (maxOff <= 0.5f) {
        s.offset = s.target = 0;
    }
    if (!F.blockClicks && view.contains(in.mouse) && in.wheel != 0) {
        s.target -= (float)in.wheel * 90.0f;
        s.target = std::max(0.0f, std::min(maxOff, s.target));
        in.wheel = 0;
    }
    if (!in.down) s.dragging = false;

    float barW = 4;
    Rect bar(view.right() - barW - 2, view.y, barW, view.h);
    float thumbH = std::max(28.0f, view.h * view.h / std::max(view.h, contentHeight));
    float thumbY = view.y + (maxOff > 0 ? (view.h - thumbH) * s.offset / maxOff : 0);
    Rect thumb(bar.x, thumbY, barW, thumbH);

    if (maxOff > 0.5f) {
        p.fill(bar, rgba(255, 255, 255, 10), 2);
        bool th = !F.blockClicks && thumb.contains(in.mouse);
        bool hoveredBar = !F.blockClicks && view.contains(in.mouse);
        if (th) setCursorHand(true);
        if (th && in.pressed) {
            s.dragging = true;
            s.dragStartY = in.mouse.Y;
            s.dragStartOffset = s.offset;
            in.pressed = false;
            F.blockClicks = true;
        }
        if (s.dragging && in.down) {
            float range = view.h - thumbH;
            if (range > 1) {
                float delta = (in.mouse.Y - s.dragStartY) / range * maxOff;
                s.target = std::max(0.0f, std::min(maxOff, s.dragStartOffset + delta));
            }
        }
        float hover = animate(std::string(id) + "#bar", th || hoveredBar, 0.3f);
        p.fill(thumb, mix(rgba(255, 255, 255, 35), rgba(255, 255, 255, 80), hover), 2);
    }

    s.offset += (s.target - s.offset) * 0.35f;
    if (std::fabs(s.target - s.offset) < 0.4f) s.offset = s.target;
    s.offset = std::max(0.0f, std::min(maxOff, s.offset));

    p.clip(view);
    p.translate(0, -s.offset);
    return s.offset;
}

void endScroll() {
    if (g_activePainter) g_activePainter->restore();
}

void scrollTo(const char* id, float offset) {
    ScrollState& s = g_scrolls[id];
    s.target = std::max(0.0f, offset);
    s.offset = s.target;
}

float scrollOffset(const char* id) { return g_scrolls[id].offset; }

void toastCard(Painter& p, const std::wstring& text, bool error, float alpha) {
    float w = std::min(420.0f, p.measure(text, Font::Body) + 58);
    Rect r(width() - w - 24, 56, w, 46);
    p.shadow(r, 10, (int)(70 * alpha), 10);
    p.fill(r, withAlpha(rgba(24, 31, 45), (int)(245 * alpha)), 10);
    p.stroke(r, withAlpha(error ? P.danger : P.success, (int)(120 * alpha)), 1, 10);
    drawIcon(p, error ? Icon::Alert : Icon::CheckCircle, Rect(r.x + 14, r.cy() - 9, 18, 18),
             withAlpha(error ? P.danger : P.success, (int)(255 * alpha)), 1.8f);
    p.text(text, Rect(r.x + 42, r.y, r.w - 54, r.h), withAlpha(P.text, (int)(255 * alpha)),
           Font::Small, Align::Left, VAlign::Middle, false, true);
}

void resetFrameInput() {
    in.pressed = false;
    in.released = false;
    in.wheel = 0;
    in.keys.clear();
    in.chars.clear();
}

void closePopup() {
    F.popupOpen = false;
    F.popupId.clear();
}

} // namespace aurora::ui
