# Données du jeu

Contenu repris de **Pet Simulator 99**, récupéré le 14/10/2026 sur le
[Pet Simulator Wiki](https://pet-simulator.fandom.com) par `tools/fetch_wiki.py` (`make wiki`).

| Fichier | Contenu | Source sur le wiki |
|---|---|---|
| `zones.csv` | les zones (« Areas ») 1 à 37 du Spawn World, prix de déblocage | `Template:PS99SpawnWorldAreaList` |
| `eggs.csv` | les œufs 1 à 50, leur zone et leur prix | `Template:PS99World1EggsList` |
| `egg_pets.csv` | les pets de chaque œuf et leur chance (%) | `Template:PS99World1EggsList` |
| `pets.csv` | les 140 pets de ces œufs et leur rareté PS99 | page de chaque pet (infobox `Pet99`) |

Les images correspondantes sont dans `assets/pets/`, `assets/eggs/` (réduites à 96 px) et
`assets/zones/` (captures des zones, réduites à 128 px de large).

## Pourquoi 37 zones pour 50 œufs ?

Les œufs 1 à 50 se trouvent dans les zones 1 à 37 (certaines zones ont 2 œufs) ; chaque zone
importée a donc au moins un œuf. Pour aller plus loin : `python3 tools/fetch_wiki.py --eggs 62`
(par exemple) importe les œufs suivants et les zones qui les contiennent, puis `make assets`.

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
- **Puissance** des pets, **PV** et **récompenses** des objets : équilibrage provisoire
  (puissance ×1,35 par œuf, multipliée par le rang ; une zone se finance en ~4 min).

Les images et les noms appartiennent à BIG Games ; textes du wiki sous licence CC BY-SA.
Projet de fan non officiel et non commercial.
