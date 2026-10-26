// Aurora Launcher - ImGui DX11 host window
#include "app.h"
#include "core/util.h"
#include "ui/imgui_ui.h"

#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <cstdio>
#include <string>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

using namespace aurora;

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_GETMINMAXINFO: {
        // Keep the layout usable: below this the sidebar and tables get cramped.
        MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = 1024;
        mmi->ptMinTrackSize.y = 640;
        return 0;
    }
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
                                      featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
                                      &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK) {
        return false;
    }
    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (SUCCEEDED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)))) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

static void EnableDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        typedef BOOL(WINAPI * SetCtxFn)(void*);
        auto setCtx = (SetCtxFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setCtx && setCtx((void*)-4)) return; // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    }
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
        typedef HRESULT(WINAPI * SetAwarenessFn)(int);
        auto setAwareness = (SetAwarenessFn)GetProcAddress(shcore, "SetProcessDpiAwareness");
        if (setAwareness && SUCCEEDED(setAwareness(2))) return;
    }
    SetProcessDPIAware();
}

static bool SaveBackBufferPng(const std::wstring& path, int width, int height) {
    ID3D11Texture2D* backBuffer = nullptr;
    if (FAILED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;

    D3D11_TEXTURE2D_DESC desc;
    backBuffer->GetDesc(&desc);
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.MiscFlags = 0;

    ID3D11Texture2D* staging = nullptr;
    if (FAILED(g_pd3dDevice->CreateTexture2D(&desc, nullptr, &staging))) {
        backBuffer->Release();
        return false;
    }
    g_pd3dDeviceContext->CopyResource(staging, backBuffer);
    backBuffer->Release();

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(g_pd3dDeviceContext->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
        staging->Release();
        return false;
    }

    bool ok = false;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IWICImagingFactory* factory = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&factory)))) {
        IWICBitmap* bitmap = nullptr;
        UINT size = mapped.RowPitch * height;
        if (SUCCEEDED(factory->CreateBitmapFromMemory((UINT)width, (UINT)height,
                                                      GUID_WICPixelFormat32bppBGRA,
                                                      mapped.RowPitch, size,
                                                      (BYTE*)mapped.pData, &bitmap))) {
            std::wstring target = path;
            if (target.size() < 4 || target.substr(target.size() - 4) != L".png") target += L".png";

            IWICStream* stream = nullptr;
            IWICBitmapEncoder* encoder = nullptr;
            IWICBitmapFrameEncode* frame = nullptr;
            if (SUCCEEDED(factory->CreateStream(&stream)) &&
                SUCCEEDED(stream->InitializeFromFilename(target.c_str(), GENERIC_WRITE)) &&
                SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
                SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
                SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) &&
                SUCCEEDED(frame->Initialize(nullptr))) {
                WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
                frame->SetSize((UINT)width, (UINT)height);
                frame->SetPixelFormat(&fmt);
                if (SUCCEEDED(frame->WriteSource(bitmap, nullptr)) &&
                    SUCCEEDED(frame->Commit()) &&
                    SUCCEEDED(encoder->Commit())) {
                    ok = true;
                }
            }
            if (frame) frame->Release();
            if (encoder) encoder->Release();
            if (stream) stream->Release();
            bitmap->Release();
        }
        factory->Release();
    }

    g_pd3dDeviceContext->Unmap(staging, 0);
    staging->Release();
    return ok;
}

namespace aurora {

int runImGuiMain(HINSTANCE hInstance, App* app) {
    EnableDpiAwareness();

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L, hInstance,
                       LoadIconW(hInstance, L"IDI_ICON1"), LoadCursorW(nullptr, IDC_ARROW),
                       nullptr, nullptr, L"AuroraImGui", nullptr };
    RegisterClassExW(&wc);

    int winW = 1280;
    int winH = 840;
    if (app && app->config_.windowW >= 1024 && app->config_.windowH >= 640) {
        winW = app->config_.windowW;
        winH = app->config_.windowH;
    }

    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Aurora Launcher", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
                              nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd || !CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    ui::InitImGui(hwnd, g_pd3dDevice, g_pd3dDeviceContext);
    if (app && !app->options_.startPage.empty()) ui::SetStartPage(app->options_.startPage.c_str());

    const std::wstring screenshotPath = app ? app->options_.screenshotPath : L"";
    const int screenshotDelay = app ? app->options_.screenshotDelayMs : 2200;
    const ULONGLONG started = GetTickCount64();
    bool shotDone = false;

    const bool uiTest = app && app->options_.uiTest;
    const wchar_t* testPages[6] = { L"play", L"versions", L"mods", L"servers", L"console", L"settings" };
    int testIndex = 0;
    int testPhase = 0;
    int testWait = 0;
    int testFails = 0;
    std::wstring testLog;
    int frameNo = 0;
    POINT testOrigCursor = {};
    GetCursorPos(&testOrigCursor);

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        ui::NewFrameImGui();
        ui::RenderImGui(app);
        ImGui::Render();

        const float clear_color[4] = { 0.055f, 0.070f, 0.100f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);

        // Remember the real window geometry (outer rect), but keep the restored
        // size when maximized so the next launch is not forced full-screen.
        if (app && !IsZoomed(hwnd)) {
            RECT wr = {};
            if (GetWindowRect(hwnd, &wr)) {
                app->lastW_ = (float)(wr.right - wr.left);
                app->lastH_ = (float)(wr.bottom - wr.top);
            }
        }

        frameNo++;
        if (uiTest && frameNo > 20) {
            if (testIndex >= 6) {
                if (!screenshotPath.empty()) {
                    std::string narrow = wideToUtf8(testLog);
                    FILE* f = nullptr;
                    _wfopen_s(&f, screenshotPath.c_str(), L"wb");
                    if (f) {
                        fwrite(narrow.data(), 1, narrow.size(), f);
                        fclose(f);
                    }
                }
                SetCursorPos(testOrigCursor.x, testOrigCursor.y);
                PostQuitMessage(testFails == 0 ? 0 : 2);
                continue;
            }

            float cx = 0, cy = 0;
            if (!ui::GetNavButtonCenter(testIndex, &cx, &cy)) {
                continue;
            }
            POINT pt = { (LONG)cx, (LONG)cy };
            ClientToScreen(hwnd, &pt);
            if (testPhase == 0) {
                SetForegroundWindow(hwnd);
                SetCursorPos(pt.x, pt.y);
                testPhase = 1;
                testWait = 0;
            } else if (testPhase == 1) {
                if (++testWait >= 3) {
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                    testPhase = 2;
                    testWait = 0;
                }
            } else if (testPhase == 2) {
                if (++testWait >= 3) {
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    testPhase = 3;
                    testWait = 0;
                }
            } else {
                if (++testWait >= 6) {
                    const wchar_t* expected = testPages[testIndex];
                    bool ok = wcscmp(ui::CurrentPage(), expected) == 0;
                    if (!ok) testFails++;
                    testLog += std::wstring(ok ? L"[OK]    " : L"[ECHEC] ") +
                               L"bouton " + std::to_wstring(testIndex) + L" -> " +
                               ui::CurrentPage() + L" (attendu " + expected + L")\n";
                    testIndex++;
                    testPhase = 0;
                    testWait = 0;
                }
            }
            continue;
        }

        if (!screenshotPath.empty() && !uiTest && !shotDone &&
            GetTickCount64() - started >= (ULONGLONG)screenshotDelay) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            SaveBackBufferPng(screenshotPath, rc.right - rc.left, rc.bottom - rc.top);
            shotDone = true;
            PostQuitMessage(0);
        }
    }

    ui::ShutdownImGui();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

} // namespace aurora
