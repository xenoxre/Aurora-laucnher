# Aurora Launcher

Launcher Minecraft pour Windows, écrit en C++20 avec ImGui et DirectX 11.

Interface sombre moderne, gestion des instances, installation des versions, mods
via Modrinth, serveurs, console intégrée et réglages mémoire/Java — sans
télémétrie.

## Fonctionnalités

- **Jouer** : instances isolées, lancement rapide, dossier d'instance, état du jeu.
- **Versions** : catalogue Mojang, installation avec progression, filtre
  « Installées », raccourci vers la dernière version.
- **Mods** : recherche Modrinth, pagination, installation dans une instance.
- **Serveurs** : ping, ajout/suppression de serveurs, serveur local, copie de
  l'adresse `localhost:<port>`.
- **Console** : journaux en direct, filtre texte, copie dans le presse-papiers,
  défilement automatique.
- **Paramètres** : mémoire Xmx en Mo/Go avec slider et recommandation, arguments
  JVM, détection/compatibilité Java (8/17/21), comptes Microsoft, réseau, données.
- **Ergonomie** : raccourcis `Ctrl+1`…`Ctrl+6`, `F5`, tooltips, confirmations
  pour les actions destructives, toasts, interface responsive (minimum 1024×640).

## Prérequis

- Windows 10/11 64 bits.
- [CMake](https://cmake.org/) ≥ 3.20.
- MinGW-w64 (GCC 13) avec `mingw32-make`, ou tout compilateur C++20 compatible.
- Java 8, 17 ou 21 pour Minecraft (le launcher détecte les installations).

## Compiler

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

Sorties :

- `build/AuroraLauncher.exe` — interface graphique ;
- `build/AuroraLauncherCli.exe` — diagnostics et tests ;
- `build/AuroraUninstall.exe` — désinstalleur.

## Tester

```powershell
build\AuroraLauncherCli.exe --selftest
build\AuroraLauncher.exe --uitest --data-dir "$env:TEMP\aurora-test" --screenshot "$env:TEMP\aurora-test\log.txt"
```

## Structure

```text
src/
  app.cpp, app.h              état de l'application et actions
  main.cpp                    point d'entrée / CLI
  main_imgui.cpp              boucle ImGui + DX11
  core/                       config, HTTP, Java, installation, lancement, logs
  ui/                         thème, état partagé, pages ImGui, widgets
  update/                     noyau du module de mise à jour (v2.0, en cours)
third_party/                  ImGui, miniz et dépendances embarquées
```

## Feuille de route

- **v2.0** : module de mise à jour GitHub Releases (`latest.json` + SHA-256),
  assistant `AuroraUpdater.exe`, vérification asynchrone et UI dédiée.
- Duplication/renommage d'instances, vérification d'intégrité des fichiers,
  statistiques de temps de jeu.

Le détail des changements de la v1.0.1 se trouve dans
[`correctif v1.01.md`](correctif%20v1.01.md).

## Licence

Projet distribué sous licence [Apache 2.0](LICENSE).
