#pragma once
// Aurora Launcher - custom drawn UI toolkit (GDI+, immediate mode).
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "../core/util.h"

namespace aurora::ui {

// ---------------------------------------------------------------- geometry
struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    Rect() = default;
    Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}
    float right() const { return x + w; }
    float bottom() const { return y + h; }
    float cx() const { return x + w / 2; }
    float cy() const { return y + h / 2; }
    bool contains(float px, float py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
    bool contains(const Gdiplus::PointF& p) const { return contains(p.X, p.Y); }
    Rect inset(float d) const { return { x + d, y + d, w - 2 * d, h - 2 * d }; }
    Rect inset(float dx, float dy) const { return { x + dx, y + dy, w - 2 * dx, h - 2 * dy }; }
    Rect offset(float dx, float dy) const { return { x + dx, y + dy, w, h }; }
    Rect withHeight(float nh) const { return { x, y, w, nh }; }
    Rect withWidth(float nw) const { return { x, y, nw, h }; }
    Rect cutTop(float amount) { Rect r(x, y, w, amount); y += amount; h -= amount; return r; }
    Rect cutBottom(float amount) { h -= amount; return { x, y + h, w, amount }; }
    Rect cutLeft(float amount) { Rect r(x, y, amount, h); x += amount; w -= amount; return r; }
    Rect cutRight(float amount) { w -= amount; return { x + w, y, amount, h }; }
    Gdiplus::RectF g() const { return { x, y, w, h }; }
};

inline Gdiplus::Color rgba(int r, int g, int b, int a = 255) {
    return Gdiplus::Color((BYTE)a, (BYTE)r, (BYTE)g, (BYTE)b);
}
inline Gdiplus::Color withAlpha(const Gdiplus::Color& c, int a) {
    return Gdiplus::Color((BYTE)a, c.GetR(), c.GetG(), c.GetB());
}
inline Gdiplus::Color mix(const Gdiplus::Color& a, const Gdiplus::Color& b, float t) {
    auto l = [&](int x, int y) { return (int)(x + (y - x) * t + 0.5f); };
    return rgba(l(a.GetR(), b.GetR()), l(a.GetG(), b.GetG()), l(a.GetB(), b.GetB()),
                l(a.GetA(), b.GetA()));
}

// ---------------------------------------------------------------- palette
struct Palette {
    Gdiplus::Color bg = rgba(9, 12, 18);
    Gdiplus::Color bgDeep = rgba(7, 9, 14);
    Gdiplus::Color sidebar = rgba(12, 16, 24, 246);
    Gdiplus::Color titleBar = rgba(11, 15, 22, 250);
    Gdiplus::Color card = rgba(19, 25, 37);
    Gdiplus::Color cardHover = rgba(25, 33, 48);
    Gdiplus::Color cardActive = rgba(30, 40, 58);
    Gdiplus::Color inset = rgba(13, 18, 27);
    Gdiplus::Color border = rgba(38, 49, 68);
    Gdiplus::Color borderSoft = rgba(30, 40, 56);
    Gdiplus::Color text = rgba(230, 237, 248);
    Gdiplus::Color textDim = rgba(148, 162, 184);
    Gdiplus::Color textMuted = rgba(103, 117, 139);
    Gdiplus::Color accent = rgba(74, 222, 190);
    Gdiplus::Color accent2 = rgba(88, 140, 255);
    Gdiplus::Color accentDeep = rgba(37, 99, 235);
    Gdiplus::Color accentSoft = rgba(74, 222, 190, 32);
    Gdiplus::Color success = rgba(70, 209, 124);
    Gdiplus::Color warn = rgba(242, 184, 75);
    Gdiplus::Color danger = rgba(245, 100, 112);
    Gdiplus::Color purple = rgba(166, 120, 255);
    Gdiplus::Color white = rgba(255, 255, 255);
    Gdiplus::Color black = rgba(0, 0, 0);
    Gdiplus::Color overlay = rgba(4, 6, 10, 190);
};
extern Palette P;

// ---------------------------------------------------------------- fonts
enum class Font {
    Display, H1, H2, H3, Body, BodyStrong, Small, SmallStrong, Tiny,
    Button, Mono, MonoSmall, Numeral
};

// ---------------------------------------------------------------- input
struct Input {
    Gdiplus::PointF mouse{ -1, -1 };
    Gdiplus::PointF mousePrev{ -1, -1 };
    bool down = false;
    bool pressed = false;
    bool released = false;
    bool rightPressed = false;
    bool doubleClick = false;
    int wheel = 0;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    std::vector<int> keys;          // virtual keys pressed this frame
    std::wstring chars;             // typed characters this frame
    bool escape = false;

    bool key(int vk) const;
    bool hover(const Rect& r) const { return r.contains(mouse); }
};
extern Input in;

// ---------------------------------------------------------------- painter
enum class Align { Left, Center, Right };
enum class VAlign { Top, Middle, Bottom };

class Painter {
public:
    void begin(Gdiplus::Graphics* g);
    Gdiplus::Graphics* g() const { return g_; }

    void fill(const Rect& r, const Gdiplus::Color& c);
    void fill(const Rect& r, const Gdiplus::Color& c, float radius);
    void fillGradient(const Rect& r, const Gdiplus::Color& a, const Gdiplus::Color& b,
                      bool vertical = true, float radius = 0);
    void stroke(const Rect& r, const Gdiplus::Color& c, float width = 1, float radius = 0);
    void line(float x1, float y1, float x2, float y2, const Gdiplus::Color& c, float width = 1);
    void circle(float cx, float cy, float radius, const Gdiplus::Color& fillColor);
    void circleStroke(float cx, float cy, float radius, const Gdiplus::Color& c, float width = 1);
    void shadow(const Rect& r, float radius, int alpha = 90, float spread = 10);
    void glow(float cx, float cy, float radius, const Gdiplus::Color& c);
    void text(const std::wstring& s, const Rect& r, const Gdiplus::Color& c, Font f,
              Align align = Align::Left, VAlign valign = VAlign::Middle, bool wrap = false,
              bool ellipsis = true);
    void textPoint(const std::wstring& s, float x, float y, const Gdiplus::Color& c, Font f);
    float measure(const std::wstring& s, Font f) const;
    float lineHeight(Font f) const;
    void clip(const Rect& r);
    void restore();
    void push();
    void translate(float dx, float dy);
    Rect clipRect() const { return clip_; }

private:
    void setupText(Gdiplus::StringFormat& fmt, Align a, VAlign v, bool wrap, bool ellipsis) const;
    Gdiplus::Graphics* g_ = nullptr;
    std::vector<Gdiplus::GraphicsState> states_;
    Rect clip_{ 0, 0, 99999, 99999 };
};

// ---------------------------------------------------------------- icons
enum class Icon {
    None, Play, Stop, Pause, Cube, Puzzle, Server, Terminal, Settings, User, Search,
    Plus, Minus, Close, Minimize, Maximize, Restore, ChevronDown, ChevronRight,
    ChevronLeft, Download, Trash, Folder, Refresh, Check, Alert, Info, External,
    Microsoft, Java, Copy, LogOut, Link, Globe, Shield, Sparkle, Edit, Power, Clock,
    Grid, Star, More, Eye, CheckCircle, XCircle, Wifi, Package, Sliders, Book, Heart
};
void drawIcon(Painter& p, Icon icon, const Rect& r, const Gdiplus::Color& c, float stroke = 1.8f);
float iconSize();

// ---------------------------------------------------------------- frame
void beginFrame(float width, float height, float dpiScale);
void endFrame(Painter& p);
float width();
float height();
void setCursorHand(bool hand);
void setCursorIBeam(bool beam);
bool wantsHandCursor();
bool wantsBeamCursor();
float animate(const std::string& id, bool active, float speed);
double time();

// ---------------------------------------------------------------- widgets
enum class ButtonKind { Primary, Secondary, Ghost, Danger, Subtle, Success };

bool button(const char* id, const std::wstring& label, const Rect& r,
            ButtonKind kind = ButtonKind::Primary, bool enabled = true, Icon icon = Icon::None);
bool iconButton(const char* id, Icon icon, const Rect& r, const std::wstring& tooltip = L"",
                bool active = false, bool enabled = true);
bool textField(const std::string& id, std::wstring& text, const Rect& r,
               const std::wstring& placeholder, Icon icon = Icon::None, bool password = false,
               bool enabled = true);
bool searchField(const std::string& id, std::wstring& text, const Rect& r,
                 const std::wstring& placeholder, bool* submitted = nullptr);
bool checkbox(const char* id, bool& value, const Rect& r, const std::wstring& label);
bool toggle(const char* id, bool& value, const Rect& r);
bool sliderInt(const char* id, int& value, int minValue, int maxValue, int step, const Rect& r,
               const std::wstring& suffix = L"");
bool dropdown(const char* id, int& index, const std::vector<std::wstring>& items, const Rect& r,
              const std::wstring& placeholder = L"");
bool segmented(const char* id, int& index, const std::vector<std::wstring>& labels, const Rect& r);
float beginScroll(const char* id, const Rect& view, float contentHeight);
void endScroll();
void scrollTo(const char* id, float offset);
float scrollOffset(const char* id);
void tooltip(const std::wstring& text);
void toastCard(Painter& p, const std::wstring& text, bool error, float alpha);
void setClickBlocked(bool blocked);
bool clickBlocked();
void closePopup();
void focusNextField();
bool anyFieldFocused();
bool animating();

// ---------------------------------------------------------------- helpers
void drawBadge(Painter& p, const std::wstring& text, float x, float y, const Gdiplus::Color& fg,
               const Gdiplus::Color& bg, float* outWidth = nullptr, Font font = Font::Tiny);
void drawSectionTitle(Painter& p, const std::wstring& title, const std::wstring& subtitle,
                      const Rect& r);
void drawAvatar(Painter& p, const std::wstring& name, float cx, float cy, float radius,
                const Gdiplus::Color& ring = rgba(255, 255, 255, 30));
Gdiplus::Color colorFromName(const std::wstring& name);
void drawProgress(Painter& p, const Rect& r, float progress, const Gdiplus::Color& fill);
std::wstring ellipsize(Painter& p, const std::wstring& text, Font f, float maxWidth);
void drawSpinner(Painter& p, float cx, float cy, float radius, const Gdiplus::Color& c);

} // namespace aurora::ui
