#include "ui/ui_theme.h"

#include "core/util.h"
#include "ui/components.h"

#include <cmath>

namespace aurora::ui::theme {

const ImVec4 Accent = ImVec4(0.32f, 0.86f, 0.80f, 1.00f);
const ImVec4 Primary = ImVec4(0.24f, 0.48f, 0.98f, 1.00f);
const ImVec4 Danger = ImVec4(0.92f, 0.36f, 0.38f, 1.00f);
const ImVec4 Warn = ImVec4(1.00f, 0.76f, 0.32f, 1.00f);
const ImVec4 Ok = ImVec4(0.36f, 0.90f, 0.62f, 1.00f);

namespace {
float g_dpi = 1.0f;
ImFont* g_body = nullptr;
ImFont* g_title = nullptr;
} // namespace

float Dpi() { return g_dpi > 0.0f ? g_dpi : 1.0f; }
void SetDpi(float dpi) { g_dpi = dpi > 0.0f ? dpi : 1.0f; }

std::string U(const std::wstring& text) { return wideToUtf8(text); }

void SetFonts(ImFont* body, ImFont* title) {
    g_body = body;
    g_title = title ? title : body;
}
ImFont* BodyFont() { return g_body; }
ImFont* TitleFont() { return g_title; }

void Apply() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0.0f;
    s.ChildRounding = 12.0f;
    s.FrameRounding = 9.0f;
    s.PopupRounding = 12.0f;
    s.GrabRounding = 8.0f;
    s.TabRounding = 9.0f;
    s.ScrollbarRounding = 9.0f;

    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;
    s.PopupBorderSize = 1.0f;

    s.WindowPadding = ImVec2(12, 12);
    s.FramePadding = ImVec2(12, 8);
    s.ItemSpacing = ImVec2(10, 9);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.ScrollbarSize = 12.0f;
    s.GrabMinSize = 12.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = ImVec4(0.90f, 0.94f, 0.99f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.60f, 0.72f, 1.00f);
    c[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.070f, 0.100f, 1.00f);
    c[ImGuiCol_ChildBg] = ImVec4(0.082f, 0.104f, 0.148f, 1.00f);
    c[ImGuiCol_PopupBg] = ImVec4(0.086f, 0.108f, 0.152f, 0.98f);
    c[ImGuiCol_Border] = ImVec4(0.155f, 0.205f, 0.285f, 1.00f);
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = ImVec4(0.118f, 0.150f, 0.205f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.160f, 0.205f, 0.280f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.190f, 0.245f, 0.340f, 1.00f);
    c[ImGuiCol_TitleBg] = ImVec4(0.055f, 0.070f, 0.100f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.070f, 0.088f, 0.126f, 1.00f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.070f, 0.088f, 0.126f, 1.00f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.060f, 0.076f, 0.104f, 1.00f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.200f, 0.262f, 0.360f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.262f, 0.334f, 0.450f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.310f, 0.396f, 0.530f, 1.00f);
    c[ImGuiCol_CheckMark] = Accent;
    c[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.58f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.44f, 0.68f, 1.00f, 1.00f);
    c[ImGuiCol_Button] = ImVec4(0.152f, 0.200f, 0.278f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.212f, 0.282f, 0.396f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.262f, 0.352f, 0.500f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.180f, 0.262f, 0.392f, 1.00f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.230f, 0.330f, 0.480f, 1.00f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.270f, 0.386f, 0.560f, 1.00f);
    c[ImGuiCol_Separator] = ImVec4(0.155f, 0.205f, 0.285f, 1.00f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.260f, 0.400f, 0.620f, 1.00f);
    c[ImGuiCol_SeparatorActive] = ImVec4(0.320f, 0.500f, 0.760f, 1.00f);
    c[ImGuiCol_ResizeGrip] = ImVec4(0.180f, 0.240f, 0.340f, 0.60f);
    c[ImGuiCol_Tab] = ImVec4(0.094f, 0.118f, 0.160f, 1.00f);
    c[ImGuiCol_TabHovered] = ImVec4(0.190f, 0.270f, 0.400f, 1.00f);
    c[ImGuiCol_TabSelected] = ImVec4(0.200f, 0.320f, 0.500f, 1.00f);
    c[ImGuiCol_TabDimmed] = ImVec4(0.086f, 0.104f, 0.140f, 1.00f);
    c[ImGuiCol_TabDimmedSelected] = ImVec4(0.150f, 0.220f, 0.340f, 1.00f);
    c[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.62f, 0.90f, 1.00f);
    c[ImGuiCol_TableHeaderBg] = ImVec4(0.105f, 0.132f, 0.180f, 1.00f);
    c[ImGuiCol_TableBorderStrong] = ImVec4(0.155f, 0.205f, 0.285f, 1.00f);
    c[ImGuiCol_TableBorderLight] = ImVec4(0.115f, 0.145f, 0.195f, 1.00f);
    c[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(0.10f, 0.13f, 0.18f, 0.35f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.24f, 0.48f, 0.98f, 0.45f);
    c[ImGuiCol_NavCursor] = Accent;
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.02f, 0.03f, 0.05f, 0.65f);

    s.ScaleAllSizes(Dpi());
}

void Title(const char* text, const char* subtitle) {
    if (ImFont* f = TitleFont()) ImGui::PushFont(f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.98f, 1.00f, 1.00f));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    if (TitleFont()) ImGui::PopFont();
    if (subtitle && subtitle[0]) ImGui::TextDisabled("%s", subtitle);
    ImGui::Spacing();
}

void Hint(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.63f, 0.75f, 1.00f));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

void InfoRow(const char* label, const std::string& value) {
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(190.0f * Dpi());
    ImGui::TextUnformatted(value.c_str());
}

void SpinnerText(const char* text) {
    const char frames[] = "|/-\\";
    const int idx = (int)(ImGui::GetTime() * 8.0) & 3;
    ImGui::TextDisabled("%c  %s", frames[idx], text);
}

bool SameLineFits(float width) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    const float next = ImGui::GetItemRectMax().x + style.ItemSpacing.x + width;
    if (next <= right + 0.5f) {
        ImGui::SameLine();
        return true;
    }
    return false;
}

bool DangerButton(const char* label, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.42f, 0.16f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.58f, 0.22f, 0.26f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.18f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.90f, 0.90f, 1.0f));
    const bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return pressed;
}

void EmptyState(const char* title, const char* description, const char* actionText, bool* action) {
    PushCardStyle();
    ImGui::BeginChild(title, ImVec2(0, 0), true);
    ImGui::Dummy(ImVec2(0, 10.0f * Dpi()));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.86f, 0.94f, 1.0f));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::TextDisabled("%s", description);
    if (actionText && action) {
        ImGui::Spacing();
        if (PrimaryButton(actionText, 0.0f, 36.0f * Dpi())) *action = true;
    }
    ImGui::EndChild();
    PopCardStyle();
}

void NavIcon(ImDrawList* dl, ImVec2 c, float s, int kind, ImU32 col) {
    const float t = 2.0f * Dpi();
    switch (kind) {
    case 0: // play
        dl->AddTriangleFilled(ImVec2(c.x - s * 0.34f, c.y - s * 0.50f),
                              ImVec2(c.x - s * 0.34f, c.y + s * 0.50f),
                              ImVec2(c.x + s * 0.54f, c.y), col);
        break;
    case 1: // versions: overlapping squares
        dl->AddRect(ImVec2(c.x - s * 0.55f, c.y - s * 0.50f),
                    ImVec2(c.x + s * 0.15f, c.y + s * 0.10f), col, 3.0f, 0, t);
        dl->AddRectFilled(ImVec2(c.x - s * 0.10f, c.y - s * 0.06f),
                          ImVec2(c.x + s * 0.55f, c.y + s * 0.52f), col, 3.0f);
        break;
    case 2: { // mods: four blocks
        const float q = s * 0.32f, gap = s * 0.13f;
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                ImVec2 a(c.x - q - gap * 0.5f + i * (2 * q + gap),
                         c.y - q - gap * 0.5f + j * (2 * q + gap));
                dl->AddRectFilled(a, ImVec2(a.x + q, a.y + q), col, 2.5f);
            }
        }
        break;
    }
    case 3: // servers: stacked bars
        for (int i = -1; i <= 1; ++i) {
            const float y = c.y + i * s * 0.36f;
            dl->AddRectFilled(ImVec2(c.x - s * 0.50f, y - s * 0.11f),
                              ImVec2(c.x + s * 0.50f, y + s * 0.11f), col, 3.0f);
        }
        break;
    case 4: // console: terminal with prompt
        dl->AddRect(ImVec2(c.x - s * 0.55f, c.y - s * 0.45f),
                    ImVec2(c.x + s * 0.55f, c.y + s * 0.45f), col, 3.0f, 0, t);
        dl->AddLine(ImVec2(c.x - s * 0.24f, c.y - s * 0.14f),
                    ImVec2(c.x - s * 0.04f, c.y + s * 0.04f), col, t);
        dl->AddLine(ImVec2(c.x - s * 0.04f, c.y + s * 0.04f),
                    ImVec2(c.x - s * 0.24f, c.y + s * 0.22f), col, t);
        dl->AddLine(ImVec2(c.x + s * 0.06f, c.y + s * 0.22f),
                    ImVec2(c.x + s * 0.28f, c.y + s * 0.22f), col, t);
        break;
    case 5: { // settings: gear
        dl->AddCircle(c, s * 0.26f, col, 14, t);
        dl->AddCircleFilled(c, s * 0.07f, col, 8);
        for (int i = 0; i < 8; ++i) {
            const float a = i * 3.14159265f / 4.0f;
            dl->AddLine(ImVec2(c.x + cosf(a) * s * 0.33f, c.y + sinf(a) * s * 0.33f),
                        ImVec2(c.x + cosf(a) * s * 0.52f, c.y + sinf(a) * s * 0.52f), col, t);
        }
        break;
    }
    default:
        break;
    }
}

void BrandMark(ImDrawList* dl, ImVec2 c, float s) {
    const ImU32 accent = ImGui::ColorConvertFloat4ToU32(Accent);
    dl->AddQuadFilled(ImVec2(c.x, c.y - s * 0.55f), ImVec2(c.x + s * 0.55f, c.y),
                      ImVec2(c.x, c.y + s * 0.55f), ImVec2(c.x - s * 0.55f, c.y), accent);
    dl->AddQuadFilled(ImVec2(c.x, c.y - s * 0.24f), ImVec2(c.x + s * 0.24f, c.y),
                      ImVec2(c.x, c.y + s * 0.24f), ImVec2(c.x - s * 0.24f, c.y),
                      IM_COL32(16, 22, 32, 255));
}

} // namespace aurora::ui::theme
