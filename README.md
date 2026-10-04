# JamulusAccess

**English** · [Français](#français)

JamulusAccess is [Jamulus](https://jamulus.io), the free software for playing music live over the Internet, made fully accessible to blind and visually impaired musicians: NVDA, JAWS, Narrator and braille displays, keyboard only.

- The mixer is a list: one row per musician, Left and Right for Volume, Pan, Mute, Solo, Group, Level and Profile, Alt+Up and Alt+Down to change the value.
- The server list is read item by item, with the ping rated good, fair or too high.
- Chat messages are a list (Ctrl+2); musicians joining and leaving, the connection and the recording are announced.
- Ctrl+Shift+G connection status, keys 1 and 2 ping and delay, Ctrl+Shift+H all shortcuts, Shift+F1 explanation of the focused control.
- Peak Watcher (Ctrl+W, Ctrl+Shift+W): the peaks of your input above a threshold are spoken.
- The sound card is respected as in a DAW: no buffer or sample rate change unless you ask for it.
- Automatic updates from this repository's releases; manual on F1, changelog on Ctrl+F1.

JamulusAccess stays compatible with Jamulus: it plays on every Jamulus server, with musicians who use Jamulus.

**Download (Windows):** [JamulusAccessInstaller.exe](https://github.com/reaperaccessible/JamulusAccess/releases/latest/download/JamulusAccessInstaller.exe)

Please report JamulusAccess problems [here](https://github.com/reaperaccessible/JamulusAccess/issues), not to the Jamulus project.

### Building (Windows)

Visual Studio 2022 Build Tools (C++), Qt 6 msvc2022_64, the Steinberg ASIO SDK copied to `libs/ASIOSDK2` (never commit it), Inno Setup 6. Then:

```
powershell -ExecutionPolicy Bypass -File jamulusaccess\build.ps1
```

The build, the logs and the installer go next to the repository (`..\build`, `..\build_log`, `..\Output`). Other platforms: see [COMPILING.md](COMPILING.md). Contributors: read [CLAUDE.md](CLAUDE.md) first.

### Credits and licence

JamulusAccess is based on Jamulus by Volker Fischer and the Jamulus Development Team, who deserve all the credit for the software (original README: [README_Jamulus.md](README_Jamulus.md)). The accessibility work is by [ReaperAccessible](https://reaperaccessible.fr). Free software under the GNU Affero General Public License, version 3 or later ([COPYING](COPYING)).

---

## Français

JamulusAccess, c'est [Jamulus](https://jamulus.io), le logiciel libre pour jouer de la musique en direct sur Internet, rendu entièrement accessible aux musiciens aveugles et malvoyants : NVDA, JAWS, Narrateur et afficheurs braille, au clavier seulement.

- Le mixeur est une liste : une ligne par musicien, Flèche gauche et droite pour Volume, Panoramique, Muet, Solo, Groupe, Niveau et Profil, Alt+Flèche haut et bas pour changer la valeur.
- La liste des serveurs se lit information par information, avec le ping jugé bon, moyen ou trop élevé.
- Les messages du tchate sont une liste (Ctrl+2) ; l'arrivée et le départ des musiciens, la connexion et l'enregistrement sont annoncés.
- Ctrl+Maj+G l'état de la connexion, touches 1 et 2 le ping et le délai, Ctrl+Maj+H tous les raccourcis, Maj+F1 l'explication du contrôle qui a le focus.
- Peak Watcher (Ctrl+W, Ctrl+Maj+W) : les crêtes de votre entrée au-dessus d'un seuil sont dites.
- La carte son est respectée comme dans un DAW : aucun changement de tampon ou de fréquence sans votre demande.
- Mises à jour automatiques depuis ce dépôt ; manuel sur F1, journal des modifications sur Ctrl+F1.

JamulusAccess reste compatible avec Jamulus : il joue sur tous les serveurs Jamulus, avec des musiciens qui utilisent Jamulus.

**Télécharger (Windows) :** [JamulusAccessInstaller.exe](https://github.com/reaperaccessible/JamulusAccess/releases/latest/download/JamulusAccessInstaller.exe)

Merci de signaler les problèmes de JamulusAccess [ici](https://github.com/reaperaccessible/JamulusAccess/issues), et non au projet Jamulus.

### Mentions et licence

JamulusAccess est basé sur Jamulus, de Volker Fischer et de l'équipe de développement de Jamulus, à qui revient tout le mérite du logiciel. Le travail d'accessibilité est de [ReaperAccessible](https://reaperaccessible.fr). Logiciel libre sous licence publique générale GNU Affero, version 3 ou ultérieure ([COPYING](COPYING)).
