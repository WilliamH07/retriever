# Plan de conception — P2 « rails d’alimentation »

Date de gel documentaire : 2026-09-16  
État : préparation avant saisie EasyEDA — aucune modification du schéma autorisée tant que les points marqués **GATE** ne sont pas levés.

## 1. Feuille de travail et périmètre

La prochaine feuille à terminer est **P2 — rails d’alimentation**.

Pour éviter de mélanger un bloc prêt avec deux alimentations de puissance encore incomplètes, P2 est traitée en deux lots :

- **P2-A, à réaliser en premier** : bloc 8 `BUS_RAW+ → +5V_HOT`, bloc 9 `+5V_HOT → +3V3`, bloc 12 `+5V_HOT → ESP_5V`.
- **P2-B, différé** : bloc 10 `BUS+ → +12V / 9 A` et bloc 11 `BUS+ → +5V / 5 A`. Les MOSFET, inductances, condensateurs de puissance, réseaux de commande/mesure et vérifications thermiques ne sont pas encore suffisamment définis pour une saisie fiable.

Ordre global après P2-A : P2-B, P3 distribution/fusibles, P4 commande et périphériques, puis P5 freinage après la mesure M15.

## 2. Architecture de P2-A

```text
BUS_RAW+ (batterie permanente, avant contacteur)
    |
    +-- U4 LM5164, buck 300 kHz, 5 V ---------------- +5V_HOT
                                                        |
                                                        +-- U5 AP2112-3.3 -- +3V3
                                                        |
                                                        +-- D3 SS34 -------- ESP_5V --> H2.1 / entrée 5 V DevKitC

GND est commun aux trois blocs.
La broche 3V3 du DevKitC reste non connectée.
```

Le nom du rail régulé par U5 est figé à **`+3V3`**. Ne pas employer `+3V3_ESP`, car l’ESP32 est alimenté par son entrée 5 V et son régulateur embarqué.

## 3. Connexions exactes à réaliser

### 3.1 Bloc 8 — U4 LM5164DDAR, `BUS_RAW+ → +5V_HOT`

| Nœud | Connexions obligatoires |
|---|---|
| `GND` | U4.1 `GND`, pad exposé U4 `EP`, bornes basses C3–C6, C11–C12, R5, R7 et R9 |
| `BUS_RAW+` | U4.2 `VIN`, bornes hautes C3–C6, borne haute R8 |
| `UVLO8` | U4.3 `EN/UVLO`, borne basse R8, borne haute R9 |
| `RON8` | U4.4 `RON` vers R5 41,2 kΩ, puis GND |
| `FB8` | U4.5 `FB`, borne basse R6 100 kΩ, borne haute R7 31,6 kΩ, une borne de C10 |
| `PGOOD8` | U4.6 `PGOOD` avec marqueur No Connect ; sortie open-drain volontairement inutilisée |
| `BST8` | U4.7 `BST`, une borne de C7 2,2 nF |
| `SW8` | U4.8 `SW`, autre borne de C7, entrée L1, entrée R4 |
| `+5V_HOT` | sortie L1, bornes hautes C11–C12, borne haute R6, sortie C9 |
| `RIPPLE8` | sortie R4 220 kΩ, entrée C9 3,3 nF, autre borne C10 270 pF |

Le réseau Type 3 est donc : `SW8 → R4 → RIPPLE8 → C9 → +5V_HOT`, avec `C10` entre `RIPPLE8` et `FB8`.

### 3.2 Bloc 9 — U5 AP2112K-3.3TRG1, `+5V_HOT → +3V3`

| Broche U5 SOT-25 | Connexion |
|---|---|
| 1 `VIN` | `+5V_HOT`, C1 1 µF vers GND |
| 2 `GND` | `GND` |
| 3 `EN` | `+5V_HOT` ; fonctionnement permanent |
| 4 `NC` | marqueur No Connect |
| 5 `VOUT` | `+3V3`, C2 1 µF vers GND |

C1 et C2 doivent être au plus près des broches 1 et 5. La référence réellement associée à LCSC C15849 est Samsung `CL10A105KB8NNNC`, **1 µF, 50 V, X5R, 0603**.

### 3.3 Bloc 12 — alimentation du DevKitC

| Élément | Connexion |
|---|---|
| D3 broche 2 `A` | `+5V_HOT` |
| D3 broche 1 `K` | `ESP_5V_D` |
| `R_ESP_ISO` 0 Ω, 0603 | entre `ESP_5V_D` et `ESP_5V`; retirable pour isoler l’alimentation carte lors du débogage USB |
| `ESP_5V` | H2.1, à confirmer comme entrée 5 V du symbole DevKitC |
| Broche `3V3` du DevKitC | marqueur No Connect |

**GATE ESP-1 :** vérifier dans EasyEDA que `H2.1` correspond bien à l’entrée `5V/EXT_5V` du module réel. Le DevKitC officiel place l’entrée 5 V sur J2.19 ; le symbole projet peut avoir une numérotation différente.

Le DevKitC contient déjà une Schottky entre USB VBUS et `EXT_5V`. D3 crée une seconde branche isolée depuis `+5V_HOT`, ce qui empêche le retour vers le robot, mais Espressif demande officiellement de n’utiliser qu’une seule source d’alimentation. Décision figée : ajouter `R_ESP_ISO = 0 Ω` en série après D3 et documenter deux modes sûrs : batterie avec résistance montée, ou USB alimenté avec résistance retirée/USB VBUS coupé. L’usage simultané reste à valider au banc et n’est pas le mode nominal.

## 4. Revue des composants et verdicts

### 4.1 U4 et passifs du buck 5 V

| Composant | Revue | Verdict |
|---|---|---|
| U4 `LM5164DDAR` | 6–100 V, 1 A, MOSFETs synchrones intégrés, référence FB 1,2 V, bootstrap 2,2 nF, PGOOD open-drain, PowerPAD relié au GND cuivre | **Validé** |
| R5 41,2 kΩ | donne environ 303 kHz avec 5 V | **Validé** |
| R6/R7 100 kΩ / 31,6 kΩ | sortie nominale 4,997 V ; tolérance extrême approximative 4,85 à 5,15 V avec résistances 1 % et référence TI | **Validé** |
| R8/R9 1 MΩ / 68,1 kΩ | UVLO typique 23,53 V ON / 21,96 V OFF ; plage extrême calculée environ 22,3–24,8 V ON et 20,8–23,0 V OFF | **Validé avec plage documentée** |
| C7 2,2 nF / 50 V X7R | valeur explicitement requise par TI entre BST et SW | **Validé** |
| R4/C9/C10 220 kΩ / 3,3 nF / 270 pF C0G | rampe FB calculée ≈20 mV à 42 V ; C9 dépasse le minimum 1,39 nF ; C10 dépasse 250 pF pour 75 µs | **Validé** |
| L1 actuelle `BMRA00060630330MA1` 33 µH | 2 A efficaces, 3 A saturation, DCR max 270 mΩ. À 1 A, L nominale donne ≈1,22 A crête ; avec −20 % de tolérance, ≈1,28 A, au-dessus du seuil minimal de limitation U4 de 1,25 A | **À remplacer** |
| L1 retenue `SRP1038A-470M`, LCSC C3220907 | 47 µH ±20 %, 3 A efficaces, 4,5 A saturation, DCR max 167 mΩ, blindée AEC-Q200, 11 × 10 × 3,8 mm. Avec L à −20 %, le courant crête calculé est ≈1,20 A | **Validé** |
| C3–C6 retenus `C3225X7R2A225KT0L0U`, LCSC C76685 | 2,2 µF, 100 V, X7R, ±10 %, TDK 1210. Même classe électrique que la référence de l’exemple TI, avec modèle DC-bias constructeur disponible | **Validé, quatre exemplaires conservés** |
| C11–C12 `CL21B106KAYQNNE` | 2 × 10 µF, 25 V, X7R. Avec L1 à 47 µH, le calcul demande ≈5,2 µF effectifs minimum | **Validé avec contrôle du modèle DC-bias avant BOM finale** |

Décision figée : remplacer L1 par **Bourns `SRP1038A-470M` / C3220907**. Le boîtier est plus grand que l’inductance actuelle ; son empreinte doit être remplacée avant tout câblage. À 42 V, 5 V et 300 kHz, l’ondulation calculée est ≈0,312 A nominale et ≈0,391 A avec L à −20 %, soit ≈1,20 A crête à 1 A de charge. La perte cuivre maximale à 1 A est ≈0,167 W. Le réseau Type 3 reste valable car la fréquence et le temps de conduction programmé ne changent pas.

Décision figée : remplacer C3–C6 par **TDK `C3225X7R2A225KT0L0U` / C76685**. Le format reste 1210. Quatre exemplaires fournissent une marge importante par rapport au minimum TI de 2,2 µF haute fréquence, même après déclassement sous tension.

### 4.2 U5 et condensateurs 3,3 V

| Composant | Revue | Verdict |
|---|---|---|
| U5 `AP2112K-3.3TRG1` | 600 mA garanti, ±1,5 %, dropout typique 250 mV à 600 mA, EN haut ≥1,5 V, pull-down EN 3 MΩ, stable avec 1 µF en entrée et sortie | **Validé pour la charge projet de 100 mA** |
| C1/C2 `CL10A105KB8NNNC` | 1 µF, 50 V, X5R, ±10 %, 0603 | **Validé ; corriger la description de BOM** |

À 100 mA, U5 dissipe environ `(5 − 3,3) × 0,1 = 0,17 W`. Le rail `+3V3` ne doit pas alimenter l’ESP32 lui-même. Si la somme réelle des périphériques dépasse 150 mA, refaire le bilan thermique avant routage.

### 4.3 D3 et DevKitC

| Composant | Revue | Verdict |
|---|---|---|
| D3 `SS34`, MDD C8678 | 40 V, 3 A, Vf max 0,55 V à 3 A, SMA ; brochage EasyEDA 1=K, 2=A | **Électriquement adapté** |
| DevKitC V4 | entrée 5 V autorisée, régulateur AMS1117 et 22 µF embarqué ; sources USB/5 V/3V3 annoncées mutuellement exclusives par Espressif | **Validé avec R_ESP_ISO et GATE ESP-1** |

La chute « 0,30 V à 0,25 A » ne doit pas être traitée comme une garantie constructeur. L’essai final doit vérifier `ESP_5V` au pic Wi-Fi et le fonctionnement lors du branchement USB.

## 5. Méthode de saisie EasyEDA après levée des GATE

1. Créer un point de restauration de P2.
2. Vérifier symboles, empreintes et correspondance broche/pad de U4, U5, D3 et du DevKitC.
3. Remplacer L1 par C3220907 et C3–C6 par C76685 avant de câbler.
4. Câbler uniquement le bloc 8, puis contrôler chaque nœud avec la table §3.1.
5. Lancer ERC/DRC ; seuls `PGOOD8` et les NC explicitement marqués doivent rester volontairement libres.
6. Câbler le bloc 9, vérifier que le rail s’appelle exclusivement `+3V3`, puis relancer ERC/DRC.
7. Câbler le bloc 12 après validation de H2.1 et de la politique USB, puis relancer ERC/DRC.
8. Faire une revue visuelle à fort zoom des jonctions automatiques EasyEDA et une revue croisée net-par-net.
9. Ajouter les points de test utiles : `BUS_RAW+`, `+5V_HOT`, `+3V3`, `ESP_5V`, `GND`; garder tout point `SW8` très petit et proche de U4/L1.
10. Exporter PDF, netlist et BOM de P2-A pour revue avant de commencer P2-B.

## 6. Critères de sortie de P2-A

- Aucune erreur ERC/DRC non expliquée.
- `PGOOD8`, U5.4 et la broche 3V3 du DevKitC portent un No Connect explicite.
- Toutes les masses de puissance et le pad exposé U4 sont reliés.
- Aucun fil ou label hérité des déplacements précédents ne relie deux nœuds fonctionnels par accident.
- Calcul U4 mis à jour avec L1 = 47 µH et vérification finale des capacités effectives.
- Test prévu à alimentation limitée : démarrage UVLO, 5 V à vide/charge, 3,3 V, pic Wi-Fi, branchement USB et absence de retour de courant.

## 7. Sources constructeur utilisées

- Texas Instruments, LM5164 Rev. D : https://www.ti.com/lit/ds/symlink/lm5164.pdf
- Diodes Incorporated, AP2112 Rev. 2-2 : https://www.diodes.com/datasheet/download/AP2112.pdf
- Yageo Group / Chilisin, série BMRA00060630 : https://www.yageogroup.com/download/datasheet/BMRA00060630220MA1
- Bourns, `SRP1038A-470M` : https://www.bourns.com/docs/Product-Datasheets/SRP1038A.pdf
- TDK, `C3225X7R2A225KT0L0U` (famille C3225 X7R 100 V) : https://product.tdk.com/en/search/capacitor/ceramic/mlcc/info?part_no=C3225X7R2A225K230AB
- Samsung Electro-Mechanics, `CL10A105KB8NNN#` : https://product.samsungsem.com/mlcc/CL10A105KB8NNN.do
- Espressif, guide et schéma ESP32-DevKitC V4 : https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html et https://dl.espressif.com/dl/schematics/esp32_devkitc_v4_sch.pdf
