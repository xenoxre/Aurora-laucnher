#pragma once
// Aurora Launcher - visual identity shared by every ImGui page.
//
// Keeping colours, typography, icons and small layout helpers in one place
// makes the interface consistent and lets pages stay focused on behaviour.

#include <string>

#include "imgui.h"

namespace aurora::ui::theme {

// Palette ---------------------------------------------------------------------
extern const ImVec4 Accent;
extern const ImVec4 Primary;
extern const ImVec4 Danger;
extern const ImVec4 Warn;
extern const ImVec4 Ok;

// DPI -------------------------------------------------------------------------
float Dpi();
void SetDpi(float dpi);

// Fonts -----------------------------------------------------------------------
void SetFonts(ImFont* body, ImFont* title);
ImFont* BodyFont();
ImFont* TitleFont();

// Theme -----------------------------------------------------------------------
void Apply();

// Text helpers ----------------------------------------------------------------
std::string U(const std::wstring& text);
void Title(const char* text, const char* subtitle = nullptr);
void Hint(const char* text);
void InfoRow(const char* label, const std::string& value);
void SpinnerText(const char* text);

// Layout helper: keep a button on the current row only when it fits. Call it
// instead of ImGui::SameLine() before drawing an item of `width` pixels.
// Returns true when SameLine() was applied.
bool SameLineFits(float width);

// Buttons / containers --------------------------------------------------------
bool DangerButton(const char* label, const ImVec2& size);
void EmptyState(const char* title, const char* description, const char* actionText, bool* action);

// Vector artwork (no external icon font, no third-party assets) ---------------
void NavIcon(ImDrawList* dl, ImVec2 center, float size, int kind, ImU32 color);
void BrandMark(ImDrawList* dl, ImVec2 center, float size);

} // namespace aurora::ui::theme
