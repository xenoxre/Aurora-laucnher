#include "ui/imgui/widgets/widgets.h"
#include "core/util.h"

using namespace aurora;

namespace aurora::ui::imgui::widgets {

void Section(const char* title, const char* subtitle) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.98f, 1.00f, 1.00f));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    if (subtitle && subtitle[0]) {
        ImGui::TextDisabled("%s", subtitle);
    }
    ImGui::Spacing();
    ImGui::Spacing();
}

bool Primary(const char* label, float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.52f, 1.00f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.34f, 0.60f, 1.00f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.56f, 0.98f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.99f, 1.00f, 1.00f));
    bool r = ImGui::Button(label, ImVec2(w, h));
    ImGui::PopStyleColor(4);
    return r;
}

bool Secondary(const char* label, float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.172f, 0.232f, 0.326f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.232f, 0.312f, 0.438f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.300f, 0.408f, 0.576f, 1.00f));
    bool r = ImGui::Button(label, ImVec2(w, h));
    ImGui::PopStyleColor(3);
    return r;
}

bool Ghost(const char* label, float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.28f, 0.42f, 0.88f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.36f, 0.54f, 0.95f));
    bool r = ImGui::Button(label, ImVec2(w, h));
    ImGui::PopStyleColor(3);
    return r;
}

void Badge(const char* text, const ImVec4& col) {
    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.06f, 0.08f, 0.12f, 1.00f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 999.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));
    ImGui::SmallButton(text);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

void CardBegin(const char* id, float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.084f, 0.108f, 0.154f, 1.00f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22, 20));
    ImGui::BeginChild(id, ImVec2(w, h), true);
}

void CardEnd() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(1);
}

bool NavItem(const char* icon, const char* label, bool active) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.38f, 0.58f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.46f, 0.70f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.36f, 0.54f, 0.82f, 1.00f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.26f, 0.40f, 0.86f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.34f, 0.52f, 0.92f));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    char buf[160];
    snprintf(buf, sizeof(buf), "  %s   %s", icon, label);
    bool pressed = ImGui::Button(buf, ImVec2(-1, 46));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return pressed;
}

void InfoRow(const char* key, const char* value) {
    ImGui::TextDisabled("%s", key);
    ImGui::SameLine(160);
    ImGui::Text("%s", value);
}

} // namespace aurora::ui::imgui::widgets
