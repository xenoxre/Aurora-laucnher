#include "ui/ui_state.h"

#include "ui/imgui_bridge.h"

namespace aurora::ui {

UiState& State() {
    static UiState state;
    return state;
}

void Navigate(Bridge* bridge, const wchar_t* page) {
    if (!page) return;
    State().page = page;
    if (bridge) bridge->navigate(page);
}

void RequestConfirm(int action, const std::wstring& payload, const std::wstring& title,
                    const std::wstring& message) {
    UiState& s = State();
    s.confirm.request = true;
    s.confirm.action = action;
    s.confirm.payload = payload;
    s.confirm.title = title;
    s.confirm.message = message;
}

} // namespace aurora::ui
