# Pet Simulator — NumWorks Edition

Adaptation 2D de Pet Simulator, en application native C (EADK) pour calculatrice NumWorks (`.nwa`).
Conception, maquettes et feuille de route : [docs/CONCEPTION.md](docs/CONCEPTION.md).

## État : M1 en cours — contenu Pet Simulator 99 intégré

- **Contenu réel de PS99** (wiki fandom) : 50 œufs, les 37 zones qui les contiennent, 140 pets,
  avec leurs vraies images. Détails et sources : [data/README.md](data/README.md).
- **Farm** : objets de la zone actuelle ; OK cible un objet (et le frappe), les pets équipés
  l'attaquent tout seuls puis passent au suivant (AUTO).
- **Œufs** : liste des œufs (zones débloquées + aperçu de la suivante), pets et chances exactes,
  pets non découverts en silhouette, animation d'ouverture selon la rareté.
- **Pets** : inventaire (pets empilés ×N), détail, équipe de 5.
- **Zones** : les 37 zones avec leur capture, déblocage dans l'ordre, téléportation.
- **Index** : les 140 pets, découverts en couleur, les autres en silhouette.
- Pas encore : sauvegarde (la partie repart de zéro à chaque lancement), améliorations, quêtes.
- Images des objets à casser et des onglets : encore provisoires.

## Installer sur la calculatrice

Compatible N0110, N0115 et N0120 (pas N0100). Testé sur PC uniquement pour l'instant.

1. Récupère `petsim.nwa` (voir « Compiler » ci-dessous).
2. Branche la calculatrice en USB, ouvre <https://my.numworks.com/apps> dans Chrome ou Edge.
3. Dépose `petsim.nwa` et lance l'installation.
4. Sur la calculatrice : l'app **Pet Sim** apparaît dans le menu d'accueil.

## Contrôles

| Touche | Action |
|---|---|
| Flèches | se déplacer |
| OK / EXE | action |
| ↓ sur la dernière ligne | aller sur la barre d'onglets (←/→ change d'onglet, ↑ revient) |
| 1 à 7 | onglet direct (Farm, Pets, Œufs, Zones, Amélio., Index, Quêtes) |
| Retour | fermer l'animation d'œuf, sinon revenir à Farm |
| Pets : OK / Effacer / Toolbox | équiper un exemplaire / en retirer un / équiper les meilleurs |
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
