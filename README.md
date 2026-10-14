# Pet Simulator — NumWorks Edition

Adaptation 2D de Pet Simulator, en application native C (EADK) pour calculatrice NumWorks (`.nwa`).
Conception, maquettes et feuille de route : [docs/CONCEPTION.md](docs/CONCEPTION.md).

## État : étape M0 (squelette)

- Écran titre, barre du haut (zone, coins, diamants), barre d'onglets, 7 écrans.
- Écran FARM : démo de l'interface (OK frappe l'objet sélectionné, réapparition, gains).
- Les autres écrans affichent ce qui est prévu. Les pets, œufs et la sauvegarde arrivent en M1.
- Images provisoires (dessinées par `tools/gen_placeholder_art.py`) en attendant les vraies.

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
| Retour | revenir à Farm |
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
make assets     # régénère src/generated/ (Python 3 + Pillow, facultatif)
```

Les fichiers de `src/generated/` (police, images) sont versionnés : Python n'est **pas** nécessaire
pour compiler, seulement pour convertir de nouvelles images. Le jeu sur la calculatrice ne dépend pas de Python.

## Organisation

```
src/platform/  accès matériel (calculatrice EADK / PC)    src/gfx/      moteur de rendu par bandes, police, sprites
src/input/     clavier, répétition des touches             src/ui/       thème et widgets communs
src/screens/   écrans du jeu                               src/game/     état et logique du jeu
src/generated/ police et images converties                 tools/        outils PC (conversion d'images, police)
assets/        images sources (PNG)                        tests/        tests unitaires PC
```

## Crédits

- Police : rendu bitmap d'[Inter](https://rsms.me/inter/) (SIL Open Font License 1.1).
- SDK : EADK / nwlink de NumWorks.
- Projet de fan non officiel, non affilié à BIG Games ni à Roblox.
