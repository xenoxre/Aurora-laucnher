#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace aurora {
class App;
}

namespace aurora::ui {

void InitImGui(void* hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
void ShutdownImGui();
void NewFrameImGui();
void RenderImGui(App* app);
void ResizeImGui();
void SetStartPage(const wchar_t* page);
const wchar_t* CurrentPage();
bool GetNavButtonCenter(int index, float* x, float* y);

} // namespace aurora::ui
