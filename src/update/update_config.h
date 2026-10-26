#pragma once
// Emplacement du manifeste de mise a jour.
//
// Depot : https://github.com/xenoxre/Aurora-laucnher
//
// Le fichier update-version.json est publie a la racine du depot (branche
// main). Il contient la version disponible et les exécutables hebergés dans
// dist/. Le lanceur verifie ce manifeste a chaque demarrage.
//
// Pour les tests locaux, definis AURORA_UPDATE_MANIFEST (variable
// d'environnement) vers un fichier update-version.json ou une URL HTTPS.

#define AURORA_UPDATE_MANIFEST_URL \
    L"https://raw.githubusercontent.com/xenoxre/Aurora-laucnher/main/update-version.json"
