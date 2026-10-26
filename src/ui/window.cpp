// Aurora Launcher - frameless window, DPI, input plumbing and frame timing.
#include "window.h"

#include <dwmapi.h>
#include <shellscalingapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <string>

#pragma comment(lib, "dwmapi.lib")

using namespace Gdiplus;

namespace aurora::ui {

namespace {

HWND g_hwnd = nullptr;
FrameHooks g_hooks;
WindowOptions g_options;
float g_scale = 1.0f;
bool g_screenshotSaved = false;
ULONGLONG g_startTick = 0;
int g_lastW = 0, g_lastH = 0;
HCURSOR g_cursorArrow = nullptr;
HCURSOR g_cursorHand = nullptr;
HCURSOR g_cursorBeam = nullptr;
ULONG_PTR g_gdiplusToken = 0;
bool g_trackingLeave = false;
std::wstring g_pendingScreenshot;
int g_screenshotDelay = 1500;

bool paintFrame(HWND hwnd, HDC targetDC, bool saveScreenshot) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return false;

    UINT dpi = 96;
    if (auto getDpi = (UINT(WINAPI*)(HWND))GetProcAddress(GetModuleHandleW(L"user32.dll"),
                                                          "GetDpiForWindow")) {
        dpi = getDpi(hwnd);
    }
    g_scale = dpi > 0 ? dpi / 96.0f : 1.0f;
    float lw = w / g_scale;
    float lh = h / g_scale;
    g_lastW = (int)lw;
    g_lastH = (int)lh;

    Bitmap bitmap(w, h, PixelFormat32bppPARGB);
    Graphics g(&bitmap);
    g.Clear(Color(255, P.bg.GetR(), P.bg.GetG(), P.bg.GetB()));
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.ScaleTransform(g_scale, g_scale);

    Painter painter;
    painter.begin(&g);
    beginFrame(lw, lh, g_scale);
    if (g_hooks.paint) g_hooks.paint(painter, lw, lh);
    endFrame(painter);

    if (saveScreenshot && !g_screenshotSaved && !g_pendingScreenshot.empty()) {
        CLSID pngClsid{};
        UINT num = 0, size = 0;
        GetImageEncodersSize(&num, &size);
        if (size > 0) {
            auto* codecs = (ImageCodecInfo*)malloc(size);
            if (codecs) {
                GetImageEncoders(num, size, codecs);
                for (UINT i = 0; i < num; i++) {
                    if (wcscmp(codecs[i].MimeType, L"image/png") == 0) {
                        pngClsid = codecs[i].Clsid;
                        break;
                    }
                }
                free(codecs);
            }
        }
        if (pngClsid.Data1 != 0) {
            bitmap.Save(g_pendingScreenshot.c_str(), &pngClsid, nullptr);
            g_screenshotSaved = true;
        }
    }

    if (targetDC) {
        Graphics screen(targetDC);
        screen.SetCompositingMode(CompositingModeSourceCopy);
        screen.DrawImage(&bitmap, 0, 0, w, h);
    }
    return true;
}

void renderNow(bool saveScreenshot = false) {
    if (!g_hwnd) return;
    HDC dc = GetDC(g_hwnd);
    if (dc) {
        paintFrame(g_hwnd, dc, saveScreenshot);
        ReleaseDC(g_hwnd, dc);
    }
}

bool shouldAnimate() {
    if (animating()) return true;
    if (g_hooks.needsFrames && g_hooks.needsFrames()) return true;
    return false;
}

void applyDwm(HWND hwnd) {
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    int corner = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, 33, &corner, sizeof(corner));
    MARGINS margins{ 0, 0, 0, 0 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

LRESULT hitTest(HWND hwnd, LPARAM lParam) {
    POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    ScreenToClient(hwnd, &pt);
    float x = pt.x / g_scale;
    float y = pt.y / g_scale;
    float lw = g_lastW > 0 ? (float)g_lastW : g_options.width;
    float lh = g_lastH > 0 ? (float)g_lastH : g_options.height;
    float border = 6.0f;
    bool maximized = IsZoomed(hwnd) != 0;

    if (!maximized) {
        bool left = x < border, right = x >= lw - border;
        bool top = y < border, bottom = y >= lh - border;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
    }
    if (y < 44.0f) {
        bool interactive = g_hooks.titleBarInteractive && g_hooks.titleBarInteractive(x, y);
        if (!interactive) return HTCAPTION;
    }
    return HTCLIENT;
}

void onMouse(HWND hwnd, LPARAM lParam, int type) {
    float x = GET_X_LPARAM(lParam) / g_scale;
    float y = GET_Y_LPARAM(lParam) / g_scale;
    if (type == WM_MOUSEMOVE) {
        in.mouse = PointF(x, y);
        if (!g_trackingLeave) {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            g_trackingLeave = true;
        }
        renderNow();
    } else if (type == WM_LBUTTONDOWN) {
        SetCapture(hwnd);
        SetFocus(hwnd);
        in.mouse = PointF(x, y);
        in.down = true;
        in.pressed = true;
        in.doubleClick = false;
        renderNow();
    } else if (type == WM_LBUTTONDBLCLK) {
        SetCapture(hwnd);
        in.mouse = PointF(x, y);
        in.down = true;
        in.pressed = true;
        in.doubleClick = true;
        renderNow();
    } else if (type == WM_LBUTTONUP) {
        if (GetCapture() == hwnd) ReleaseCapture();
        in.mouse = PointF(x, y);
        in.down = false;
        in.released = true;
        renderNow();
    }
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_NCCALCSIZE: {
            if (wParam == TRUE) {
                auto* params = (NCCALCSIZE_PARAMS*)lParam;
                if (IsZoomed(hwnd)) {
                    int frame = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                    params->rgrc[0].left += frame;
                    params->rgrc[0].right -= frame;
                    params->rgrc[0].top += frame;
                    params->rgrc[0].bottom -= frame;
                }
                return 0;
            }
            break;
        }
        case WM_NCHITTEST:
            return hitTest(hwnd, lParam);
        case WM_NCACTIVATE:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        case WM_GETMINMAXINFO: {
            auto* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = (LONG)(980 * g_scale);
            mmi->ptMinTrackSize.y = (LONG)(640 * g_scale);
            return 0;
        }
        case WM_DPICHANGED: {
            g_scale = HIWORD(wParam) / 96.0f;
            auto* suggested = (RECT*)lParam;
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            if (g_hooks.onResize) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                g_hooks.onResize((rc.right - rc.left) / g_scale, (rc.bottom - rc.top) / g_scale);
            }
            renderNow();
            return 0;
        }
        case WM_SIZE: {
            int w = LOWORD(lParam), h = HIWORD(lParam);
            if (w > 0 && h > 0 && g_hooks.onResize)
                g_hooks.onResize(w / g_scale, h / g_scale);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            if (dc) paintFrame(hwnd, dc, false);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
            onMouse(hwnd, lParam, msg);
            return 0;
        case WM_MOUSELEAVE:
            g_trackingLeave = false;
            in.mouse = PointF(-1, -1);
            renderNow();
            return 0;
        case WM_RBUTTONDOWN:
            in.rightPressed = true;
            renderNow();
            return 0;
        case WM_MOUSEWHEEL: {
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            in.mouse = PointF(pt.x / g_scale, pt.y / g_scale);
            in.wheel += GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            renderNow();
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            in.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            in.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            in.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
            if (wParam == VK_ESCAPE) in.escape = true;
            in.keys.push_back((int)wParam);
            if (wParam == VK_TAB) {
                // Tab focus traversal is handled by the focused text field.
            }
            renderNow();
            if (wParam == VK_F12 && !g_pendingScreenshot.empty()) {
                g_screenshotSaved = false;
                renderNow(true);
            }
            return 0;
        }
        case WM_CHAR: {
            wchar_t c = (wchar_t)wParam;
            if (c >= 32 || c == 9) in.chars.push_back(c);
            renderNow();
            return 0;
        }
        case WM_SETCURSOR: {
            if (LOWORD(lParam) == HTCLIENT) {
                if (in.mouse.X < 0) SetCursor(g_cursorArrow);
                else if (wantsBeamCursor()) SetCursor(g_cursorBeam);
                else if (wantsHandCursor()) SetCursor(g_cursorHand);
                else SetCursor(g_cursorArrow);
                return TRUE;
            }
            break;
        }
        case WM_TIMER: {
            if (wParam == 1) {
                bool screenshotDue = false;
                ULONGLONG elapsed = GetTickCount64() - g_startTick;
                if (!g_pendingScreenshot.empty() && !g_screenshotSaved &&
                    elapsed >= (ULONGLONG)g_screenshotDelay) {
                    screenshotDue = true;
                }
                if (screenshotDue || shouldAnimate()) renderNow(screenshotDue);
                if (g_screenshotSaved && screenshotDue) {
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                }
            }
            return 0;
        }
        case WM_ACTIVATE:
            if (LOWORD(wParam) != WA_INACTIVE) renderNow();
            return 0;
        case WM_CLOSE: {
            if (g_hooks.onClose) g_hooks.onClose();
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

float windowScale() { return g_scale; }
bool windowMaximized() { return g_hwnd && IsZoomed(g_hwnd); }
void windowMinimize() { if (g_hwnd) ShowWindow(g_hwnd, SW_MINIMIZE); }
void windowClose() { if (g_hwnd) PostMessageW(g_hwnd, WM_CLOSE, 0, 0); }
void windowMaximizeToggle() {
    if (!g_hwnd) return;
    ShowWindow(g_hwnd, IsZoomed(g_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
}
void windowInvalidate() { if (g_hwnd) InvalidateRect(g_hwnd, nullptr, FALSE); }

int runWindow(HINSTANCE instance, const WindowOptions& options, FrameHooks hooks) {
    g_options = options;
    g_hooks = std::move(hooks);
    g_pendingScreenshot = options.screenshotPath;
    g_screenshotDelay = options.screenshotDelayMs;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    GdiplusStartupInput gpInput;
    if (GdiplusStartup(&g_gdiplusToken, &gpInput, nullptr) != Ok) return 1;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = instance;
    wc.hCursor = nullptr;
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"AuroraLauncherWindow";
    wc.hIcon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0,
                                 LR_DEFAULTSIZE);
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    UINT dpi = 96;
    if (auto getDpi = (UINT(WINAPI*)())GetProcAddress(GetModuleHandleW(L"user32.dll"),
                                                      "GetDpiForSystem")) {
        dpi = getDpi();
    }
    g_scale = dpi / 96.0f;
    int w = (int)(options.width * g_scale);
    int h = (int)(options.height * g_scale);
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    if (y < 0) y = 0;

    g_hwnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, options.title.c_str(),
                             WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
                                 WS_CLIPCHILDREN,
                             x, y, w, h, nullptr, nullptr, instance, nullptr);
    if (!g_hwnd) {
        GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    g_cursorArrow = LoadCursorW(nullptr, IDC_ARROW);
    g_cursorHand = LoadCursorW(nullptr, IDC_HAND);
    g_cursorBeam = LoadCursorW(nullptr, IDC_IBEAM);

    applyDwm(g_hwnd);
    if (options.maximized) ShowWindow(g_hwnd, SW_SHOWMAXIMIZED);
    else ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    g_startTick = GetTickCount64();
    SetTimer(g_hwnd, 1, 33, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_gdiplusToken) GdiplusShutdown(g_gdiplusToken);
    g_gdiplusToken = 0;
    g_hwnd = nullptr;
    return 0;
}

} // namespace aurora::ui
