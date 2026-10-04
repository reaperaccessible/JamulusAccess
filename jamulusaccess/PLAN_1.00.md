# Jamulus accessible : plan des derniers ajouts

Plan du 3 octobre 2026, demandé par Lee. Les étapes se font dans cet ordre. Chaque étape
est testée hors écran (banc d'essai `build_test`), traduite en français, ajoutée à l'aide
Ctrl+Maj+H, puis livrée dans `build\release` pour l'essai de Lee, et enregistrée dans git.

Abandonné à la demande de Lee : l'annonce automatique des coupures audio (le voyant rouge
du tampon de gigue). L'information reste dans Ctrl+Maj+G.

## État au 3 octobre 2026

Toutes les étapes sont faites et testées hors écran (commits db145427 à 9a259e43), en attente
de l'essai de Lee. Ctrl+W va au Peak Watcher ; « Mon chariot en premier » est devenu
« Ma tranche en premier », sans raccourci, et « chariot » est devenu « tranche » (ou
« curseur ») partout en français. L'heure n'est pas dans l'annonce d'un message, seulement
dans la liste. Reste à entendre avec NVDA : le Peak Watcher (annonce « la plus récente »).

## 1. Maj+F1 : l'explication du contrôle qui a le focus

À l'écran, Jamulus a une aide « Qu'est-ce que c'est ? » pour presque chaque contrôle,
mais elle s'obtient avec Maj+F1 puis un clic de souris.

- Maj+F1 ouvre une petite boîte « Aide » avec l'explication du contrôle qui a le focus,
  en texte simple : on la lit avec les flèches et en braille. Échap la ferme et rend le
  focus au contrôle.
- Les contrôles qui n'en ont pas disent « Pas d'aide pour ce contrôle ».
- Le mixeur (liste) reçoit une aide qui reprend les explications de Jamulus pour le
  volume, le panoramique, le muet, le solo et les groupes.
- L'élément de menu « Qu'est-ce que c'est ? » (Maj+F1) fait la même chose.

## 2. La boîte des conditions du serveur

Certains serveurs demandent d'accepter des conditions. La boîte de Jamulus dit de les lire
dans le chat, mais elle bloque la fenêtre : impossible d'aller aux messages.

- La boîte affiche elle-même le texte des conditions (le message de bienvenue du serveur),
  dans une zone de lecture, avant la case « J'ai lu et j'accepte ».
- Ordre au Tab : le texte, la case, Accepter, Refuser.
- Si le texte arrive après l'ouverture de la boîte, il s'y ajoute.

## 3. Le Peak Watcher (observateur de crêtes), repris de GuitarAccess

Comme dans GuitarAccess :

- **Ctrl+W** ouvre une boîte « Peak Watcher » : un champ « Seuil en dB » (0 ou moins,
  -6 par défaut), avec OK et Annuler. OK enregistre le seuil et active l'observateur.
- **Ctrl+Maj+W** active ou désactive l'observateur : « Peak Watcher activé, seuil -6 dB »,
  « Peak Watcher désactivé ».
- Quand il est actif, toutes les 30 ms : si la crête de votre entrée depuis la dernière
  lecture dépasse le seuil, sa valeur est dite, par exemple « -3,2 ». Chaque nouvelle
  valeur coupe la précédente, pour qu'on entende toujours la dernière.
- Ce qui est mesuré : le son de votre entrée tel qu'il part vers le serveur, après
  l'amplification d'entrée, comme le vumètre de Jamulus. La mesure se fait dans le fil
  audio sans verrou ni allocation (règle de Jamulus pour le temps réel).
- Le seuil et l'état activé sont gardés dans les réglages de Jamulus.
- Pour couper la parole précédente, l'annonce passe par une notification UI Automation
  « la plus récente » (NVDA et JAWS), comme dans WingetAccess : Qt ne sait pas le faire.
- **À trancher par Lee :** dans Jamulus, Ctrl+W sert à « Mon chariot en premier » (menu
  Affichage). Proposition : le Peak Watcher prend Ctrl+W et Ctrl+Maj+W, comme dans toute
  la famille, et « Mon chariot en premier » reste dans le menu, sans raccourci.

## 4. Le débit envoyé

Les réglages audio affichent « Débit du flux audio : 657 kbps », un texte qu'on ne peut
pas atteindre au clavier.

- Ctrl+Maj+G le dit avec le reste de l'état : « Débit 657 kbps ».
- Dans les réglages, la valeur devient un champ en lecture seule, nommé
  « Débit du flux audio », atteignable au Tab.

## 5. L'avis de mise à jour

Jamulus affiche une minute en rouge « Nouvelle version disponible », en comparant avec la
version officielle de Jamulus, ce qui n'a pas de sens pour notre version.

- La vérification de mise à jour de Jamulus est désactivée dans notre version.
- Plus tard, notre propre mise à jour par GitHub, comme qBittorrentAccess (étape à part,
  avec le nom du produit et la première publication).

## 6. L'heure des messages

- Chaque message se lit avec son heure à la fin, après une virgule :
  « Paul: salut, 22:21 ». L'heure est celle qu'envoie le serveur, ramenée au format
  24 heures, sans les secondes.
- Ctrl+C copie toujours le texte seul, sans le nom ni l'heure.

## 7. La fenêtre du serveur

Pour qui fait tourner son propre serveur Jamulus sous Windows (pas les serveurs de Lee,
qui tournent sous Linux, sans fenêtre).

- Relevé complet au Tab, comme pour le client : liste des musiciens connectés,
  réglages (nom, ville, pays, annuaire, enregistrement, message de bienvenue), état.
- Corrections dans le même esprit : noms courts, listes lisibles en entier, annonces
  d'arrivée et de départ, Ctrl+Maj+H.

## Après ce plan

- Nom du produit, version 1.00, installateur, publication sur le GitHub de Lee.
- Serveur Linux : mise à jour et améliorations demandées par Lee.
