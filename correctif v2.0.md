# Correctif v2.0 — module de mise à jour

Date : 26 octobre 2026

Cette version transforme Aurora Launcher en application capable de se mettre à
jour elle-même depuis son dépôt GitHub, sans télémétrie et sans service tiers.

## Nouveautés

### Vérification automatique

- Le lanceur lit `update-version.json` à chaque démarrage, deux secondes après
  l'ouverture, en arrière-plan (aucun blocage de l'interface).
- Si la version distante est identique ou inférieure, aucune fenêtre n'apparaît.
- Si elle est supérieure, une fenêtre propose la mise à jour avec les notes de
  version, la version actuelle, la nouvelle version et les boutons
  « Télécharger », « Plus tard » et « Ignorer cette version ».
- Une option dans **Paramètres → À propos** permet de désactiver la vérification
  au démarrage ou de vérifier manuellement.

### Téléchargement vérifié

- Téléchargement HTTPS avec barre de progression et annulation.
- Taille et SHA-256 obligatoires dans le manifeste.
- Le fichier n'est publié qu'après vérification complète ; un fichier corrompu
  est supprimé et n'est jamais exécuté.
- Les deux binaires (`AuroraLauncher.exe` et `AuroraUpdater.exe`) sont
  téléchargés dans `updates/` sous le dossier de données.

### Installation et relance

- `AuroraUpdater.exe` est un petit exécutable Win32 autonome (environ 240 Ko).
- Il attend la fermeture du lanceur (PID), déplace l'ancien exécutable en
  `.old`, installe le nouveau, supprime l'ancien puis relance le lanceur.
- En cas d'échec du remplacement, l'ancienne version est restaurée.
- Toutes les opérations sont tracées dans `update.log` à côté du lanceur.
- Le lanceur affiche un toast « Mise à jour installée » au démarrage suivant.

### Configuration

Deux nouvelles préférences sont conservées dans `config.json` :

```json
{
  "checkUpdates": true,
  "skippedUpdateVersion": ""
}
```

La migration est automatique : les anciennes configurations reçoivent les
valeurs par défaut.

## Manifeste

Nouveau fichier à la racine du dépôt : `update-version.json`.

```json
{
  "schema": 1,
  "version": "2.0.0",
  "channel": "stable",
  "publishedAt": "2026-10-26",
  "notes": "Nouveautés…",
  "launcher": {
    "url": "https://raw.githubusercontent.com/xenoxre/Aurora-laucnher/main/dist/AuroraLauncher.exe",
    "size": 3379712,
    "sha256": "…"
  },
  "updater": {
    "url": "https://raw.githubusercontent.com/xenoxre/Aurora-laucnher/main/dist/AuroraUpdater.exe",
    "size": 243200,
    "sha256": "…"
  }
}
```

Le manifeste accepte aussi `changelog` comme alias de `notes` et `minVersion`
pour une future migration minimale. Le champ `schema` doit valoir `1` et le
canal doit être `stable` (ou vide).

## Publication

Nouveau script : `tools/publish_update.ps1`.

```powershell
.\tools\publish_update.ps1 -Version 2.0.1 -Notes "Corrections" -Push
```

Il vérifie que `src/version.h` et `CMakeLists.txt` portent le même numéro,
copie les exécutables dans `dist/`, calcule les empreintes, écrit le manifeste,
puis commit et pousse. `dist/` est volontairement versionné pour que les liens
`raw.githubusercontent.com` restent stables.

## Interface

- Fenêtre modale dédiée aux mises à jour, cohérente avec le thème.
- Section « Mises à jour » dans **Paramètres → À propos** : version actuelle,
  nouvelle version, notes, progression, téléchargement, installation, option de
  vérification et bouton de vérification manuelle.
- États explicites : vérification, disponible, téléchargement, prêt, erreur,
  version ignorée.
- Les tests UI (`--uitest`) et les captures (`--screenshot`) désactivent la
  vérification automatique pour rester déterministes.

## Technique

- `src/update/updater.{h,cpp}` : comparaison de versions, analyse du manifeste,
  lecture locale/HTTPS, téléchargement vérifié, lancement de l'assistant.
- `src/update/updater_main.cpp` : assistant Win32 autonome, attente du PID,
  remplacement avec sauvegarde/restauration, relance, journal.
- `src/update/update_config.h` : URL du manifeste et surcharge
  `AURORA_UPDATE_MANIFEST`.
- `core/util` : SHA-256 via BCrypt.
- `core/http` : téléchargement avec taille et SHA-256 attendus.
- `core/config` : nouvelles préférences persistées.
- `App`, `Bridge`, pages Paramètres/Jouer : états et actions de mise à jour.
- CMake : cible `AuroraUpdater`, liaison `bcrypt`, version 2.0.0.

## Tests

- 69 tests hors ligne réussis (`--selftest`) : versions, manifeste, SHA-256,
  téléchargement local, refus de hash invalide, configuration.
- Test UI : navigation sur les six pages (`--uitest`), 6/6.
- Test de l'assistant : remplacement d'un faux exécutable, ancienne version
  supprimée, code de sortie 0, journal généré.
- Test de non-régression de compilation : GUI, CLI, désinstalleur et assistant.

## Limites connues

- La version 1.0.1 doit être remplacée manuellement une première fois.
- Les binaires ne sont pas signés Authenticode.
- GitHub Raw peut mettre le manifeste en cache quelques minutes après un push.
- Le dépôt GitHub est la racine de confiance ; sa compromission reste un risque.
- Le mode `--no-update-check` est réservé aux tests et au dépannage.

## Fichiers principaux

```text
src/update/updater.h
src/update/updater.cpp
src/update/updater_main.cpp
src/update/update_config.h
src/app.cpp
src/app.h
src/core/config.cpp
src/core/config.h
src/ui/imgui_bridge.cpp
src/ui/imgui_bridge.h
src/ui/imgui_ui.cpp
src/ui/ui_state.h
src/ui/imgui_pages/settings.cpp
update-version.json
dist/AuroraLauncher.exe
dist/AuroraUpdater.exe
tools/publish_update.ps1
```
