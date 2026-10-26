#pragma once

#include "imgui.h"

namespace aurora::ui::imgui::widgets {

void Section(const char* title, const char* subtitle = nullptr);
bool Primary(const char* label, float w = 0.0f, float h = 42.0f);
bool Secondary(const char* label, float w = 0.0f, float h = 38.0f);
bool Ghost(const char* label, float w = 0.0f, float h = 38.0f);
void Badge(const char* text, const ImVec4& col);
void CardBegin(const char* id, float w = 0.0f, float h = 0.0f);
void CardEnd();
bool NavItem(const char* icon, const char* label, bool active);
void InfoRow(const char* key, const char* value);

} // namespace aurora::ui::imgui::widgets
