#pragma once
#include <windows.h>

#include <functional>
#include <string>

#include "ui.h"

namespace aurora::ui {

struct FrameHooks {
    std::function<void(Painter&, float, float)> paint;
    std::function<bool(float, float)> titleBarInteractive;
    std::function<bool()> needsFrames;
    std::function<void(float, float)> onResize;
    std::function<void()> onClose;
};

struct WindowOptions {
    std::wstring title = L"Aurora Launcher";
    int width = 1220;
    int height = 800;
    bool maximized = false;
    std::wstring screenshotPath;
    int screenshotDelayMs = 1500;
};

int runWindow(HINSTANCE instance, const WindowOptions& options, FrameHooks hooks);

void windowMinimize();
void windowMaximizeToggle();
void windowClose();
bool windowMaximized();
void windowInvalidate();
float windowScale();

} // namespace aurora::ui
