#pragma once

#include "imgui.h"
#include <string>

namespace aurora::ui {

void PushCardStyle();
void PopCardStyle();
bool CardHeader(const char* title, const char* subtitle = nullptr);
bool ActionButton(const char* label, float width = 0.0f, float height = 38.0f);
bool PrimaryButton(const char* label, float width = 0.0f, float height = 42.0f);
bool GhostButton(const char* label, float width = 0.0f, float height = 38.0f);
void Badge(const char* text, const ImVec4& col = ImVec4(0.32f, 0.92f, 0.82f, 1.00f));
void SectionTitle(const char* title, const char* desc = nullptr);
void Divider();

} // namespace aurora::ui
