#pragma once
// Emplacement du manifeste de mise a jour (GitHub Releases).
//
// Depot : https://github.com/xenoxre/Aurora-laucnher
//
// Publie une release contenant AuroraLauncher.exe, AuroraUpdater.exe et
// latest.json (voir la feuille de route v2.0). L'URL ci-dessous pointe
// toujours vers l'asset latest.json de la derniere release publiee.
//
// Pour les tests locaux, definis AURORA_UPDATE_MANIFEST (variable
// d'environnement) vers un fichier latest.json ou une URL HTTPS.

#define AURORA_UPDATE_MANIFEST_URL \
    L"https://github.com/xenoxre/Aurora-laucnher/releases/latest/download/latest.json"
