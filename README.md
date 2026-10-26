# Aurora Launcher

Launcher Minecraft pour Windows, écrit en C++20 avec Dear ImGui et DirectX 11.

Interface sombre moderne, gestion des instances, installation des versions, mods
via Modrinth, serveurs, console intégrée, réglages mémoire/Java et mises à jour
automatiques — sans télémétrie.

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
- **Mises à jour** : manifeste `update-version.json` lu à chaque démarrage,
  téléchargement vérifié SHA-256, installation par `AuroraUpdater.exe`, relance.
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
- `build/AuroraUninstall.exe` — désinstalleur ;
- `build/AuroraUpdater.exe` — assistant de remplacement pour les mises à jour.

## Tester

```powershell
build\AuroraLauncherCli.exe --selftest
build\AuroraLauncher.exe --uitest --no-update-check --data-dir "$env:TEMP\aurora-test" --screenshot "$env:TEMP\aurora-test\log.txt"
```

Les tests du module de mise à jour sont hors ligne : comparaison de versions,
analyse du manifeste, SHA-256, copie locale vérifiée et refus d'un fichier
corrompu.

## Mises à jour

À chaque démarrage (si l'option est activée), le lanceur lit :

```text
https://raw.githubusercontent.com/xenoxre/Aurora-laucnher/main/update-version.json
```

Si la version du manifeste est identique ou plus ancienne, rien ne se passe.
Si elle est plus récente, une fenêtre propose la mise à jour. Le téléchargement
affiche une barre de progression, puis `AuroraUpdater.exe` attend la fermeture
du lanceur, remplace l'ancien exécutable, le supprime et relance la nouvelle
version.

### Format du manifeste

```json
{
  "schema": 1,
  "version": "2.0.0",
  "channel": "stable",
  "publishedAt": "2026-10-26",
  "notes": "Nouveautés de la version…",
  "launcher": {
    "url": "https://raw.githubusercontent.com/xenoxre/Aurora-laucnher/main/dist/AuroraLauncher.exe",
    "size": 3379712,
    "sha256": "empreinte-sha256-hexadécimale"
  },
  "updater": {
    "url": "https://raw.githubusercontent.com/xenoxre/Aurora-laucnher/main/dist/AuroraUpdater.exe",
    "size": 243200,
    "sha256": "empreinte-sha256-hexadécimale"
  }
}
```

Le champ `size` et le SHA-256 sont obligatoires. Les URL distantes doivent être
en HTTPS (HTTP n'est accepté que pour `localhost`, afin de tester). Une variable
d'environnement `AURORA_UPDATE_MANIFEST` permet de pointer vers un fichier local
ou une autre URL pendant les tests.

### Publier une version

1. Mettre à jour la version dans `src/version.h` et `CMakeLists.txt`.
2. Recompiler les quatre cibles.
3. Lancer le script de publication :

```powershell
.\tools\publish_update.ps1 -Version 2.0.1 -Notes "Corrections et nouveautés" -Push
```

Le script vérifie la cohérence des versions, copie les exécutables dans `dist/`,
calcule taille et SHA-256, écrit `update-version.json`, commit et pousse.

> **Première migration** : la version 1.0.1 ne contient pas encore le module de
> mise à jour. Il faut installer la 2.0.0 manuellement une fois en récupérant
> `dist/AuroraLauncher.exe`. Les versions suivantes se mettent à jour seules.

### Limites de sécurité

Le manifeste et les binaires sont servis en HTTPS depuis le dépôt, et chaque
binaire est vérifié par SHA-256 avant installation. Le dépôt GitHub reste
toutefois la racine de confiance : une compromission du compte GitHub permettrait
de publier un binaire malveillant. Les binaires ne sont pas signés
cryptographiquement.

## Structure

```text
src/
  app.cpp, app.h              état de l'application et actions
  main.cpp                    point d'entrée / CLI
  main_imgui.cpp              boucle ImGui + DX11
  core/                       config, HTTP, Java, installation, lancement, logs
  ui/                         thème, état partagé, pages ImGui, widgets
  update/                     manifeste, SHA-256, téléchargement, AuroraUpdater
third_party/                  ImGui, miniz et dépendances embarquées
dist/                         exécutables publiés pour les mises à jour
tools/publish_update.ps1      génération du manifeste et publication
```

## Feuille de route

- Duplication/renommage d'instances, vérification d'intégrité des fichiers,
  statistiques de temps de jeu.
- Signature Authenticode des exécutables.
- Canaux de mise à jour (stable/beta) et notes de version localisées.

Le détail de la v2.0 se trouve dans
[`correctif v2.0.md`](correctif%20v2.0.md) et celui de la v1.0.1 dans
[`correctif v1.01.md`](correctif%20v1.01.md).

## Licence

Projet distribué sous licence [Apache 2.0](LICENSE).
