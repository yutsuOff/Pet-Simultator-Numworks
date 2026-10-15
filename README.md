# Pet Simulator — NumWorks Edition

Adaptation 2D de Pet Simulator, en application native C (EADK) pour calculatrice NumWorks (`.nwa`).
Conception, maquettes et feuille de route : [docs/CONCEPTION.md](docs/CONCEPTION.md).

## État : v0.6 — zones 38 à 65 après le rebirth, 4 nouvelles machines (partie d'environ 10 h)

- **Contenu réel de PS99** (wiki fandom) : 78 œufs, les 65 zones qui les contiennent, 233 pets,
  avec leurs vraies images. Détails et sources : [data/README.md](data/README.md).
- **Farm** : objets de la zone actuelle ; OK cible un objet (et le frappe), les pets équipés
  l'attaquent tout seuls puis passent au suivant (AUTO).
- **Œufs** : liste des œufs (zones débloquées + aperçu de la suivante), pets et chances exactes,
  pets non découverts en silhouette, animation d'ouverture selon la rareté.
- **Pets** : inventaire (pets empilés ×N), détail, équipe de 5 (jusqu'à 8), barre d'outils :
  tri (puissance, rareté, nom, nombre), filtre (tous, équipés, doublons), « Meilleurs »,
  « Recycler » (vend les doublons contre des coins, garde 1 exemplaire et les équipés).
- **Améliorations** (diamants) : Pets (force, équipe), Farming (coup de poing, réapparition),
  Œufs (chance, ouverture multiple ×2/×3), Richesse (coins, diamants).
- **Zones** : les 65 zones avec leur capture, déblocage dans l'ordre, téléportation. Les zones
  38 à 65 demandent le rebirth.
- **Variantes** : à chaque œuf, 2 % de Golden, 0,5 % de Rainbow, 1 sur 1 000 de Shiny
  (couleurs et étincelles à l'écran). **Machine Golden** (zone 7) et **Machine Rainbow**
  (zone 14) : 10 pets identiques (non équipés) → 1 de la variante suivante.
- **Machines des zones 38 à 65** (dernière case de la Farm, icônes fournies) :
  - zone 38, **Améliore les potions** : 3 potions identiques → 1 du niveau suivant (wiki PS99) ;
  - zone 50, **Machine de fusion** : 3 à 12 pets (non équipés, pas de Huge) → 1 pet plus fort,
    tiré parmi les 3 plus forts des œufs débloqués dont la puissance ne dépasse pas la moitié de
    celle des pets mis ; Golden / Rainbow selon la part de Golden / Rainbow mis, jamais Shiny ;
  - zone 55, **Améliore les enchants** : 3 enchants identiques (non équipés) → 1 du niveau suivant ;
  - zone 58, **Daycare** : 5 places ; un pet y reste 10 min de jeu puis revient avec des coins,
    des diamants et parfois une potion ou un enchant (selon sa force).
- **Huges** (rareté Exclusive) : Huge Cat, Huge Dog et Huge Axolotl, dans n'importe quel œuf,
  avec une animation spéciale. Chance par œuf : 1/50 000 (normal), 1/75 000 (golden),
  1/100 000 (shiny), 1/125 000 (rainbow), 1/150 000 (golden shiny), 1/175 000 (rainbow shiny).
  Puissance : 200 / 225 / 250 / 275 / 300 / 325 % de ton meilleur autre pet (suit ta progression).
- **Objets** (onglet 8) : potions Coins, Dégâts, Diamants, Chance, Trésor et enchants Coins,
  Diamants, Chance, Force, Coup, Trésor, niveaux I à III (valeurs PS99). Trouvés en cassant des
  objets et dans les récompenses de rang. Potions actives visibles dans la barre du haut.
- **Quêtes et rangs** (onglet 7) : les 12 premiers rangs de PS99 (Noob → Superstar), jusqu'à
  4 quêtes (1 à 4 ★) ; les étoiles débloquent les récompenses du rang ; emplacements d'enchant
  aux rangs 1, 3, 5, 8 et 11.
- **Rebirth** (entre la zone 37 et la zone 38 dans la liste, une fois la zone 37 atteinte) :
  coins et zones remis à zéro, pets +75 % plus forts, coins ×10 (les zones 1 à 37 se refont en
  ~25 min) ; ouvre les zones 38 à 65. 1 rebirth pour l'instant.
- **Index** : les 236 pets, découverts en couleur, les autres en silhouette. Compléter un œuf
  (tous ses pets découverts, ★) donne des diamants ; tous les 20 pets découverts : +5 % de coins.
- **Sauvegarde automatique** dans le fichier `petsim.py` (visible dans l'app Python) : voir plus bas.
- **Durée** : réglée (`make balance`, voir plus bas) pour environ 6 h jusqu'à la zone 37, puis
  ~25 min pour refaire les zones 1 à 37 après le rebirth et ~3 h 30 pour les zones 38 à 65 ;
  les diamants sont rares.
- Objets à casser, du moins au plus riche : pièce, tas de pièces, caisse, cadeau, coffre ;
  diamant, tas de diamants. Vraies images PS99 (`assets/art/`). Icônes des onglets : provisoires.

## Jouer sur PC (navigateur)

`make web` compile le même code en WebAssembly (clang + wasi-libc, sans Emscripten) et produit
une page autonome `output/web/petsim.html` (`web/index.html` + `src/platform/platform_web.c`).
Clavier : flèches, Entrée = OK, Échap = Retour, E = EXE, Retour arrière = Effacer, T = Toolbox,
1 à 8 = onglets ; touches à l'écran sur téléphone. La sauvegarde reste dans le navigateur ; on
peut y coller le contenu de `petsim.py` de la calculatrice pour reprendre sa partie, et copier
celle du navigateur.

## Installer sur la calculatrice

Compatible N0110, N0115 et N0120 (pas N0100) ; v0.6 testée sur N0120 (Epsilon 26.4.0). Taille : 0,97 Mo (`.nwa`), 0,70 Mo une fois
installée (images compressées sans perte, décodées à l'affichage).

1. Récupère `petsim.nwa` (voir « Compiler » ci-dessous).
2. Branche la calculatrice en USB, ouvre <https://my.numworks.com/apps> dans Chrome ou Edge.
3. Dépose `petsim.nwa` et lance l'installation.
4. Sur la calculatrice : l'app **Pet Sim** apparaît dans le menu d'accueil.

## Sauvegarde

La partie est enregistrée toute seule (quelques secondes après chaque changement, et en quittant
avec Home) dans un script **`petsim.py`**, visible dans l'app Python. L'écran titre indique
« Partie chargée » ou « Nouvelle partie » ; la touche Toolbox y affiche le diagnostic.

- Ça tient : quitter l'app, éteindre la calculatrice, lancer d'autres apps.
- Ça ne tient pas : un reset ou une batterie complètement vide (le stockage d'Epsilon est en RAM).
- Une sauvegarde d'une version précédente se charge (format 4 ; les formats 1 à 3 sont relus,
  les Huges retrouvent leur place malgré les nouveaux pets).
- Supprimer `petsim.py` dans l'app Python recommence une nouvelle partie. Ne pas modifier ce
  fichier (la partie serait déclarée illisible).
- EADK n'a pas d'API de sauvegarde : le jeu écrit directement dans le stockage d'Epsilon, en
  suivant son format (code source d'Epsilon). Prévu pour Epsilon 26 ; à revérifier après une
  mise à jour d'Epsilon.

## Contrôles

| Touche | Action |
|---|---|
| Flèches | se déplacer |
| OK / EXE | action |
| ↓ sur la dernière ligne | aller sur la barre d'onglets (←/→ change d'onglet, ↑ revient) |
| 1 à 8 | onglet direct (Farm, Pets, Œufs, Zones, Amélio., Index, Quêtes, Objets) |
| Retour | fermer l'animation d'œuf, sinon revenir à Farm |
| Écran titre : Toolbox | diagnostic de la sauvegarde |
| Pets : OK / Effacer / Toolbox | équiper un exemplaire / en retirer un / équiper les meilleurs |
| Pets : ↑ depuis la 1re ligne | barre d'outils (←/→ choisir, OK appliquer) |
| Amélio. : ←/→, ↑/↓, OK | catégorie, amélioration, acheter le niveau suivant |
| Objets : OK / Effacer / Toolbox | boire la potion ou équiper un exemplaire de l'enchant / retirer un exemplaire / passer Potions ↔ Enchants |
| Farm, zones 7, 14, 38, 50, 55, 58 : OK sur la machine | ouvrir la machine |
| Fusion : OK / Effacer / EXE | ajouter le pet choisi / retirer le dernier / fusionner |
| Daycare : ↑ / ↓ | places du Daycare (OK : récupérer) / tes pets (OK : inscrire) |
| 2 8 1 2 ↑ ↑ ↓ ↓ ← → ← → OK | menu admin caché (coins, diamants, zones, Huges, objets, quêtes…) |
| Œufs : OK (pendant l'animation) | passer l'animation, puis rouvrir le même œuf |
| Œufs, Zones : ←/→ | défiler par page |
| Home | quitter |

## Compiler

Prérequis : `arm-none-eabi-gcc` (+ newlib), Node.js (pour `nwlink`, téléchargé automatiquement par `npx`).

```sh
# Ubuntu/Debian : sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi nodejs npm
# macOS : brew install numworks/tap/arm-none-eabi-gcc node
make            # -> output/petsim.nwa
make run        # installe directement via USB (nwlink)
make host       # version PC (sans écran) -> output/petsim-host
make screens    # captures PNG de tous les écrans -> output/screens/
make test       # tests unitaires
make assets     # régénère src/generated/ depuis data/ et assets/ (Python 3 + Pillow, facultatif)
make wiki       # retélécharge data/ et les images depuis le wiki PS99 (réseau)
make balance    # un bot joue toute la partie : temps par zone (output/balance.txt)
make calibrate  # règle les gains de chaque zone : ~6 h jusqu'à la zone 37, ~3 h 30 pour 38-65
```

Les fichiers de `src/generated/` (police, images, tables du contenu) sont versionnés : Python n'est **pas** nécessaire
pour compiler, seulement pour convertir de nouvelles images. Le jeu sur la calculatrice ne dépend pas de Python.

## Organisation

```
src/platform/  accès matériel (calculatrice EADK / PC)    src/gfx/      moteur de rendu par bandes, police, sprites
src/input/     clavier, répétition des touches             src/ui/       thème et widgets communs
src/screens/   écrans du jeu                               src/game/     état et logique du jeu
src/generated/ police, images et contenu convertis         tools/        outils PC (wiki, données, images, police)
assets/        images sources (PNG)                        tests/        tests unitaires PC
data/          zones, œufs, pets (CSV issus du wiki PS99)
```

## Crédits

- Données et images des pets, œufs et zones : [Pet Simulator Wiki](https://pet-simulator.fandom.com)
  (textes sous CC BY-SA) ; les images et noms de Pet Simulator 99 appartiennent à BIG Games.
- Police : rendu bitmap d'[Inter](https://rsms.me/inter/) (SIL Open Font License 1.1).
- SDK : EADK / nwlink de NumWorks.
- Projet de fan non officiel, non affilié à BIG Games ni à Roblox.
