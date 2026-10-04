# JamulusAccess : instructions pour les contributeurs (et leurs agents)

JamulusAccess est Jamulus rendu entièrement accessible aux musiciens aveugles (NVDA, JAWS,
Narrateur, braille), au clavier seul. Dépôt : `reaperaccessible/JamulusAccess`, branche `main`.
Responsable : Lee (ReaperAccessible). Collaborateur : math65 (Mathieu Martin), qui s'occupe
aussi de la version Mac.

Le fichier `AGENTS.md` est celui du projet Jamulus d'origine : ses règles techniques
(temps réel audio, entrées réseau, fichiers générés) restent valables. Celles-ci les complètent.

## Règles d'accessibilité (non négociables)

- **Une seule voix par événement.** Quand le focus bouge, c'est le lecteur d'écran qui parle :
  le programme n'ajoute rien. Si le programme doit parler, il parle AVANT de déplacer le focus,
  jamais par une minuterie qui court après le lecteur.
- Annonces **brèves** (« Muet activé », pas « Le musicien a été mis en sourdine ») ; la valeur
  lue doit rester sur l'afficheur braille (texte de la ligne, nom ou valeur du contrôle).
- Listes : « First, » / « Last, » aux extrémités (`Access::installListEdges`).
- Listes de paramètres de la famille : Flèche gauche/droite = paramètre,
  Alt+Flèche haut/bas = valeur, Alt+Maj = petit pas.
- Ctrl+Maj+H = liste des raccourcis (à tenir à jour), Maj+F1 = explication du contrôle,
  F1 = manuel, Ctrl+F1 = journal des modifications. Ctrl+1 = session, Ctrl+2 = messages.
- Rien ne doit reconfigurer la carte son sans que l'utilisateur le demande (la voix du lecteur
  d'écran passe souvent par la même carte).
- Les listes déroulantes changent normalement avec les flèches, SAUF le périphérique audio
  (propriété `accessOpenToChange` : Alt+Flèche bas pour ouvrir, Entrée pour choisir).
- Pas d'abréviations dans ce qui est lu (« Volume », pas « Vol »).

Les outils communs sont dans `src/access/` (`accessibility.h` : announce, announceLatest,
labelControls, installSliderKeys, installCellReading…). S'en servir plutôt que réinventer.

## Langue

- Textes de l'interface en **anglais** dans le code (`tr ( "..." )`), traduits en français dans
  `src/translation/translation_fr_FR.ts`. Chaque nouveau texte doit être traduit :
  `lupdate src -extensions cpp,h,ui -no-obsolete -ts src\translation\translation_fr_FR.ts`,
  puis remplir les `type="unfinished"` (Qt Linguist ou à la main), zéro texte non traduit.
- Manuel et journal : `jamulusaccess/docs/*_fr.html` et `*_en.html`, toujours les deux.

## Compiler (Windows)

`powershell -ExecutionPolicy Bypass -File jamulusaccess\build.ps1` (ou `-NoInstaller`).
Il faut : Visual Studio 2022 Build Tools (C++), Qt 6 msvc2022_64 (`-QtDir` si ailleurs que
`C:\Qt\6.10.2\msvc2022_64`), le kit ASIO de Steinberg copié dans `libs\ASIOSDK2`
(**jamais committé** : licence Steinberg), Inno Setup 6. Tout ce qui est produit va à côté du
dépôt : `..\build`, `..\build_log`, `..\Output`. Les autres plateformes : `COMPILING.md`.

## Tester sans écran ni carte son

Version de test : dans `..\build_test`, `qmake ..\<dépôt>\JamulusAccess.pro CONFIG+=release CONFIG+=accesstest`
puis `nmake release` (fausse carte son, harnais `src/access/accesstest.cpp`). Puis
`jamulusaccess\tests\run_test.ps1 -Name <script>` : serveur local, faux musiciens Paul, Marie
(et Luc avec `-LateJoin`), JSON-RPC pour le tchate (`-Events`). Le journal dit ce que le lecteur
d'écran recevrait (focus, noms, annonces). Les scripts sont dans `jamulusaccess\tests\scripts`.
Mise à jour : `JAMULUSACCESS_UPDATE_API=http://127.0.0.1:<port>/latest.json` remplace GitHub
(script `upd1`). Toujours tester avant de committer ; Lee écoute ensuite avec NVDA.

## Versions et publication

- Versions **1.00, 1.01, 1.02…** (jamais 0.xx). Le numéro est dans `src/global.h` :
  `JAMULUSACCESS_VERSION`, `_MAJOR`, `_MINOR` (les trois ensemble ; `build.ps1` vérifie).
  `APP_VERSION` (le `VERSION` du .pro) reste celui de Jamulus : c'est ce que voient les serveurs.
- Pour publier : nouveau numéro dans `global.h`, une section en tête de
  `jamulusaccess/docs/changelog_fr.html` et `changelog_en.html`, le manuel si besoin,
  `build.ps1`, Lee teste l'installateur, puis :
  `gh release create vX.YY ..\Output\JamulusAccessInstaller.exe ..\Output\JamulusAccessInstaller_X.YY.exe --title "JamulusAccess X.YY" --notes-file notes.md`
  Les notes ont deux parties, chacune sur sa ligne : `[lang=fr]` puis `[lang=en]` ; la mise à
  jour automatique montre celle de la langue de l'utilisateur.
- La mise à jour automatique (`src/access/updater.cpp`) lit
  `api.github.com/repos/reaperaccessible/JamulusAccess/releases/latest` et cherche l'asset
  **`JamulusAccessInstaller.exe`** (nom exact) ; l'installateur est lancé en silence et relance
  le programme. Un tag sans installateur casse la mise à jour : ne jamais publier ainsi.
- Pas d'intégration continue sur GitHub (les workflows de Jamulus ont été retirés) : les
  installateurs se construisent sur la machine. Le Mac se fait à part (math65).

## Ce qu'on ne fait pas

- Aucune pull request ni issue vers `jamulussoftware/jamulus` sans l'accord de Lee.
- Ne pas casser la compatibilité réseau avec Jamulus (protocole, version annoncée).
- Les réglages de l'utilisateur (`AppData\Roaming\JamulusAccess`) ne sont jamais effacés.
- Pas de travail lourd, d'allocation ni de verrou dans le fil audio (`src/sound`,
  `src/socket.cpp`, `src/server.cpp`, `CClient::ProcessAudioDataIntern`).

Historique de la version 1.00 : `jamulusaccess/PLAN_1.00.md`.
