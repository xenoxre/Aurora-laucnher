#include "ui/components.h"
#include "imgui.h"
#include "core/util.h"

#include <string>

using namespace aurora;

namespace aurora::ui {

void PushCardStyle() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.082f, 0.104f, 0.148f, 1.00f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 16));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
}

void PopCardStyle() {
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(1);
}

void SectionTitle(const char* title, const char* desc) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.98f, 1.00f, 1.00f));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    if (desc && desc[0]) {
        ImGui::TextDisabled("%s", desc);
    }
    ImGui::Spacing();
    ImGui::Spacing();
}

void Divider() {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

bool ActionButton(const char* label, float width, float height) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.168f, 0.226f, 0.318f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.226f, 0.302f, 0.426f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.296f, 0.400f, 0.564f, 1.00f));
    bool pressed = ImGui::Button(label, ImVec2(width, height));
    ImGui::PopStyleColor(3);
    return pressed;
}

bool PrimaryButton(const char* label, float width, float height) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.48f, 0.98f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.32f, 0.58f, 1.00f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.52f, 0.96f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.99f, 1.00f, 1.00f));
    bool pressed = ImGui::Button(label, ImVec2(width, height));
    ImGui::PopStyleColor(4);
    return pressed;
}

bool GhostButton(const char* label, float width, float height) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.24f, 0.36f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.34f, 0.50f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.30f, 0.44f, 0.80f));
    bool pressed = ImGui::Button(label, ImVec2(width, height));
    ImGui::PopStyleColor(4);
    return pressed;
}

void Badge(const char* text, const ImVec4& col) {
    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.06f, 0.08f, 0.12f, 1.00f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
    ImGui::SmallButton(text);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

bool CardHeader(const char* title, const char* subtitle) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.98f, 1.00f, 1.00f));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    if (subtitle && subtitle[0]) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.0f);
        ImGui::TextDisabled("%s", subtitle);
    }
    ImGui::Spacing();
    return true;
}

} // namespace aurora::ui
