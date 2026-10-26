cmake_minimum_required(VERSION 3.20)
project(AuroraLauncher VERSION 2.0.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RC_COMPILER windres)
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

set(SOURCES_GUI
  src/main.cpp
  src/main_imgui.cpp
  src/app.cpp
  src/tests.cpp
  src/ui/imgui_ui.cpp
  src/ui/imgui_bridge.cpp
  src/ui/components.cpp
  src/ui/ui_state.cpp
  src/ui/ui_theme.cpp
  src/ui/imgui_pages/play.cpp
  src/ui/imgui_pages/versions.cpp
  src/ui/imgui_pages/mods.cpp
  src/ui/imgui_pages/servers.cpp
  src/ui/imgui_pages/console.cpp
  src/ui/imgui_pages/settings.cpp
  src/ui/imgui/widgets/widgets.cpp
  src/update/updater.cpp
  src/core/util.cpp
  src/core/json.cpp
  src/core/sha1.cpp
  src/core/log.cpp
  src/core/http.cpp
  src/core/zip.cpp
  src/core/tasks.cpp
  src/core/config.cpp
  src/core/nbt.cpp
  src/core/java.cpp
  src/core/meta.cpp
  src/core/install.cpp
  src/core/launch.cpp
  src/core/auth.cpp
  src/core/modrinth.cpp
  src/core/server.cpp
  src/core/instance.cpp
  src/core/process.cpp
  src/core/uninstall.cpp
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
  ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
  third_party/miniz.c
  resources/app.rc
)

add_executable(AuroraLauncher WIN32 ${SOURCES_GUI})

target_include_directories(AuroraLauncher PRIVATE
  src
  third_party
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
  resources
)

target_compile_definitions(AuroraLauncher PRIVATE
  UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
  _WIN32_WINNT=0x0A00 WINVER=0x0A00
  AURORA_VERSION="2.0.0"
  IMGUI_DEFINE_MATH_OPERATORS
)

target_compile_options(AuroraLauncher PRIVATE
  -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
  -finput-charset=UTF-8 -fexec-charset=UTF-8 -O2 -ffunction-sections -fdata-sections
)

target_link_options(AuroraLauncher PRIVATE
  -static -static-libgcc -static-libstdc++
  -Wl,--gc-sections -mwindows -municode -s
)

target_link_libraries(AuroraLauncher PRIVATE
  d3d11 dxgi d3dcompiler windowscodecs
  winhttp ws2_32 dwmapi shcore shlwapi dnsapi
  dbghelp ole32 shell32 comctl32 uxtheme user32 gdi32 advapi32 crypt32 bcrypt
)

set(CLI_SOURCES
  src/main.cpp
  src/app.cpp
  src/tests.cpp
  src/core/util.cpp
  src/core/json.cpp
  src/core/sha1.cpp
  src/core/log.cpp
  src/core/http.cpp
  src/core/zip.cpp
  src/core/tasks.cpp
  src/core/config.cpp
  src/core/nbt.cpp
  src/core/java.cpp
  src/core/meta.cpp
  src/core/install.cpp
  src/core/launch.cpp
  src/core/auth.cpp
  src/core/modrinth.cpp
  src/core/server.cpp
  src/core/instance.cpp
  src/core/process.cpp
  src/core/uninstall.cpp
  src/update/updater.cpp
  third_party/miniz.c
)

add_executable(AuroraLauncherCli ${CLI_SOURCES})

target_include_directories(AuroraLauncherCli PRIVATE src third_party resources)

target_compile_definitions(AuroraLauncherCli PRIVATE
  UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
  _WIN32_WINNT=0x0A00 WINVER=0x0A00
  AURORA_VERSION="2.0.0"
  AURORA_CLI=1
)

target_compile_options(AuroraLauncherCli PRIVATE
  -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
  -finput-charset=UTF-8 -fexec-charset=UTF-8 -O2
)

target_link_options(AuroraLauncherCli PRIVATE -static -static-libgcc -static-libstdc++ -municode)

target_link_libraries(AuroraLauncherCli PRIVATE
  winhttp ws2_32 dwmapi shcore shlwapi dnsapi
  dbghelp ole32 shell32 comctl32 uxtheme user32 gdi32 advapi32 crypt32 bcrypt
)


add_executable(AuroraUninstall WIN32 src/uninstall_main.cpp src/core/uninstall.cpp src/core/util.cpp)
target_include_directories(AuroraUninstall PRIVATE src third_party resources)
target_compile_definitions(AuroraUninstall PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_options(AuroraUninstall PRIVATE -static -static-libgcc -static-libstdc++ -mwindows -municode)
target_link_libraries(AuroraUninstall PRIVATE shell32 user32 bcrypt)

# Assistant de mise a jour : petit binaire sans dependance au reste du projet.
add_executable(AuroraUpdater WIN32 src/update/updater_main.cpp)
target_compile_definitions(AuroraUpdater PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(AuroraUpdater PRIVATE -Wall -Wextra -O2 -ffunction-sections -fdata-sections)
target_link_options(AuroraUpdater PRIVATE -static -static-libgcc -static-libstdc++ -Wl,--gc-sections -mwindows -municode -s)
target_link_libraries(AuroraUpdater PRIVATE shell32 user32)
