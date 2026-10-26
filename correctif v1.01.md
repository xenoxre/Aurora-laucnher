# Correctif v1.01 — Aurora Launcher

**Version :** 1.0.1
**Date :** 26 octobre 2026
**Périmètre :** interface graphique ImGui, gestion de la mémoire/Java, corrections du pont UI ↔ application.

---

## Résumé

Cette version regroupe trois chantiers :

1. **Interface graphique entièrement restructurée** : thème unifié, icônes vectorielles, navigation latérale, six pages cohérentes et états de chargement/erreur/vide soignés.
2. **Configuration mémoire et Java fiabilisée** : une seule source de vérité pour `-Xmx`/`-Xms`, détection de la mémoire physique, recommandation sûre et avertissements explicites.
3. **Corrections de bugs** dans le pont UI (désinstallation, suppression d'instance, création d'instance, filtres de console, toasts).

Le lanceur, la CLI et le désinstalleur compilent ; les tests internes et le test d'interface passent.

---

## 1. Interface graphique

### Structure du code

L'ancien fichier monolithique `src/ui/imgui_ui.cpp` a été découpé en modules :

| Fichier | Rôle |
| --- | --- |
| `src/ui/imgui_ui.cpp` | Contexte ImGui (Win32 + DX11), sidebar, dialogues, toasts, racine |
| `src/ui/ui_theme.h/.cpp` | Palette, typographie, DPI, icônes vectorielles, helpers de mise en page |
| `src/ui/ui_state.h/.cpp` | État partagé entre pages (page courante, dialogues, confirmation) |
| `src/ui/pages.h` | Déclarations des pages |
| `src/ui/imgui_pages/play.cpp` | Page Jouer |
| `src/ui/imgui_pages/versions.cpp` | Page Versions |
| `src/ui/imgui_pages/mods.cpp` | Page Mods |
| `src/ui/imgui_pages/servers.cpp` | Page Serveurs |
| `src/ui/imgui_pages/console.cpp` | Page Console |
| `src/ui/imgui_pages/settings.cpp` | Page Paramètres (onglets) |

### Thème et identité visuelle

- Palette sombre cohérente (fond, cartes, bordures, accents cyan/bleu) appliquée aux widgets ImGui : boutons, onglets, tables, scrollbars, sliders, popups.
- Police système **Segoe UI** chargée depuis Windows (aucune ressource embarquée, aucun téléchargement), avec taille adaptée au DPI et repli sur la police ImGui par défaut.
- **Icônes vectorielles dessinées à la main** (triangle « Jouer », calques « Versions », blocs « Mods », barres « Serveurs », terminal « Console », engrenage « Paramètres ») : aucun emoji, aucun logo tiers, aucune police d'icônes externe.
- Interface **DPI-aware** : détection via `GetDpiForWindow`, mise à l'échelle de la typographie et de toutes les métriques.
- Navigation clavier activée (`ImGuiConfigFlags_NavEnableKeyboard`).

### Navigation et pages

- Sidebar avec logo, version, entrées de navigation, compte actif et état du jeu/serveur local.
- **Jouer** : sélection d'instance, résumé (version, chargeur, mémoire allouée), lancement, création d'instance, accès aux versions, suppression avec confirmation.
- **Versions** : filtres (toutes/releases/snapshots/anciennes), recherche, tableau virtualisé (`ImGuiListClipper`), installation, lancement, désinstallation confirmée, états de chargement et d'erreur du manifeste.
- **Mods** : choix de l'instance cible, recherche Modrinth, résultats avec auteur/téléchargements, installation, états vide/chargement/erreur.
- **Serveurs** : ping (MOTD, joueurs, version, latence), connexion, liste des serveurs locaux, démarrage/arrêt, création d'un serveur (nom, version, port, RAM).
- **Console** : filtres (tout/info/avertissements/erreurs/jeu), défilement automatique, effacement, ouverture du dossier des journaux, coloration par niveau.
- **Paramètres** : onglets Général, Java, Comptes, Réseau, À propos.

### Qualité d'usage

- Confirmations explicites pour les actions destructrices (désinstallation d'une version, suppression d'une instance, retrait d'un compte, arrêt d'un serveur).
- Toasts et statuts de tâches en surimpression, **non cliquables** (`NoInputs`) pour ne jamais bloquer l'interface.
- États vides, chargements animés et messages d'erreur sur chaque page.
- Libellés et aides en français, cohérents avec le reste du lanceur.

---

## 2. Mémoire et Java

### Mémoire allouée (Xmx)

- Affichage exact en **Mo**, avec équivalent en **Go**, champ numérique éditable et curseur.
- Bornes : **512 Mo minimum, 131072 Mo maximum**, appliquées dans l'UI, la configuration et au lancement.
- Détection de la mémoire physique et disponible via `GlobalMemoryStatusEx` (`SystemMemoryInfo` / `getSystemMemoryInfo`).
- **Recommandation** calculée à partir de la RAM totale (≈ 40 %), plafonnée par une réserve système (2 Go, ou 4 Go au-delà de 16 Go) et par la mémoire disponible.
- Avertissements :
  - valeur supérieure au plafond sûr ;
  - valeur supérieure à la mémoire actuellement disponible ;
  - valeur inférieure à 1 Go pour les versions modernes.
- Explication explicite : **Xmx est le plafond du tas Java, pas la mémoire totale du processus** (JVM, bibliothèques natives, graphismes).
- Arguments JVM personnalisés éditables ; les `-Xmx`/`-Xms` qui y figurent sont **ignorés** pour éviter les doublons et les conflits.
- La mémoire des serveurs locaux est également bornée (minimum 512 Mo).

### Ligne de commande Java

- Une seule paire `-Xmx`/`-Xms` est générée par le lanceur.
- `Xms = max(512, Xmx / 4)` et `Xms <= Xmx`.
- Les `-Xmx`/`-Xms` issus du manifeste ou des arguments personnalisés sont filtrés ; les arguments utilisateur sont ajoutés en dernier.
- La configuration existante reste compatible : aucune migration destructive, les valeurs hors bornes sont simplement ramenées dans l'intervalle autorisé.

### Onglet Java

- Mode **Automatique (recommandé)** ou sélection manuelle d'une installation détectée.
- Liste des Java détectés avec version, chemin et version majeure.
- Bouton de détection à la demande.
- Rappel de compatibilité : Java 8 (≤ 1.16), Java 17 (1.17 → 1.20), Java 21 (1.20.5+).

---

## 3. Corrections apportées

- **Désinstallation d'une version** : le pont UI appelle désormais `App::uninstallVersion` (état, verrous et notifications corrects) au lieu de supprimer le dossier directement.
- **Suppression d'instance** : passe par `deleteInstance` du cœur, efface la sélection courante, enregistre la configuration et notifie le résultat.
- **Création d'instance** : l'index de version est résolu dans le manifeste complet ; il n'y a plus de décalage avec la liste filtrée des releases.
- **Toasts** : le drapeau d'erreur est réinitialisé à chaque rafraîchissement (une erreur passée ne colore plus les messages suivants).
- **Filtre de console** : le filtre est réellement persisté dans l'application (`Bridge::setLogFilter`) au lieu d'être perdu à chaque rafraîchissement.
- **Version** : passage à **1.0.1** dans `version.h`, CMake, `launcher_version`, `-Dminecraft.launcher.version` et le User-Agent Modrinth (valeurs désormais centralisées sur `AURORA_VERSION_W`).
- Nettoyage des scripts temporaires créés pendant le développement.

---

## 4. Fichiers

### Ajoutés

- `src/ui/ui_theme.h`, `src/ui/ui_theme.cpp`
- `src/ui/ui_state.h`, `src/ui/ui_state.cpp`
- `src/ui/pages.h`
- `src/ui/imgui_pages/play.cpp`
- `src/ui/imgui_pages/versions.cpp`
- `src/ui/imgui_pages/mods.cpp`
- `src/ui/imgui_pages/servers.cpp`
- `src/ui/imgui_pages/console.cpp`
- `src/ui/imgui_pages/settings.cpp`

### Modifiés

- `src/ui/imgui_ui.cpp` (réécrit : hôte ImGui, sidebar, dialogues, toasts)
- `src/ui/imgui_bridge.h`, `src/ui/imgui_bridge.cpp`
- `src/ui/components.h/.cpp` (conservés, réutilisés)
- `src/core/config.cpp` (bornes mémoire)
- `src/core/launch.cpp` (Xmx/Xms uniques, filtrage, version)
- `src/core/util.h/.cpp` (`getSystemMemoryInfo`)
- `src/core/modrinth.cpp` (User-Agent versionné)
- `src/version.h`
- `CMakeLists.txt` (nouveaux fichiers, version 1.0.1)

### Hérité / non compilé

- Les anciens fichiers de l'interface GDI+ (`ui.cpp`, `ui.h`, `window.cpp`, `dialogs.cpp`, `pages_*.cpp` hors `imgui_pages/`) ne font pas partie des cibles CMake. L'ancien `pages_play.cpp` a été remplacé par `src/ui/imgui_pages/play.cpp`. Ces fichiers peuvent être supprimés lors d'un prochain nettoyage.

---

## 5. Validation

- Compilation des trois cibles : `AuroraLauncher.exe`, `AuroraLauncherCli.exe`, `AuroraUninstall.exe`.
- `AuroraLauncherCli.exe --selftest` : **49 réussis, 0 échec**.
- `AuroraLauncher.exe --uitest` : **6/6** boutons de navigation valides (play, versions, mods, servers, console, settings).
- Captures des six pages vérifiées (rendu sans erreur) : Jouer, Versions, Mods, Serveurs, Console, Paramètres.
- `--version` renvoie bien `1.0.1`.

---

## 6. Limites connues

- La mémoire disponible affichée est un **instantané** au moment du rendu de la page ; elle n'est pas rafraîchie en continu.
- La recommandation mémoire est une heuristique prudente, pas une garantie de performance.
- `Xmx` ne représente toujours que le tas Java : la consommation totale du processus peut être supérieure.
- Aucune télémétrie ni appel réseau supplémentaire n'a été ajouté pour ces fonctionnalités.

---

## 7. Améliorations continues — 26 octobre 2026

### Mods

- **Pagination corrigée** : le curseur `modsOffset_` n'avançait jamais, ce qui aurait réaffiché la même page ; il progresse désormais du nombre de résultats reçus.
- Bouton **« Charger plus de résultats »** avec compteur « X sur Y » et nombre restant.
- Le total renvoyé par Modrinth est maintenant exposé dans l'interface (`BridgeState::modTotal`).

### Versions

- Nouveau filtre **« Installées »**.
- Compteur des versions affichées.
- Raccourci **« Dernière version »** en haut de page avec bouton Installer/Jouer.
- Bouton **« Dossier »** sur chaque version installée (avec infobulle).

### Console

- Champ de **filtre texte** sur les lignes (message ou source).
- Bouton **« Copier »** : copie les lignes affichées dans le presse-papiers.
- Compteur de lignes visibles.

### Serveurs

- Bouton **« Adresse »** : copie `localhost:<port>` dans le presse-papiers.
- Bouton **« Retirer »** pour enlever un serveur local de la liste, avec confirmation ; le serveur est arrêté s'il tournait et les fichiers restent sur le disque.

### Jouer

- Bouton **« Dossier »** pour ouvrir le répertoire de l'instance.
- Message explicite lorsque la version n'est pas encore installée (installation automatique au lancement).

### Paramètres

- Bouton **« Réinitialiser les arguments »** JVM.
- Onglet Réseau : affichage du **dossier de données** et accès direct.

### Ergonomie

- **Raccourcis clavier** : `Ctrl+1` à `Ctrl+6` pour naviguer, `F5` pour actualiser la page courante (manifeste, Java ou état général). Les raccourcis sont ignorés pendant la saisie de texte.
- Infobulles sur les entrées de navigation avec leur raccourci.
- Bouton **« Rafraîchir »** (F5) dans la barre latérale, avec la page courante comme cible.
- La boîte de confirmation place le focus par défaut sur **« Annuler »** (Entrée n'exécute jamais l'action destructive par accident).
- Arrêter un serveur local depuis sa fiche demande désormais aussi une confirmation (cohérence avec le bouton d'en-tête).

### Fenêtre et mise en page

- Taille de fenêtre minimale **1024 × 640** : évite les colonnes et boutons tronqués.
- La taille réelle de la fenêtre est **mémorisée à la fermeture** (y compris après redimensionnement, hors mode maximisé).
- Nouvelle aide `SameLineFits()` : les boutons passent automatiquement à la ligne quand la place manque, au lieu d'être coupés (Jouer, Serveurs, Console, Mods).
- Sur fenêtre étroite, la colonne Actions des versions bascule en menu **« ... »** (Jouer + dossier/désinstallation).
- Libellés de la console raccourcis (« Défilement auto », « Journaux ») pour tenir sur une ligne aux petites largeurs.

### Validation de cette passe

- Compilation des trois cibles.
- `--selftest` : 49/49.
- `--uitest` : 6/6.
- Captures Versions, Mods, Serveurs, Console, Paramètres régénérées sans erreur.
- Vérification de la restauration de taille : configuration 1100 × 700 → fenêtre 1100 × 700 (zone client 1084 × 661), taille conservée après fermeture.


