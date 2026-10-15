# Pet Simulator — NumWorks Edition · Document de conception (v0.1)

> Phase actuelle : **M1 en cours** — squelette (M0) fait, contenu PS99 (œufs 1-50, zones, pets,
> images) intégré avec farm, œufs, inventaire, zones et index jouables. Voir §4 et §10.
> Ce document rassemble les vérifications techniques, une proposition d'interfaces,
> les mécaniques à valider ensemble et l'architecture prévue.
> Les points marqués **[À DÉCIDER]** attendent ta réponse.

---

## 1. Contraintes techniques vérifiées

### 1.1 Ce qui a été réellement vérifié (14/10/2026)

| Élément | Résultat | Comment |
|---|---|---|
| SDK officiel | **EADK**, fourni par le paquet npm `nwlink` (dernière version : **1.0.0**) | `eadk.h` lu dans le paquet |
| Exemple officiel C | `numworks/epsilon-sample-app-c` | cloné et lu |
| Compilation `.nwa` | **Réussie** avec `arm-none-eabi-gcc 13.2.1` + `nwlink 1.0.0` (flags `eadk-cflags-device`) | build réel dans l'environnement de dev |
| Test sur calculatrice | **Non fait** (pas de calculatrice ici) | — |

Remarque : le Makefile de l'exemple officiel épingle `nwlink@0.0.16`, qui **plante** avec Node 22.
Avec `nwlink@1.0.0`, la commande `eadk-cflags` a été renommée `eadk-cflags-device`.
Notre Makefile en tiendra compte.

### 1.2 Ce que l'API EADK permet

- **Écran** 320×240, couleurs RGB565 : `push_rect` (envoi d'un bloc de pixels), `push_rect_uniform`
  (rectangle plein), `pull_rect` (lecture de l'écran), `wait_for_vblank`, `draw_string`
  (2 polices système, fond opaque obligatoire).
- **Clavier** : état de toutes les touches (`keyboard_scan`) ou événements (`event_get`).
- **Temps** : `timing_millis` (millisecondes **depuis le démarrage**), `msleep`, `usleep`.
- **Hasard** : `eadk_random()` (générateur matériel).
- **Divers** : batterie, rétroéclairage, USB, données externes en lecture seule.

### 1.3 Ce que l'API EADK **ne** permet **pas**

- **Aucune API de fichiers ni de sauvegarde.** C'est la contrainte la plus importante (voir §8).
- **Aucune horloge réelle (date/heure).** On ne peut donc pas faire de gains hors-ligne ni de
  récompenses « quotidiennes » basées sur la date. Tous les timers fonctionnent uniquement
  pendant que le jeu tourne.
- Pas d'alpha-blending matériel : la transparence des images est gérée par notre code.
- Pas de son.

### 1.4 Modèles compatibles

| Modèle | Apps `.nwa` | Remarques |
|---|---|---|
| N0100 | **Non** | pas d'applications externes |
| N0110 / N0115 | Oui | Cortex-M7 216 MHz, ~256 Ko de RAM au total |
| N0120 | Oui | Cortex-M7 plus rapide, plus de RAM |

- **Flash disponible pour les apps : environ 2,5 Mo** (source communautaire Nwagyu) → budget pour
  le code + toutes les images + les données.
- **RAM** : la taille exacte du tas des apps externes n'est pas documentée. Je vise un budget
  **très prudent** : pas de framebuffer plein écran (320×240×2 = 150 Ko), allocation **statique**
  uniquement (pas de `malloc`).
- **Bug connu** ([epsilon#2357](https://github.com/numworks/epsilon/issues/2357)) : corruption
  mémoire quand une app écrit dans son tas, corrigée en Epsilon 24.6.0 mais un problème subsiste
  sur N0120 dans certains cas. Raison de plus pour éviter le tas.
- Installation : via le site officiel `my.numworks.com/apps` (ou `nwlink install-nwa` en USB).

**[À DÉCIDER] Q1 — J'ai besoin de ton modèle exact (N0110 / N0115 / N0120, écrit au dos) et de ta
version d'Epsilon (Paramètres → À propos).** Utilises-tu l'OS officiel Epsilon, ou Upsilon/Omega ?

---

## 2. Principes de gameplay (rappel de ton cahier des charges)

- Aucun personnage déplaçable : tout passe par des **écrans**, des **cartes** et des **menus**.
- Boucle principale : **farmer** des objets → gagner **coins** (+ parfois **diamonds**) →
  **ouvrir des œufs** → obtenir de meilleurs **pets** → farmer plus vite → **débloquer des zones** →
  nouveaux œufs, etc.
- En parallèle : **upgrades**, **index**, **quêtes**, **succès**.

---

## 3. Organisation des interfaces (proposition)

### 3.1 Structure commune à tous les écrans

```
y=0   ┌──────────────────────────────────────────────────────┐
      │ [icône zone] Zone 3 · Forest      🪙 12.4K   💎 230  │  barre haute (20 px)
y=20  ├──────────────────────────────────────────────────────┤
      │                                                      │
      │              contenu de l'écran (200 px)             │
      │                                                      │
y=220 ├──────────────────────────────────────────────────────┤
      │ FARM │ PETS │ ŒUFS │ ZONES │ AMÉL. │ INDEX │ QUÊTES  │  barre d'onglets (20 px)
y=240 └──────────────────────────────────────────────────────┘
```

- La couleur dominante de la barre haute et du fond change avec l'**identité graphique de la zone**.
- Style : cartes à coins arrondis, bordures épaisses, dégradés simples, couleurs vives par rareté
  (gris Basic, bleu Rare, violet Epic, orange Legendary, rouge/arc-en-ciel Mythical, etc.).
- Police : les 2 polices système ont un fond opaque. Je propose d'intégrer **une police bitmap
  maison** (chiffres + lettres) pour un rendu plus « jeu » et des textes posés sur les dégradés.

### 3.2 Contrôles

| Touche | Action |
|---|---|
| Flèches | se déplacer entre les éléments de l'écran |
| OK / EXE | action principale (attaquer, ouvrir, équiper, acheter…) |
| Retour | fermer une fenêtre / revenir |
| 1 à 7 | accès direct aux onglets (FARM=1 … QUÊTES=7) |
| ↓ depuis la dernière ligne | sélectionne la barre d'onglets, puis ←/→ pour changer d'onglet, ↑ pour revenir |
| Toolbox | options de l'écran (tri, filtres, actions groupées) |
| Home | sauvegarder et quitter |

### 3.3 Écran FARM (accueil)

```
┌──────────────────────────────────────────────────────┐
│ 🌲 Zone 3 · Forest                🪙 12.4K   💎 230   │
├──────────────────────────────────────────────────────┤
│ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐                  │
│ │ pile │ │COFFRE│ │ pile │ │ 💎   │   grille 4×2 de  │
│ │ ▓▓▓░ │ │▓▓▓▓▓▓│ │ ▓░░░ │ │ ▓▓░░ │   cartes « objet »│
│ └──────┘ └══════┘ └──────┘ └──────┘   (image + barre │
│ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐    de vie + gain)│
│ │ pile │ │ (⌛) │ │ pile │ │ pile │   ⌛ = réapparition│
│ └──────┘ └──────┘ └──────┘ └──────┘                  │
│ Pets : [🐱][🐶][🐉][  ][  ]   Dégâts/s : 1.2K  [AUTO]│
├──────────────────────────────────────────────────────┤
│ FARM │ PETS │ ŒUFS │ ZONES │ AMÉL. │ INDEX │ QUÊTES  │
└──────────────────────────────────────────────────────┘
```

- On sélectionne un objet avec les flèches, OK envoie les pets dessus.
- Les icônes des pets équipés « sautent » vers l'objet attaqué, petits chiffres de dégâts,
  particules de coins à la destruction, barre de vie animée.
- **Anti-répétition** : mode **AUTO** (les pets passent seuls à l'objet suivant). Voir Q4.

### 3.4 Écran PETS / INVENTAIRE

```
┌──────────────────────────────────────────────────────┐
│ Inventaire 37/50     Équipés 3/5      Tri : Puissance│
├──────────────────────────────────────────────────────┤
│ ┌────┐┌────┐┌────┐┌────┐┌────┐  │ ┌─────────────────┐│
│ │🐉✔ ││🐱  ││🐶✔ ││🦊  ││🐰  │  │ │  [grande image] ││
│ │x2  ││x14 ││    ││x3  ││x7  │  │ │ Dragon  ★LEGEND.││
│ └────┘└────┘└────┘└────┘└────┘  │ │ Puissance 1.4K  ││
│ (grille 5×3, défilement)        │ │ Œuf : Wood Egg  ││
│                                 │ │ [Équiper] [Plus]││
│                                 │ └─────────────────┘│
└──────────────────────────────────────────────────────┘
```

- Grille d'icônes avec cadre coloré selon la rareté, ✔ si équipé, ×N pour les doublons.
- Panneau de détail à droite. Tri : puissance / rareté / nom. Filtres : rareté, équipés.
- Actions : équiper/déséquiper, **« Équiper les meilleurs »** (un seul bouton), supprimer/recycler
  les doublons, (fusion si retenue, voir §5).

### 3.5 Écran ŒUFS

- Liste des œufs de la zone actuelle (et des zones débloquées) : image, nom, prix.
- Panneau : tous les pets de l'œuf avec **leur probabilité exacte** (tient compte de la chance
  actuelle) ; les pets non découverts apparaissent en **silhouette noire « ? »**.
- Ouverture : animation en 3 temps → l'œuf tremble, se fissure, éclate avec un flash de la couleur
  de rareté → le pet apparaît en grand avec son nom, sa rareté, « NOUVEAU ! » s'il est inédit.
  Plus la rareté est haute, plus l'animation est spectaculaire (plus de flashs, fond arc-en-ciel).
- Historique des **10 meilleures découvertes** (les plus rares) visible depuis cet écran.

### 3.6 Écran ZONES

- Liste verticale des zones (37 pour l'instant, celles des œufs 1 à 50), regroupées par « mondes » de 5 à 10 zones (thème graphique).
- Chaque carte : nom, couleur/identité, prix de déblocage, difficulté (★ à ★★★★★),
  œufs de la zone, récompenses typiques. Zones verrouillées grisées avec cadenas.
- OK = se téléporter (instantané, sans déplacement).

### 3.7 Écran AMÉLIORATIONS

- Onglets de catégories : **Pets** · **Farming** · **Œufs** · **Richesse**.
- Chaque upgrade : icône, nom, description, niveau actuel / max, effet actuel → effet suivant, coût
  (coins ou diamonds).

### 3.8 Écran INDEX

- Grille de tous les pets, par œuf. Découverts : image en couleur ; non découverts : silhouette.
- Compteur global (ex. 87/212) + **récompenses de complétion** par œuf et par palier global.

### 3.9 Écran QUÊTES / SUCCÈS

- Deux sous-onglets. Quêtes : objectifs courts qui se renouvellent (ex. « Casse 50 coffres »,
  « Ouvre 20 Wood Eggs ») ; Succès : objectifs permanents à paliers (bronze ### 4.1 Source : wiki Pet Simulator 99 (fait le 14/10/2026)

L'accès à `pet-simulator.fandom.com` fonctionne désormais. `tools/fetch_wiki.py` lit les tableaux
officiels du wiki via son API et produit les CSV de `data/` et les images :

- **50 œufs** (Cracked Egg n° 1 → Snow Egg n° 50), avec prix et chances ;
- les **37 zones** qui les contiennent (Spawn → Snow Village), avec prix et capture d'écran ;
- les **140 pets** de ces œufs, avec leur rareté PS99 et leur image.

Tout est repris sans modification (détails, conversions de monnaie et exceptions dans
[data/README.md](../data/README.md)). 11 pets ont une chance « ?? » sur le wiki (pet rare de
l'œuf suivant) : le jeu affiche « ?? » et utilise 0,05 %.

Ce que le wiki ne fournit pas est calculé par `tools/gen_data.py` (règles modifiables en un seul
endroit) :

| Donnée | Règle |
|---|---|
| Rareté affichée | Tous les pets d'œufs sont « Basic » dans PS99 → rang selon la chance dans l'œuf d'origine (dernier œuf qui le contient) : ≥ 20 % Basique, ≥ 5 % Rare, ≥ 1 % Épique, ≥ 0,1 % Légendaire, sinon Mythique |
| Puissance d'un pet | 10 × 1,35^(n° de l'œuf − 1) × rang (×1 / 1,4 / 2 / 3 / 5) |
| PV d'une pile de coins | 4 × puissance d'un pet basique du premier œuf de la zone |
| Coins d'une pile | prix de la zone suivante ÷ 250 (≈ 4 min de farm par zone) |
| Couleurs de la zone | moyennes des couleurs de sa capture d'écran |

### 4.2 Format des données (facile à étendre)

```
data/zones.csv      area, name, cost_coins, cost_wiki, image
data/eggs.csv       egg, name, area, cost_coins, cost_wiki, image
data/egg_pets.csv   egg, pet, chance_percent
data/pets.csv       pet, rarity, image
```

`make assets` les convertit en tableaux C (`src/generated/content.c`) ; `make wiki` les
retélécharge. Plus d'œufs : `python3 tools/fetch_wiki.py --eggs 62`, puis `make assets`.

œuf, poids (probabilité), image
data/breakables.csv  id, zone, type, PV, récompense, réapparition, rareté
```

### 4.3 Probabilités vérifiables

- Les chances sont stockées en **entiers** (poids sur 1 000 000 000) → aucune erreur d'arrondi.
- L'écran Œufs affiche le % exact. Un **test automatique sur PC** ouvre 10 millions d'œufs et
  vérifie que la distribution observée correspond aux pourcentages affichés.
- Les bonus de chance (upgrades) multiplient les poids des raretés hautes, puis renormalisent :
  le % affiché est toujours le % réel.

---

## 5. Mécaniques de progression à choisir ensemble **[À DÉCIDER] Q7**

Je ne les ajoute pas sans ton accord. Voici mes propositions avec leurs avantages :

| Mécanique | Principe | Avantages | Coût / risque |
|---|---|---|---|
| **Variantes Golden / Rainbow** | Fusionner N exemplaires identiques → version dorée (×2-3 puissance), puis arc-en-ciel | Donne une utilité aux doublons, objectif de collection énorme, fidèle à PS99 | Multiplie l'index (×3) |
| **Shiny** | Petite chance (ex. 1/1000) qu'un pet éclose « shiny » (scintillement, ×1,5) | Le frisson du jackpot à chaque ouverture | Quasi aucun |
| **Huge pets** | Pets gigantesques ultra-rares (1/50 000+) par monde | Objectif ultime, très PS99 | Équilibrage délicat |
| **Rebirth** | Remise à zéro des coins/zones contre un multiplicateur permanent | Rejouabilité quasi infinie | Doit rester optionnel et rentable |
| **Niveaux / maîtrise des pets** | Les pets gagnent de l'XP en farmant | Attachement aux pets | Sauvegarde plus lourde |
| **Potions / boosts temporaires** | Objets ×2 coins, ×2 chance pendant X minutes de jeu | Récompenses de quêtes variées | Simple |
| **Enchantements** | Bonus passifs équipables (coins, chance…) | Personnalisation | Interface en plus |
| **Rangs** | Rangs débloqués par des quêtes, donnant + emplacements de pets/récompenses | Objectifs court terme constants (comme PS99) | Doit bien s'articuler avec les quêtes |
| **Événements aléatoires** | « Pluie de coins », « Coffre géant » apparaissent pendant le jeu | Surprise, dynamisme | Simple |
| **Secrets** | Pets secrets via codes, actions cachées, zones cachées | Découverte | À scénariser |

Ma recommandation pour la **première version jouable** : Golden/Rainbow + Shiny + Événements
aléatoires. Rebirth, Huge et Rangs ensuite.

---

## 6. Économie et équilibrage (principes)

- **Grands nombres** : `double` (jusqu'à 10^308), affiché avec suffixes : 1.2K, 34.5M, 1.0B, 7.8T,
  1.2Qa, 3.4Qi, Sx, Sp, Oc, No, Dc… Formateur écrit à la main (le `printf` embarqué ne gère pas
  les flottants).
- Croissance **géométrique** maîtrisée : PV des objets, prix des zones et puissance des pets
  progressent d'environ ×2 à ×3 par zone, calibrés pour que :
  - chaque zone se débloque après quelques minutes de farm avec les pets de la zone précédente ;
  - un pet rare de l'œuf N rende la zone N+2 / N+3 confortable, sans permettre de tout sauter.
- **Diamonds** : rares (petite chance sur chaque objet, coffres à diamants, quêtes, succès, index).
  Utilisés pour les améliorations permanentes.
- **Simulateur d'équilibrage sur PC** : un « bot » joue au jeu en accéléré avec les vraies formules
  et produit un graphique temps/zone → on ajuste les chiffres sans deviner.

**[À DÉCIDER] Q6 — Durée visée** pour finir les 50 zones (sans rebirth) : ~10 h ? ~30 h ? plus ?

---

## 7. Graphismes et images

- **Pipeline PC** (Python, uniquement sur l'ordinateur) : `tools/convert_images.py` lit tes PNG,
  les redimensionne, réduit la palette (256 couleurs max par image, souvent bien moins), gère la
  transparence (1 couleur transparente + anti-aliasing des bords précalculé), compresse en RLE et
  génère des fichiers C intégrés à l'app. **Le jeu sur la calculatrice ne dépend pas de Python.**
- Tailles retenues : **48×48** pour les pets et les œufs (détail, éclosion en ×2), **24×24**
  calculé à l'affichage (moyenne 2×2, pas de double stockage), captures de zones en **64×36**.
- Mesuré : 140 pets + 50 œufs + 37 zones = 564 Ko d'images ; l'app complète fait 811 Ko
  (`.nwa`), dans les ~2,5 Mo disponibles.
- Rendu sans framebuffer plein écran : composition par **bandes** (ex. 320×20 px) en RAM puis
  envoi à l'écran → pas de scintillement, peu de RAM.
- Tant que je n'ai pas tes images, j'utilise des **placeholders** clairement identifiés, remplacés
  dès réception.

**[À DÉCIDER] Q3 — Images** : combien en as-tu, quel format (PNG transparent ?), quelle taille ?
Fournis-tu aussi les œufs, les objets à casser, les fonds de zone, l'icône de l'app ?

---

## 8. Sauvegarde — l'état réel des possibilités

### 8.1 Constat

- EADK **ne fournit aucune API de sauvegarde**.
- Le stockage d'Epsilon (là où vivent les scripts Python) est accessible **de façon non officielle**
  grâce à une bibliothèque communautaire (`storage.c` de Yaya.Cout, utilisée par plusieurs apps,
  dont le template Rust NumworksAppsRust). Elle localise le stockage en mémoire et lit/écrit des
  « fichiers » dedans.
- Ce stockage est en **RAM** : d'après la communauté, il survit à l'extinction (mise en veille)
  et aux sorties d'app, mais **pas à un reset complet ni à une batterie totalement vide**.
- Taille du stockage partagé avec tes scripts Python : quelques dizaines de Ko → la sauvegarde
  doit être **compacte** (objectif < 4 Ko).

### 8.2 Ce qui est fait (v0.3, pour N0120 / Epsilon 26.4.0)

Vérifié dans le code source d'Epsilon (dépôt `numworks/epsilon`, octobre 2026) :

- Epsilon place un **« userland header »** au début de son code, dans chacun des deux
  emplacements de firmware de la flash externe (0x90010000 / 0x90410000, ou +64 Ko selon le
  modèle). Il contient l'adresse et la taille du stockage en RAM (42 Ko) et la zone de flash
  des apps externes. Le jeu choisit l'emplacement dont la zone d'apps contient son propre code,
  puis vérifie les « magic numbers » du stockage avant d'y toucher.
- Format du stockage : `| magic | enregistrements… | 0x0000 | libre | magic |`, chaque
  enregistrement = taille (16 bits) + nom (`petsim.py\0`) + contenu. Un script Python = 1 octet
  d'état + texte.
- **Sécurité des scripts** : Epsilon garde en cache l'adresse du dernier enregistrement utilisé,
  donc le jeu ne déplace **jamais** un enregistrement existant : `petsim.py` a une taille fixe
  (1 Ko) et est réécrit sur place ; à la création, il est ajouté après le dernier. Si ce n'est
  pas possible (stockage plein, fichier modifié), le jeu n'écrit rien et le signale.
- Contenu : 2 lignes de commentaire Python, l'état du jeu en binaire versionné + CRC32 encodé
  en base64 (~310 caractères). Un fichier abîmé, modifié ou venant d'une version plus récente
  n'est jamais écrasé.
- Écriture : quelques secondes après chaque changement, et en quittant avec Home.
- Tests PC : `tests/test_save.c` (aucun enregistrement déplacé, refus si plein, CRC) et le
  stockage émulé de `petsim-host --storage FICHIER` (quitter puis relancer).

**Statut : expérimental tant que ce n'est pas confirmé sur la calculatrice.** Test : jouer,
quitter avec Home, relancer → l'écran titre doit dire « Partie chargée » (Toolbox : détails).

## 9. Architecture technique

### 9.1 Choix

- **Langage : C (C11)**, comme l'exemple officiel : code compact, rapide, pas de dépendances,
  et compilable aussi sur PC pour les tests.
- **Boucle à pas fixe** (30 images/s) : logique et rendu séparés ; seules les zones de l'écran qui
  changent sont redessinées.
- **Zéro `malloc`** : tout l'état du jeu est statique et de taille connue.
- **Couche plateforme** : le jeu ne parle jamais directement à EADK. Deux implémentations :
  - `platform_eadk.c` → la calculatrice (`.nwa`) ;
  - `platform_host.c` → PC (fenêtre SDL2, ou mode sans écran qui produit des captures PNG) pour
    tester, déboguer et faire tourner les tests automatiques.

### 9.2 Arborescence prévue

```
Pet-Simultator-Numworks/
├── Makefile                 make (→ .nwa), make host, make test
├── src/
│   ├── main.c               boucle principale
│   ├── platform/            platform.h, platform_eadk.c, platform_host.c
│   ├── gfx/                 primitives, sprites (décompression), police, thème/couleurs
│   ├── input/               touches, répétition, appuis longs
│   ├── ui/                  widgets : boutons, cartes, barres, fenêtres, listes défilantes
│   ├── screens/             farm, pets, eggs, hatch, zones, upgrades, index, quests
│   ├── game/                state, farming, pets, eggs, economy, upgrades, progress,
│   │                        quests, rng, numfmt (format 1K/1M/1B…)
│   └── save/                serialize (format binaire versionné), storage (lib communautaire)
├── data/                    zones/eggs/pets/breakables/upgrades/quests (.csv)
├── assets/                  pets/, eggs/, ui/, icon.png (tes images originales)
├── tools/                   gen_data.py, convert_images.py, balance_sim
├── tests/                   tests unitaires sur PC (probabilités, économie, sauvegarde)
└── docs/                    conception, installation, ajout de contenu
```

### 9.3 Compilation et installation (prévu)

```sh
# Outils : arm-none-eabi-gcc + Node.js (pour nwlink) + Python 3 (outils de conversion sur PC)
make                # → output/petsim.nwa
make host && ./output/petsim-host   # version PC pour tester
make test           # tests automatiques
```

Installation : brancher la calculatrice en USB, aller sur `my.numworks.com/apps`, glisser le
fichier `petsim.nwa` (ou `make run` avec nwlink).

---

## 10. Plan de développement progressif

| Étape | Contenu | Résultat testable |
|---|---|---|
| **M0** | Squelette : build `.nwa` + build PC, barre haute/onglets, navigation, police, thème | App qui démarre, navigation entre écrans vides |
| **M1** | Boucle jouable : farm, œufs + animation, inventaire, équiper, zones, index *(fait, avec le contenu PS99)* ; sauvegarde + écran de test sauvegarde *(à faire)* | **Premier jeu jouable sur calculatrice** |
| **M2** | Upgrades, Index, tri/filtres, gestion des doublons | Progression complète sur 3 zones |
| **M3** | Contenu complet : 50 œufs, 37 zones, 140 pets du wiki PS99 *(fait)* ; images des objets à casser et des onglets *(à faire)* | Contenu complet |
| **M4** | Quêtes, succès, mécaniques choisies (§5) | Rejouabilité |
| **M5** | Simulateur d'équilibrage, réglages, polish visuel, optimisation | Version 1.0 |

Chaque étape sera compilée ; je te dirai exactement ce qui a été testé sur PC et ce qui reste à
tester sur ta calculatrice.

---

## 11. Questions ouvertes (récapitulatif)

1. ~~Modèle~~ : N0120, Epsilon 26.4.0.
2. ~~Contenu PS99~~ : fait (œufs 1 à 50 et leurs 37 zones, depuis le wiki).
3. **Images** : pets, œufs et zones viennent du wiki. Restent les objets à casser, les onglets et
   l'icône : les fournis-tu ?
4. **Farming** : semi-idle (on choisit une cible, les pets attaquent seuls, mode AUTO) **ou** aussi
   une part active (OK = coup de pouce « tap ») ?
5. **Pets sur les cibles** : tous les pets sur un même objet, ou répartis sur plusieurs objets ?
6. **Durée** visée pour finir les 50 zones ?
7. **Mécaniques** du §5 à retenir (et lesquelles pour la première version) ?
8. **Inventaire** : pets empilés (×N) ou exemplaires individuels ? Taille de départ ? Supprimer un
   pet rend-il quelque chose (coins/diamonds) ?
9. **Diamonds** : sources validées (objets rares, coffres, quêtes, succès, index) ?
10. ~~Sauvegarde~~ : faite (§8.2), à confirmer sur ta N0120.
11. **Langue** de l'interface : français avec noms de pets en anglais (comme PS99) ?
12. **Ouverture multiple** : dès le début, ou débloquée par upgrade (×1 → ×3 → ×8) ?
