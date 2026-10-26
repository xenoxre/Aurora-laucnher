#pragma once
// Aurora Launcher - page renderers.
//
// Every page receives the UI bridge (which owns a snapshot of the application
// state) and draws itself with Dear ImGui. Pages never block: long operations
// are dispatched through Bridge and reflected via BridgeState.

namespace aurora::ui {

class Bridge;

void DrawPlay(Bridge* bridge);
void DrawVersions(Bridge* bridge);
void DrawMods(Bridge* bridge);
void DrawServers(Bridge* bridge);
void DrawConsole(Bridge* bridge);
void DrawSettings(Bridge* bridge);

} // namespace aurora::ui
