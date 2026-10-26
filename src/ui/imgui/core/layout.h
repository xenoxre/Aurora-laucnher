#pragma once

#include "../imgui_bridge.h"
#include "../../app.h"

namespace aurora::ui::imgui {

struct FrameContext {
    Bridge* bridge = nullptr;
    App* app = nullptr;
    float deltaTime = 0.016f;
    float width = 0.0f;
    float height = 0.0f;
};

void DrawSidebar(const FrameContext& ctx);
void DrawTopbar(const FrameContext& ctx);
void DrawPage(const FrameContext& ctx, const wchar_t* page);
void DrawOverlay(const FrameContext& ctx);

} // namespace aurora::ui::imgui
