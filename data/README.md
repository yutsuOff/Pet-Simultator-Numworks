# Données du jeu

Contenu repris de **Pet Simulator 99**, récupéré le 14/10/2026 sur le
[Pet Simulator Wiki](https://pet-simulator.fandom.com) par `tools/fetch_wiki.py` (`make wiki`).

| Fichier | Contenu | Source sur le wiki |
|---|---|---|
| `zones.csv` | les zones (« Areas ») 1 à 65 du Spawn World, prix de déblocage | `Template:PS99SpawnWorldAreaList` |
| `eggs.csv` | les œufs 1 à 78, leur zone et leur prix | `Template:PS99World1EggsList` |
| `egg_pets.csv` | les pets de chaque œuf et leur chance (%) | `Template:PS99World1EggsList` |
| `pets.csv` | les 233 pets de ces œufs et leur rareté PS99 | page de chaque pet (infobox `Pet99`) |
| `huges.csv` | Huge Cat, Huge Dog, Huge Axolotl (images fournies) | pages Huge … (rareté Exclusive) |
| `potions.csv`, `enchants.csv` | niveaux I à III : effet (%) et durée | pages Potions et Enchants |
| `ranks.csv`, `rank_rewards.csv`, `rank_goals.csv` | rangs 1 à 12, récompenses, quêtes | modèles `RankNinfo` |
| `balance.csv` | gains de chaque zone (calculé par `tools/calibrate_balance.py`, pas le wiki) | — |

Les images correspondantes sont dans `assets/pets/`, `assets/eggs/` (réduites à 96 px) et
`assets/zones/` (captures des zones, réduites à 128 px de large).

## Pourquoi 65 zones pour 78 œufs ?

Les œufs 1 à 50 se trouvent dans les zones 1 à 37 (certaines zones ont 2 œufs), les œufs 51 à 78
dans les zones 38 à 65 (un par zone) ; chaque zone importée a donc au moins un œuf. Pour aller plus
loin : `python3 tools/fetch_wiki.py --eggs 90` (par exemple) importe les œufs suivants et les zones
qui les contiennent, puis `make assets`. Les nouveaux pets s'ajoutent à la fin de `pets.csv`
(les numéros des pets existants ne changent pas, la sauvegarde en dépend).

## Fidélité au wiki

- Noms, prix, zones et chances sont ceux du wiki, **sans modification**.
- Prix convertis en coins selon la page « Currencies » : 1 Gold Bar = 1 million de coins,
  1 Platinum Coin = 1 million de Gold Bars. La colonne `cost_wiki` garde l'écriture d'origine.
- Une chance **« ?? »** veut dire que le wiki ne la donne pas (pet rare « teaser » de l'œuf suivant,
  11 cas). Elle est gardée telle quelle ici ; le jeu l'affiche « ?? » et utilise 0,05 %.
- Sur le wiki, les chances d'un œuf ne font pas toujours 100 % pile (arrondis, ex. 99,45 %).
  Le jeu les **normalise** : les % affichés dans le jeu sont les chances réelles du tirage.
- Le nom « Chrystal Caverns » de la liste des zones est corrigé en « Crystal Caverns »
  (orthographe utilisée par la liste des œufs).

## Ce qui n'est pas sur le wiki (règles du jeu, `tools/gen_data.py`)

- **Rareté** : tous les pets d'œufs sont « Basic » dans PS99. Le jeu leur donne un rang selon
  leur chance dans leur « œuf d'origine » (le dernier œuf qui les contient) :
  ≥ 20 % Basique, ≥ 5 % Rare, ≥ 1 % Épique, ≥ 0,1 % Légendaire, sinon Mythique.
- **Puissance** des pets et **PV** des objets : puissance ×1,35 par œuf, multipliée par le rang.
- **Coins des objets** : calibrés zone par zone par `tools/calibrate_balance.py` (un bot joue la
  partie avec les vraies règles) pour environ 6 h jusqu'à la zone 37 : ~4 min pour la zone 1,
  puis de plus en plus long (`data/balance.csv`).

## Objets, rangs et quêtes (`tools/fetch_wiki_items.py`)

- Potions et enchants : valeurs exactes du wiki pour les niveaux I à III (les seuls du jeu
  pour l'instant) ; une récompense de rang de niveau IV+ devient un niveau III.
- Récompenses de rang : les sacs de diamants sont divisés par 10 (diamants rares, partie de
  6 h) ; une récompense absente du jeu (fruits, drapeaux, jouets, TNT…) devient 2 × n° du rang
  diamants. Les étoiles nécessaires sont celles du wiki.
- Quêtes : les objectifs du wiki ; « coin jars » → caisses, « comets » → cadeaux,
  « mini-chests » / « Safes » → coffres ; pêche, fouilles, fruits, drapeaux et améliorations
  de potions/enchants sont ignorés (absents du jeu).

## Variantes (règles du jeu)

- Golden 2 %, Rainbow 0,5 %, Shiny 1 sur 1 000 à l'éclosion, Huges de 1/50 000 à 1/175 000 (choix du
  joueur : le wiki ne donne que 10 %/10 % avec le gamepass Magic Eggs et rien pour Shiny).
- Puissance : Golden +13 %, Rainbow +27 % (pages Gold / Rainbow Machine), Shiny ×1,5 (non
  publié par BIG Games), Huge = 200 % (golden 225, shiny 250, rainbow 275, golden shiny 300,
  rainbow shiny 325 %) de ton meilleur autre pet, chances 1/50 000 à 1/175 000 selon la variante, Rebirth +75 % (Rebirth 1).

## Machines des zones 38 à 65 (règles du jeu)

- Améliore les potions / les enchants : 3 → 1 du niveau suivant (valeurs du wiki, pages Upgrade
  Potions / Upgrade Enchants Machine) ; placées en zones 38 et 55 (PS99 : 13 et 16).
- Fusion (PS99 : zone 28, ici 50) et Daycare (PS99 : zone 20, ici 58) : principe du wiki, valeurs du
  jeu (`src/game/machines.c`).
- Rebirth : +75 % de force (Rebirth 1 de PS99) et coins ×10 (choix du jeu).

Les images et les noms appartiennent à BIG Games ; textes du wiki sous licence CC BY-SA.
Projet de fan non officiel et non commercial.
