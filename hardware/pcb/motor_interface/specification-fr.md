# Carte Hall + ESP32 — spécification v2

Ce qui est **déjà saisi et validé** au schéma, et ce qui reste à ajouter.

---

## Partie 1 — déjà fait et vérifié

| Bloc | État |
|---|---|
| 8 connecteurs passthrough, changement de code couleur | ✅ |
| 12 canaux RC 1 kΩ / 10 nF sur la dérivation | ✅ |
| 2 × SN74LVC14AD, découplage 100 nF | ✅ |
| Support ESP32 2 × 19 broches, 12 GPIO en banque 0 | ✅ |
| Rail +5V_MOT traversant, GND commun | ✅ |

---

## Partie 2 — les six blocs à ajouter

### Bloc A — Entrée 5 V protégée

Deux borniers à vis 2 voies, montés **en parallèle** sur l'entrée : `5V_IN` (venant du convertisseur de la carte de distribution) et `5V_OUT` (pour repiquer vers autre chose). Fils de section normale ici, contrairement aux fils Hall — le bornier à vis est le bon choix.

Chaîne de protection, dans cet ordre exact :

```
bornier → PPTC 500 mA → TVS SMAJ5.0A vers GND → Schottky SS14 → cavalier JP1 → broche 5V du module
```

Ce que chaque élément traite :

| Composant | Protège contre |
|---|---|
| **PPTC 500 mA** (fusible réarmable) | surintensité, et sert de fusible aux deux suivants |
| **TVS SMAJ5.0A** | surtension — si ton convertisseur lâche et envoie 37 V, elle écrête et fait fondre le PPTC |
| **Schottky SS14** | inversion de polarité, **et retour de courant vers l'USB** |
| **Cavalier JP1** | isolation manuelle totale |

En inversion de polarité, la TVS conduit en direct, court-circuite l'entrée, le PPTC déclenche. C'est le comportement voulu.

### Bloc B — Le conflit USB / convertisseur

**C'est le point le plus délicat de la carte, et ta question était la bonne.**

Sur un DevKitC, la broche `5V` est reliée **directement au VBUS de l'USB**, sans diode. Deux sources de 5 V en parallèle : la plus haute impose sa tension et refoule dans l'autre. Si ton convertisseur est à 5,5 V et l'USB à 5,0 V, tu injectes du courant dans le port USB de ton ordinateur.

La parade tient en trois points :

1. **La Schottky SS14** en série sur l'entrée externe. Elle chute d'environ 0,35 V : ton 5,0 V arrive à **4,65 V** sur la broche du module. L'USB étant à ~5,0 V, il est **toujours au-dessus** — donc c'est lui qui alimente, la Schottky est bloquée, et **rien ne remonte vers ton convertisseur**.
2. **Règle le convertisseur à 5,0 V, pas plus.** À 5,5 V la parade tombe : 5,15 V après diode, au-dessus de l'USB, et ça refoule. C'est une consigne, pas une option.
3. **Le cavalier JP1** pour les paranoïaques et pour le stockage.

4,65 V suffisent largement au régulateur du module (l'AMS1117 sort son 3,3 V à partir de ~4,4 V au courant qui nous concerne).

**Sérigraphie obligatoire à côté du bornier :** `5,0 V MAX`.

### Bloc C — CAN

L'ESP32 a un contrôleur CAN intégré (TWAI), mais il ne sort que du **TX/RX logique en 3,3 V**. Ce n'est pas du CAN. Il faut un **émetteur-récepteur** pour obtenir le CAN_H / CAN_L différentiel — sinon tu envoies de la logique 3,3 V asymétrique sur un faisceau de robot, et ça ne marchera pas.

| Élément | Choix |
|---|---|
| Émetteur-récepteur | **SN65HVD230D** (SOIC-8) — alimenté en 3,3 V natif, aucune adaptation |
| CAN_TX | **GPIO 4** |
| CAN_RX | **GPIO 35** (entrée seule — parfait pour une entrée, impossible à piloter par erreur) |
| Découplage | 100 nF |
| Broche Rs | 10 kΩ vers GND (mode haute vitesse) |
| Terminaison | **120 Ω + cavalier JP2** — à fermer uniquement si la carte est en bout de bus |
| Connecteurs | **deux** embases 3 broches (CAN_H, CAN_L, GND) câblées **en parallèle** |

Les deux connecteurs permettent de traverser le bus sans créer d'antenne : le faisceau entre par l'un et repart par l'autre. C'est la topologie CAN correcte.

Ni GPIO 4 ni GPIO 35 ne sont des broches de strap ou de flash.

### Bloc D — 12 LED d'activité

Une LED par capteur Hall, pilotée depuis la **sortie du buffer**, jamais depuis la ligne Hall — zéro charge sur le passthrough.

- LED 0805 + résistance **1 kΩ** en série vers GND → ≈ 2 mA
- Le LVC14 débite 24 mA, on est à un dixième de sa capacité
- Le buffer étant inverseur, la LED s'allume quand le capteur est à l'état bas — sans importance

Tu fais tourner une roue à la main, robot éteint, sans ordinateur : trois LED clignotent en séquence. Un capteur mort ou un fil coupé se voit immédiatement.

### Bloc E — LED de présence

| LED | Sur | Résistance |
|---|---|---|
| `PWR` | +3V3 | 1 kΩ |
| `MOT` | +5V_MOT | 2,2 kΩ |

La seconde te dit que le contrôleur alimente bien les capteurs. Sans elle, une absence de 5 V côté contrôleur te fait chercher pendant une heure.

### Bloc F — Sorties libres et points de test

**Barrette GPIO libres, 7 trous :**

| GPIO | Type |
|---|---|
| 32 | entrée/sortie |
| 33 | entrée/sortie |
| 34 | entrée seule |
| 36 (VP) | entrée seule, ADC1 |
| 39 (VN) | entrée seule, ADC1 |
| +3V3 | — |
| GND | — |

**15 points de test :** les 12 signaux bufférisés `Mx_Hx_N`, plus 2 GND et 1 +3V3. Zéro composant, uniquement des pastilles.

---

## Récapitulatif des composants ajoutés

| Repère | Composant | Qté | Dans la BOM |
|---|---|---|---|
| U3 | SN65HVD230D SOIC-8 | 1 | oui |
| F1 | PPTC 500 mA | 1 | oui |
| D1 | TVS SMAJ5.0A | 1 | oui |
| D2 | Schottky SS14 | 1 | oui |
| JP1, JP2 | cavalier 2 broches | 2 | non |
| R25 | 120 Ω (terminaison CAN) | 1 | oui |
| R26 | 10 kΩ (Rs) | 1 | oui |
| C15 | 100 nF (découplage CAN) | 1 | oui |
| D3 → D14 | LED 0805 activité | 12 | oui |
| R27 → R38 | 1 kΩ | 12 | oui |
| D15, D16 | LED 0805 présence | 2 | oui |
| R39, R40 | 1 kΩ et 2,2 kΩ | 2 | oui |
| J9, J10 | bornier à vis 2 voies | 2 | oui |
| J11, J12 | embase CAN 3 broches | 2 | non |
| J13 | barrette GPIO 7 broches | 1 | non |
| TP1 → TP15 | points de test | 15 | non |

**≈ 37 composants ajoutés à la nomenclature.** Tous posés par l'assembleur.

---

## Ordre de saisie

1. **Bloc A + B** — entrée 5 V protégée *(le plus critique)*
2. **Bloc C** — CAN
3. **Bloc E** — 2 LED de présence *(entraînement avant les 12)*
4. **Bloc D** — 12 LED d'activité
5. **Bloc F** — barrette GPIO + points de test
6. **ERC** puis circuit imprimé

Validation par bloc, comme jusqu'ici.

---

## Conséquence sur la carte

La carte va passer d'environ 100 × 75 à **120 × 100 mm**, et de 28 à ~65 composants posés. Ça reste une carte 2 couches, sans aucun boîtier à pas fin, et l'assembleur fait tout le CMS. La seule vraie nouveauté électronique est l'émetteur-récepteur CAN — huit broches, un condensateur, deux résistances.
