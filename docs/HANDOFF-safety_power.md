# HANDOFF — carte `safety_power` du robot Husky

**Destinataire : agent de conception (Codex) reprenant la saisie du schéma et le routage.**
Rédigé le 14 septembre 2026. Remplace toute consigne antérieure en cas de contradiction.

---

## 0. LE BUT

Terminer la saisie du schéma puis router la carte `safety_power` du robot Husky, dans **EasyEDA**.

La **feuille 1 est terminée et validée broche par broche** — ne la modifie pas sans raison explicite.
La **feuille 2 est en cours** : le bloc 8 est spécifié ci-dessous, les blocs 9 à 12 aussi.
Les **feuilles 3 à 5 sont à concevoir**.

### Règles de travail, non négociables

1. **Aucun composant n'entre dans le schéma sans avoir été vérifié contre sa fiche technique.** Brochage, maximums absolus, équations de dimensionnement. C'est la règle qui a déjà attrapé six erreurs sur cette carte.
2. **Aucune valeur inventée.** Si une valeur demande un calcul que tu ne peux pas faire (compensation de boucle, par exemple), écris-le explicitement au lieu de proposer un nombre plausible.
3. **Toute référence LCSC doit être vérifiée en stock** avant d'être écrite dans la nomenclature.
4. **Après chaque bloc saisi, vérifier qu'aucune broche n'est en l'air.** Dans EasyEDA : point **vert** au bout d'une broche = connecté, point **gris** = en l'air. Les seuls points gris légitimes sont les ancres d'étiquettes de net.
5. **Ne pas trancher seul les points listés au §11.** Ils appartiennent au propriétaire du projet.

---

## 1. LE SYSTÈME

| | |
|---|---|
| Robot | 4 roues motrices, tout-terrain, ≈ 35 kg, pente ≤ 10 % |
| Batterie | Pack Xiaomi M365, 10S, **42 V**, 7,8 Ah, connecteur XT30 |
| Coupe-batterie | **externe, entre le pack et la carte** — géré hors de cette carte |
| Moteurs | 4 moyeux de trottinette, variateurs **ZS-X11H** |
| Calculateur | Youyeetoo X1, **12 V / 3 A** |
| Contrôle | ESP32 DevKitC V4 sur la carte, bus CAN vers le reste du robot |
| Régime | 15 A continu, 30 A crête légitime |

---

## 2. LE PRINCIPE DIRECTEUR

> **L'état sûr est l'état par défaut. L'état utile demande une action.**

Concrètement, et vérifiable composant par composant :

- Les grilles des transistors de puissance sont tirées vers le bas. Plus de commande → plus de courant.
- `INP` et `INP_G` ont des tirages internes vers la masse. ESP32 en reset → contacteur ouvert.
- La décharge du bus **s'alimente sur le bus lui-même**. Il faut que l'ESP32 l'**inhibe activement** pour rouler.
- Le rail logique se prend **avant** le contacteur, sinon l'ESP32 ne peut pas le fermer (voir §4).

Second principe :

> **Une LED sur une commande ment. Une LED sur une mesure dit la vérité.**

`LED_BUS` est câblée sur `BUS+`, pas sur l'ordre de fermeture. Le diagnostic mesure les trois nœuds physiques du contacteur au lieu de faire confiance aux ordres envoyés.

---

## 3. ÉTAT D'AVANCEMENT

| Feuille | Contenu | État |
|---|---|---|
| **1** | entrée, contacteur, précharge, diagnostic, décharge, LED, mesure, 28 points de test | ✅ **validée le 10 sept.** |
| **2** | rails d'alimentation | 🔨 bloc 8 spécifié, à saisir |
| **3** | distribution, fusibles, borniers | à concevoir |
| **4** | ESP32, CAN, watchdog, expander, périphériques | à concevoir |
| **5** | hacheur de freinage | à concevoir — **voir §11.3** |

---

## 4. ARCHITECTURE DES RAILS

```
coupe-batterie externe
        │
       XT30 (CN1)
        │
    [shunt 0,5 mΩ]
        │
    BUS_RAW+ ──┬───────────────────────────────── Q1 · Q2 ───── BUS+ ──┬── J3..J6  42 V fusés
               │                                (contacteur)           │   (4 × ZS-X11H, feuille 3)
               │                                                       │
               ├── BLOC 8   42 → 5 V, 1 A  ──── +5V_HOT                ├── BLOC 10  42 → 12 V, 9 A ── +12V_AUX
               │            (LM5164)              │                    │            (LM5145)           │
               │                                  ├── BLOC 12          │                               ├── Youyeetoo X1  3 A
               │                                  │   Schottky →       │                               ├── 2 ventilateurs PWM  0,4 A
               │                                  │   broche 5 V       │                               └── J8 bornier 12 V  5 A
               │                                  │   du module ESP32  │
               │                                  │                    └── BLOC 11  42 → 5 V, 5 A ── +5V_PWR
               │                                  └── BLOC 9                       (LM5145)           │
               │                                      LDO 3,3 V ── +3V3                               ├── bandeau 20 × WS2812B  1,2 A
               │                                                   └ INA228, ADS1115,                 └── J9 bornier 5 V  3 A
               │                                                     tirages I²C, LED
               └── TPS48111 (VS) · diviseur DIAG_RAW
```

### ⚠️ Le piège qui gouverne cette architecture

Si le convertisseur 5 V logique se branche sur `BUS+`, **l'ESP32 n'est pas alimenté tant que le contacteur est ouvert — et c'est lui qui doit le fermer.** La carte ne démarre jamais.

D'où : **le rail hôtel (`+5V_HOT`) se prend sur `BUS_RAW+`, avant le contacteur.** C'est ce que signifie le suffixe `_HOT`.

### Table des rails

| Net | Tension | Prise | Charge |
|---|---|---|---|
| `BUS_RAW+` | 42 V | après le shunt, avant `Q1` | TPS48111, diviseur `DIAG_RAW`, bloc 8 |
| `+5V_HOT` | 5 V · 1 A | bloc 8 | module ESP32, LDO 3,3 V |
| `+3V3` | 3,3 V · 100 mA | bloc 9 | INA228, ADS1115, tirages I²C, LED, expander |
| `BUS+` | 42 V | après `Q2` | 4 × ZS-X11H fusés, blocs 10 et 11 |
| `+12V_AUX` | 12 V · 9 A | bloc 10 | X1, ventilateurs, bornier 12 V |
| `+5V_PWR` | 5 V · 5 A | bloc 11 | bandeau WS2812B, bornier 5 V |

**Blocs 10 et 11 en parallèle depuis `BUS+`, pas en cascade.** À 5 A, un 42→5 V direct donne 397 ns de temps de conduction, soit dix fois le minimum du LM5145 (40 ns). Chaque convertisseur est plus petit, le 5 V ne subit qu'une conversion, et une panne de l'un ne tue pas l'autre.

---

## 5. NOMENCLATURE — FEUILLE 1 (validée, ne pas modifier)

### Actifs

| Rep. | Composant | Boîtier | LCSC |
|---|---|---|---|
| `U1` | TPS48111LQDGXRQ1 | VSSOP-19 | **C17558994** |
| `U2` | INA228AIDGSR | VSSOP-10 | **C2887910** |
| `U3` | ADS1115IDGSR | VSSOP-10 | **C37593** |
| `Q1` `Q2` `Q3` `Q4` | IPB017N10N5, 100 V, 1,7 mΩ | D²PAK-7 | **C536479** |
| `Q5` | DMG1012T-7 | **SOT-523** | **C20512** |
| `D1` | SMCJ48CA — TVS bidirectionnelle | SMC | à confirmer |
| `D2` | SS310 | SMA | à confirmer |
| `D_Z4` | BZT52C12 — zener 12 V | SOD-123 | **C43491** |
| `D_DS` `D_DB` `D_DA` | BAT54S — **suffixe S obligatoire** | SOT-23 | **C83935** |
| LED vertes ×4 | KT-0603G | 0603 | **C12624** |
| LED rouges ×2 | KT-0603R | 0603 | **C2286** |
| LED jaunes ×2 | KT-0603Y | 0603 | **C2287** |

### Passifs

| Rep. | Valeur | LCSC / MPN |
|---|---|---|
| `R4a` `R4b` | 1 mΩ ×2 **en parallèle** = 0,5 mΩ | **C500734** |
| `R_VS` `R_G1` `R_G2` | **0 Ω** | **C21189** |
| `R_SET` | 100 Ω 1 % | `0603WAF1000T5E` |
| `R_G3` | 220 Ω | **C22962** |
| `R_ISCP` | 1,5 kΩ 1 % → seuil 61,3 A | `0603WAF1501T5E` |
| `R_IWRN` | 59 kΩ 1 % → seuil 40 A | `0603WAF5902T5E` |
| `R_TMR` `R_G4` `R_G5` | 100 kΩ | **C25803** |
| `R_GS3` | **1 MΩ** | `0603WAF1004T5E` |
| `R_UV1` / `R_UV2` | 470 kΩ / 24,9 kΩ → UVLO 23,9 V | `0603WAF4703 / WAF2492` |
| `R_D?1a` `R_D?1b` ×6 | 47 kΩ 1 % | `0603WAF4702T5E` |
| `R_D?2` ×3 | 6,8 kΩ 1 % | `0603WAF6801T5E` |
| `R_D?3` ×3, `R_S1..S5`, `R_L1` `R_L5..L8` | 1 kΩ | **C21190** |
| `R_FLT1` `R_FLT2` `R_ALERT` `R3` | 10 kΩ | **C25804** |
| `R_L2` | 2,2 kΩ | `0603WAF2201T5E` |
| `R_SDA` `R_SCL` | 2,2 kΩ | `0603WAF2201T5E` |
| `R_L3` | 6,8 kΩ | `0603WAF6801T5E` |
| `R_L4a` `R_L4b` | 20 kΩ **en série** | `0603WAF2002T5E` |
| `R_F1` `R_F2` | 10 Ω | `0603WAF100JT5E` |
| `R_DIS1..6` | 2 kΩ 2512 1 W, **6 en parallèle** = 333 Ω | `25121WF2001T4E` |
| `R_PRECH` | 10 Ω 10 W ciment axial, 48 × 9,5 mm | **C1527343** (SQP10AJB-10R) |
| `C_IN1..4` | 10 µF 100 V X7S 1210 | **C576517** |
| `C_TMR` | **470 nF** ≥ 16 V | à confirmer |
| `C_VS2` `C_BST` `C_F` | 1 µF 0805 | **C28323** |
| `C_VS1` `C_VS3` `C_EN` `C_VDD3` `C_D?` | 100 nF 0603 | **C14663** |
| `C_ISCP` | 1 nF 0603 | **C1588** |

### Connecteurs feuille 1

| Rep. | Type | Rôle |
|---|---|---|
| `CN1` | XT30PW-M30 | entrée batterie |
| `CN2` | XY-HY2.0-3PWZ | liaison BMS — **bloc gelé, validé au banc, ne rien y changer** |
| `H1` `H2` | 2,54 mm × 19 points | module ESP32 DevKitC |

### Les seuils de la feuille 1

| Grandeur | Valeur | Composant qui la fixe |
|---|---|---|
| Continu | 15 A | — |
| Crête légitime | 30 A | — |
| Surintensité, coupure après **7,4 ms**, **verrouillante** | **40 A** | `R_IWRN` 59 kΩ + `C_TMR` 470 nF |
| Court-circuit, coupure en **1,2 µs** | **61,3 A** | `R_ISCP` 1,5 kΩ |
| UVLO du contrôleur | 23,9 V | `R_UV1` / `R_UV2` |
| Rapport des diviseurs de diagnostic | 14,82 → 2,833 V à 42 V | 47 k + 47 k / 6,8 k |
| Résolution INA228 | 156 µA par pas | shunt 0,5 mΩ, ADCRANGE = 1 |
| Résolution ADS1115 | 1,85 mV par pas ramené au bus | PGA ±4,096 V |

---

## 6. NOMENCLATURE — FEUILLE 2

### BLOC 8 — 42 → 5 V, 1 A, rail hôtel, depuis `BUS_RAW+`

**`U4` = LM5164DDAR, SO-8 PowerPAD, LCSC C477928.**

Brochage réel — **à vérifier contre le symbole EasyEDA avant de câbler** :
`1 GND · 2 VIN · 3 EN/UVLO · 4 RON · 5 FB · 6 PGOOD · 7 BST · 8 SW` + semelle thermique.

⚠️ **Le LM5164 n'a PAS de broche VCC.** Son régulateur de grille est interne. Ne pose aucun condensateur de VCC.
⚠️ **`PGOOD` (broche 6) reste NON connectée.** Le seul composant qui pourrait la lire, l'ESP32, est alimenté par ce rail. `LED_5V` donne déjà l'information.

| Rep. | Valeur | Boîtier | Réf. | Rôle |
|---|---|---|---|---|
| `U4` | LM5164DDAR | SO-8 PowerPAD | **C477928** | 100 V, 1 A, 3 µA en veille |
| `L8` | 33 µH, Isat ≥ 2 A | 7,3 × 6,6 mm | `BMRA00060630330MA1` | inductance blindée, 3 A |
| `C_IN8` ×4 | 2,2 µF **100 V** X7R | 1210 | à choisir | TI exige une tenue de 2 × Vin |
| `C_O8` ×2 | 10 µF 25 V X7R | 0805 | `CL21B106KAYQNNE` | ondulation 25 mV |
| `C_BST8` | **2,2 nF** 50 V X7R | 0603 | à choisir | ⚠️ **2,2 nF, pas 100 nF** |
| `R_A8` | **220 kΩ 1 %** | 0603 | à choisir | injection d'ondulation Type 3, de `SW` vers `C_A8` |
| `C_A8` | **3,3 nF** 50 V X7R | 0603 | à choisir | injection Type 3, entre `R_A8` et `+5V_HOT` |
| `C_B8` | **270 pF** 50 V **C0G/NP0** | 0603 | à choisir | couplage du nœud `R_A8`/`C_A8` vers `FB` |
| `R_RON` | 41,2 kΩ 1 % | 0603 | `0603WAF4122T5E` | 300 kHz |
| `R_FB1` / `R_FB2` | 100 kΩ / 31,6 kΩ 1 % | 0603 | `0603WAF1003 / WAF3162` | Vref = 1,2 V |
| `R_UV3` / `R_UV4` | 1 MΩ / **68,1 kΩ** 1 % | 0603 | `0603WAF1004T5E` / `0603WAF6812T5E` | démarre ≈ 23,53 V, s'arrête ≈ 21,96 V |

Câblage : `VIN`←`BUS_RAW+` avec les `C_IN8` · `EN/UVLO`← milieu `R_UV3`/`R_UV4` · `RON`→`R_RON`→`GND` · `SW`→`L8`→`+5V_HOT` avec les `C_O8` · `C_BST8` entre `BST` et `SW` · `FB`← milieu `R_FB1`/`R_FB2` · réseau Type 3 : `SW`→`R_A8`→nœud `RIPPLE8`→`C_A8`→`+5V_HOT`, avec `C_B8` entre `RIPPLE8` et `FB`.

⚠️ **Le réseau Type 3 est obligatoire avec les condensateurs de sortie céramiques choisis.** Sans une ondulation en phase suffisante sur `FB`, le contrôle COT peut produire des salves de commutation instables. Dimensionnement retenu sur la base des équations 24 à 26 de la fiche TI LM5164 Rev. D :

```
R_FB1 || R_FB2 = 24,0 kΩ
C_A8,min = 10 / (Fsw × (R_FB1 || R_FB2)) = 1,39 nF  → 3,3 nF
ΔV_FB ≈ tON × (Vin − Vout) / (R_A8 × C_A8)
       ≈ 20 mV à Vin = 24,1 V avec R_A8 = 220 kΩ
C_B8,min = 75 µs / (3 × R_FB1) = 250 pF  → 270 pF C0G
```

Le choix de `C_B8` correspond à un objectif documenté de stabilisation de **75 µs**, identique à l'exemple de calcul TI. Il devra être vérifié au banc sur échelon de charge avant validation finale.

Équations utilisées :
```

Références provisoires retenues pendant la saisie EasyEDA du 14 septembre 2026 :

- `C_IN8` ×4 : **remplacement décidé le 16 septembre 2026** par TDK `C3225X7R2A225KT0L0U`, LCSC **C76685**, 2,2 µF ±10 %, 100 V, X7R, 1210. Les `C106116` déjà placés doivent être remplacés avant câblage.
- `C_BST8` : FH `0603B222J500NT`, LCSC **C60328**, 2,2 nF ±5 %, 50 V, X7R, 0603.
- `C_A8` : CCTC `TCC0603X7R332K500CT`, LCSC **C282680**, 3,3 nF ±10 %, 50 V, X7R, 0603.
- `C_B8` : HRE `CSA0603C0G271J500JT`, LCSC **C20069470**, 270 pF ±5 %, 50 V, C0G, 0603. `C107046` (Yageo, NP0) est une alternative compatible.
- `R_A8` : Uniroyal `0603WAF2203T5E`, LCSC **C22961**, 220 kΩ ±1 %, 0603.
- `R_RON` : Uniroyal `0603WAF4122T5E`, LCSC **C23166**, 41,2 kΩ ±1 %, 0603.
- `R_FB1` / `R_FB2` : Uniroyal `0603WAF1003T5E` / `0603WAF3162T5E`, LCSC **C25803** / **C25967**, 100 kΩ / 31,6 kΩ ±1 %, 0603.
- `R_UV3` / `R_UV4` : Uniroyal `0603WAF1004T5E` / `0603WAF6812T5E`, LCSC **C22935** / **C25976**, 1 MΩ / 68,1 kΩ ±1 %, 0603. La valeur 66,5 kΩ initialement visée n'avait pas de symbole implantable dans la bibliothèque utilisée ; avec les seuils typiques TI 1,5 V/1,4 V, le couple retenu donne environ 23,53 V à l'enclenchement et 21,96 V au déclenchement.
- `C_O8` ×2 : Samsung `CL21B106KAYQNNE`, LCSC **C3039694**, 10 µF ±10 %, 25 V, X7R, 0805.
- `L8` : **remplacement décidé le 16 septembre 2026** par Bourns `SRP1038A-470M`, LCSC **C3220907**, 47 µH ±20 %, `Irms` 3 A, `Isat` 4,5 A, DCR max 167 mΩ, boîtier 11 × 10 × 3,8 mm. Le `C2838325` 33 µH déjà placé doit être remplacé avec son empreinte avant câblage ; voir `PLAN-P2-rails-power.md`.

Ces références fixent les caractéristiques électriques et les empreintes pour la saisie, mais restent soumises à une vérification de stock au moment de l'approvisionnement.
R_RON (kΩ)  = Vout (V) × 2500 / Fsw (kHz)        →  5 × 2500 / 300 = 41,7 → 41,2 kΩ
L           = Vout / (Fsw × ΔIL) × (1 − Vout/Vin) →  33 µH pour ΔIL = 0,45 A
C_OUT       = ΔIL / (8 × Fsw × Vripple)           →  7,5 µF minimum
R_FB2       = 1,2 / (Vout − 1,2) × R_FB1
Vin_on      = 1,5 × (1 + R_UV3/R_UV4)   Vin_off = 1,4 × (…)
```

### BLOC 9 — 5 → 3,3 V, 100 mA

| Rep. | Valeur | Boîtier | LCSC |
|---|---|---|---|
| `U5` | AP2112K-3.3TRG1, 600 mA, 55 µA | SOT-25 | **C51118** |
| `C_L1` `C_L2` | Samsung `CL10A105KB8NNNC`, 1 µF 50 V X5R | 0603 | **C15849** |

### BLOC 10 — 42 → 12 V, 9 A, depuis `BUS+`
### BLOC 11 — 42 → 5 V, 5 A, depuis `BUS+`

**Topologie strictement identique. Même contrôleur, même inductance de 10 µH, même résistance de fréquence de 133 kΩ.**

| Rep. | Valeur | LCSC | Note |
|---|---|---|---|
| `U6` `U7` | LM5145RGYR, VQFN-20 | **C485912** | 6–75 V, abs. max 105 V |
| `Q8` `Q9` ×4 | **100 V · ≤ 10 mΩ · Qg ≤ 30 nC**, boîtier à semelle | **à trouver** | voir §10.2 |
| `L10` `L11` | 10 µH moulée, DCR ≤ 5 mΩ | à choisir | Isat ≥ 11 A / ≥ 6 A |
| `R_RT` ×2 | 133 kΩ 1 % → 300 kHz | `0603WAF1333T5E` | Fsw(kHz) = 4·10⁴ / R(kΩ) |
| `R_UV5` ×2 | 1 MΩ 1 % | à choisir | |
| `R_UV6` ×2 | 34,8 kΩ 1 % → **démarre à 35,7 V** | `0603WAF3482T5E` | voir §8.4 |
| `Q7` ×2 | DMG1012T-7 | **C20512** | inhibition par `EN_12V` / `EN_5V` |
| `C_VCC` ×2 | Samsung `CL10A105KB8NNNC`, 1 µF 50 V X5R | **C15849** | description corrigée le 16 septembre 2026 |
| `C_BST` ×2 | 100 nF 50 V X7R | **C14663** | ⚠️ 100 nF ici, contrairement au bloc 8 |

Différences entre les deux :

| | BLOC 10 — 12 V | BLOC 11 — 5 V |
|---|---|---|
| Courant | 9 A | 5 A |
| Rapport cyclique | 28,6 % | 11,9 % |
| Temps de conduction | 952 ns | 397 ns (min. 40 ns) |
| Ondulation dans L | 2,86 A (32 %) | 1,47 A (29 %) |
| Isat | ≥ 11 A | ≥ 6 A |
| `C_IN` | 4 × 10 µF 100 V + 100 µF | 2 × 10 µF 100 V + 100 µF |
| Courant efficace en entrée | 4,07 A | 1,62 A |
| `C_OUT` | 4 × 22 µF 25 V | 4 × 22 µF 16 V |
| `R_F1` / `R_F2` | 10 kΩ / 715 Ω | 10 kΩ / 1,91 kΩ |

Détection de courant : **sur le RDS(on) de `Q9`** (le LM5145 le permet, pas de shunt).

### BLOC 12 — alimentation du module ESP32

`+5V_HOT` → **`D3` = SS34 (LCSC C8678)** → `R_ESP_ISO` 0 Ω 0603 retirable → broche **5 V** (`H2` broche 1) du module. L’ajout de `R_ESP_ISO` a été décidé le 16 septembre 2026 pour pouvoir isoler l’alimentation carte pendant le débogage USB ; voir `PLAN-P2-rails-power.md`.

⚠️ **Laisser la broche `3V3` du module NON connectée.** Sinon deux régulateurs se retrouvent en parallèle dès que quelqu'un branche l'USB.

Marge vérifiée : à 0,25 A la Schottky chute 0,30 V → 4,70 V au module, dont le régulateur a besoin de 3,9 V. Tient aussi à la pointe Wi-Fi de 0,5 A.

---

## 7. FEUILLES 3, 4 ET 5 — À CONCEVOIR

### Feuille 3 — distribution et fusibles
- 4 branches depuis `BUS+` vers les ZS-X11H : `J3` à `J6`, **fusible 15 A par branche**
- Détection de fusible fondu
- `J7` Youyeetoo X1, 12 V / 3 A
- `J8` bornier 12 V, 5 A · `J9` bornier 5 V, 3 A

⚠️ **Pas de fusible de tête.** Un 40 A en tête ferait fondre le fusible de branche *et* celui de tête sur un court-circuit franc : on perdrait le robot entier au lieu d'une roue, et surtout l'information de localisation du défaut.

### Feuille 4 — ESP32 et périphériques
Module ESP32, transceiver CAN (SN65HVD230), watchdog TPL5010, expander I²C PCA9555, champignon d'arrêt d'urgence et bus `/SAFE`, 2 ventilateurs PWM 12 V, bandeau 20 × WS2812B, connecteurs `J13` (CAN) et `J15` (`/SAFE`).

### Feuille 5 — hacheur de freinage
Comparateur analogique à seuil 42,5 V + `Q6` 100 V + résistance de dissipation, sur `BUS+`.
**Doit fonctionner sans l'ESP32** — auto-alimenté sur `BUS+`, comme la décharge du bloc 5.
**Voir §11.3 avant de le concevoir : il dépend d'une mesure jamais faite.**

---

## 8. DÉCISIONS DE CONCEPTION À RESPECTER

**8.1 — `Q1`/`Q2` en source commune, dos à dos.** Les deux diodes de corps ont leur anode au nœud `SRC` et pointent vers l'extérieur. C'est la seule disposition qui bloque la décharge *et* la régénération. Elle impose que la grille commune soit référencée à `SRC`, qui monte à 42 V — d'où la pompe de charge du TPS48111.

**8.2 — `Q3` de précharge ne contourne que `Q1`.** Sa source est au nœud `SRC` : c'est la seule position où la broche `G` peut le piloter, puisque `BST` est référencé à `SRC`. `R_PRECH` va donc **côté drain**. Le courant revient au `BUS+` par la diode de corps de `Q2`.
→ Conséquence : **le bus s'arrête ≈ 0,5 V sous `BAT+`. Le seuil « bus chargé » est à 95 %, pas 99 %.**

**8.3 — Les branches hautes des diviseurs de diagnostic sont en deux résistances.** Un 0603 tient 50 V de service ; une seule 94 kΩ verrait 39 V puis 60 V sur un transitoire. Deux en série : 20 V chacune. Même raison pour `R_L4a`/`R_L4b`.

**8.4 — Les blocs 10 et 11 démarrent à 35,7 V, pas à zéro.** Ce n'est pas de la protection : c'est pour qu'ils **ne chargent pas pendant la précharge**. À 35,7 V le bus est à 85 %, la précharge est presque finie. Un seuil plus bas empêcherait la précharge d'atteindre ses 95 %.

**8.5 — Six résistances de décharge, pas une.** La contrainte dimensionnante n'est pas l'impulsion mais le **défaut permanent** : décharge restée active en marche = 5,35 W en continu. Six 2512 se le partagent à 0,88 W chacune, sous le watt nominal. Le défaut devient tenable *par construction*, sans dépendre du firmware.

**8.6 — Le shunt fait 0,5 mΩ avec l'INA228 en calibre fin.** Même performance de mesure qu'un 2 mΩ en calibre large, mais 0,45 W au lieu de 1,8 W à 30 A.

**8.7 — Aucun signal de sécurité ne passe par l'expander I²C.** `INP`, `INP_G`, `DISCH_INH`, `FLT_I`, `FLT_T`, le champignon : ils doivent survivre à un bus I²C planté. Aucun signal chronométré non plus (WS2812B, PWM, CAN, watchdog). Seuls y passent : `STAT_G`, `STAT_R`, `EN_12V`, `EN_5V`, `INA_ALERT`, `FAN_TACH`.

---

## 9. CONTRAINTES DE ROUTAGE — INVISIBLES AU CONTRÔLE DE RÈGLES

Aucune de ces cinq contraintes ne sera signalée par le DRC. Elles doivent être appliquées à la main.

1. **Les quatre pistes Kelvin partent au ras des pastilles du shunt**, en pistes fines dédiées, jamais sur le cuivre de puissance. Deux pour le TPS48111, deux pour l'INA228. Le signal utile fait 20 mV à 40 A ; la chute dans le cuivre s'y ajouterait.
2. **`TP3` + `TP10` + `TP11` + `TP12` en grappe serrée.** Ces quatre points se mesurent par rapport à `SRC`, pas à la masse — il faut pouvoir poser les deux pointes ensemble. Idem **`TP6` collé à `TP7`**, au ras du shunt.
3. **`TP5` (la masse unique) au centre de la carte**, pas au bord.
4. **Les six 2512 de décharge étalées, pas groupées.** 0,88 W sur une pastille 2512 monte à ≈ 60 °C au-dessus de l'ambiant, et il y en a six.
5. **`R_PRECH` sur un bord de carte**, surélevée de 2 à 3 mm, zone d'exclusion de 5 mm, jamais au-dessus d'un électrolytique ni du module ESP32. En défaut elle dissipe 176 W dans un boîtier de 10 W.

Pour la feuille 2, s'ajoutent :

6. **Les semelles thermiques des convertisseurs se relient à la broche GND et à un grand plan de cuivre.** C'est leur seul chemin thermique.
7. **Les condensateurs d'entrée au plus près des broches VIN et GND.** C'est la boucle qui commute 42 V en quelques nanosecondes ; chaque millimètre de piste y ajoute du dépassement sur `SW`.
8. **Les `C_IN` de la feuille 1 répartis le long du chemin `BUS_RAW+` → drain de `Q1`**, boucle la plus courte possible.

---

## 10. LES LIMITES CONNUES

**10.1 — Le budget de broches de l'ESP32 est à zéro.** 21 signaux pour 21 broches utilisables, une fois retirés le flash, l'USB et les deux prises par le BMS. Le PCA9555 de la feuille 4 rend six broches et n'est pas optionnel si un capteur doit être ajouté plus tard.

**10.2 — Le MOSFET des blocs 10 et 11 n'est pas choisi.** Cahier des charges : 100 V, ≤ 10 mΩ, **≤ 30 nC de charge de grille**, boîtier à semelle thermique. Le `SiR870ADP` correspond mais son stock LCSC n'est pas confirmé.
⚠️ **Ne pas réutiliser l'`IPB017N10N5`** des blocs 1 à 5 : ses 180 nC donneraient 3,4 W de pertes de commutation sur un convertisseur de 108 W. Il est parfait sur le contacteur, qui ne commute jamais ; catastrophique dans un buck à 300 kHz.

**10.3 — La compensation des blocs 10 et 11 n'est pas calculée.** `R_C`, `C_C1`, `C_C2` dépendent de l'ESR réelle des condensateurs de sortie et du gain de boucle. **Ne pas inventer ces valeurs** — un convertisseur instable en résulterait. Passer par WEBENCH : 42 V en entrée, 12 V / 9 A puis 5 V / 5 A.

**10.4 — L'INA228 a 85 V de maximum ABSOLU**, pas de plage d'emploi, sur `IN+`, `IN−` et `VBUS`. Il est branché sur `BUS_RAW+`, le nœud qui prend le dépassement inductif. Les quatre `C_IN` de 10 µF sont ce qui le protège : sans eux le nœud monterait à 113 V, avec eux il plafonne à 64 V au pire.

**10.5 — Aucune protection contre l'inversion de polarité en entrée.** `VS` du TPS48111 ne tient que −1 V. Le détrompage du XT30 est la seule barrière.

**10.6 — Le XT30 est donné pour 30 A**, soit le goulot d'étranglement de la chaîne. Choix assumé : c'est le connecteur natif du pack.

---

## 11. CE QUI RESTE OUVERT — NE PAS TRANCHER SEUL

**11.1 — Forme physique de `R_PRECH`.** Le `SQP` ciment traversant réserve ≈ 65 × 22 mm sur un bord et demande une soudure manuelle. Dix 2512 de 100 Ω à plat sont montées automatiquement mais brûlent si `Q3` reste collé sur un bus en court-circuit. Décision du propriétaire.

**11.2 — Rétro-alimentation par les moteurs.** Constat de terrain : tourner une roue à la main allume les cartes et fait bouger les trois autres moteurs. Mécanisme : le moteur BLDC entraîné est un alternateur, les six diodes de corps du pont du ZS-X11H le redressent passivement, et la tension apparaît sur `BUS+` — **c'est-à-dire en aval du contacteur**.

Conséquence : l'hypothèse « contacteur ouvert ⇒ bus mort » est fausse. **Règle de maintenance : bloquer les roues ou débrancher les moteurs avant d'intervenir. Le coupe-batterie n'y change rien, il est en amont.**

Le diagnostic à trois voies distingue déjà ce cas d'un transistor collé : roue qui tourne → `DIAG_BUS` élevé et `DIAG_SRC` ≈ 0, parce que la diode de corps de `Q2` empêche le bus de remonter vers `SRC`. Un `Q2` collé donnerait `DIAG_SRC` = `DIAG_BUS`.

Trois options, **non tranchées** :
- rien, plus le hacheur de freinage (≈ 4 €)
- diode idéale par branche, LM74700 + N-FET (≈ 12 €) — bloque aussi la régénération légitime
- FET série piloté par l'ESP32 par branche (≈ 30 €) — résout tout, quadruple le contacteur

**Mesure préalable indispensable** : combien de volts apparaissent sur `BUS+` (`TP4` / `TP5`) quand on tourne une roue à la main, puis quand on pousse le robot au pas. Entre 5 V et 40 V, ce ne sont pas les mêmes options qui se justifient.

**11.3 — La feuille 5 dépend d'une mesure de vingt minutes.** Si le BMS du M365 accepte la charge par son port de décharge, le pack absorbe la régénération et le hacheur ne sert à rien. Test : alimentation de labo bridée, pousser 43 V sur le XT30, regarder si le courant entre. **Faire ce test avant de dessiner la feuille 5.**

---

## 12. MESURES BLOQUANTES AVANT ROUTAGE

| # | Mesure | Ce qu'elle décide |
|---|---|---|
| M1 | Seuil de coupure basse du BMS | la marge réelle sous les 23,9 V de l'UVLO |
| M2 | Résistance interne du pack | la chute aux pointes, donc la validité du seuil de 95 % |
| M10 | Capacité d'entrée totale du bus (4 × ZS-X11H) | la durée de précharge et de décharge — **aucune puissance n'en dépend** |
| M12 | Tenue en tension des condensateurs d'entrée des ZS-X11H | si l'écrêtage à 61 V du bus leur convient |
| M13 | Consommation de repos du bus | les 200 mA supposés fixent `R_PRECH` à 10 Ω |
| M14 | Tension générée par une roue tournée à la main | §11.2 |
| M15 | Le BMS accepte-t-il la charge par le port de décharge | §11.3 |

---

## 13. ERRATA — ERREURS DÉJÀ COMMISES, NE PAS LES REFAIRE

| Sujet | L'erreur | Pourquoi |
|---|---|---|
| `R_VS` | 100 Ω | `R_SET` se référence à la **broche** `VS`, pas au bus : 300 mV d'erreur sur un signal de 20 mV. Doit valoir **0 Ω**. |
| `R_PRECH` | 25 W sous alu, ≈ 20 € | Dimensionnement de service continu appliqué à une impulsion. 8,8 J font monter un corps de ciment de 3 °C. |
| `C_TMR` | 68 nF (1,1 ms) | Ne distingue pas une surcharge d'une pointe d'accélération, et le défaut est verrouillant. **470 nF → 7,4 ms**. |
| Nets Kelvin | `R_CO_1` / `R_CO_2` | La contrainte Kelvin est **physique, pas nominale**. Les nets s'appellent `BAT+` et `BUS_RAW+`. |
| `C_VCC8` | condensateur de VCC sur le LM5164 | **Le LM5164 n'a pas de broche VCC.** |
| Tirages I²C | `R_SCL` et `R_SDA` en série | En chaîne, `SDA` est tiré vers `SCL` : la donnée suit l'horloge. **Deux branches séparées depuis le même `+3V3`.** |
| MOSFET de buck | réutiliser l'`IPB017N10N5` | 180 nC de charge de grille → 3,4 W de pertes de commutation. |
| Isolation | « isoler les quatre borniers » | Quatre charges sur un bus continu partagent forcément le + et le −. Ce qui est possible, c'est **bloquer le retour**, pas isoler. |
| Boîtier de `Q5` | SOT-23 | Le `DMG1012T-7` de Diodes Incorporated est livré en **SOT-523**. L'empreinte EasyEDA est correcte ; l'ancienne nomenclature ne l'était pas. |
| Ondulation LM5164 | seulement `R_FB1` / `R_FB2` avec `C_OUT` céramiques | Le contrôle COT demande une ondulation en phase sur `FB`. Ajouter le réseau Type 3 `R_A8` / `C_A8` / `C_B8` défini au §6. |

---

## 15. REVUE EASYEDA — 14 SEPTEMBRE 2026

Revue effectuée sur la feuille 1 du schéma local `Power Interface` avant poursuite de la saisie.

- DRC initial : **0 erreur fatale, 0 erreur, 9 avertissements, 40 informations**.
- Après pose du marqueur No Connect sur `U3.2` : **0 erreur fatale, 0 erreur, 7 avertissements, 39 informations** (46 signalements au total).
- Topologies contrôlées visuellement : `Q1`/`Q2` à sources communes, précharge `BUS_RAW+ → R_PRECH → Q3 → SRC`, brochage TPS48111, mesure INA228 et trois voies ADS1115.
- `U3.2` (`ALERT/RDY`) est volontairement inutilisée : marqueur No Connect posé et vérifié par un nouveau DRC le 14 septembre 2026.
- Les broches encore flottantes de `H1`/`H2` doivent être résolues par les connexions de la feuille 4 ou marquées No Connect lors de la revue finale.
- `CN2.3` appartient au bloc BMS gelé : ne pas modifier son câblage ; documenter son absence de connexion par un marqueur No Connect uniquement si cela ne change pas le bloc validé au banc.
- Sept objets portent encore des désignateurs internes `$1I113`, `$1I114`, `$1I117`, `$1I118`, `$1I119`, `$1I120`, `$1I213` : identifier leur nature puis les normaliser avant export de la nomenclature.
- EasyEDA contient `D1 = SMCJ48CA`, LCSC **C3039961**. Cette référence est annoncée non recommandée pour les nouveaux designs et son écrêtage maximal publié est de **77,4 V**. Ne pas figer la BOM avant sélection d'une TVS pérenne et vérification de la marge transitoire vis-à-vis de l'INA228 (85 V absolu).
- EasyEDA contient `D_DS`, `D_DB`, `D_DA = BAT54S`, LCSC **C51898308**, au lieu de C83935. La fonction et le brochage sont compatibles, mais ce fabricant alternatif doit être qualifié avant gel de la BOM.

### Poursuite de la saisie — feuille 2 et structure du schéma

- Le schéma EasyEDA comporte désormais les **cinq feuilles prévues** : P1 existante, P2 alimentations, P3 distribution/fusibles, P4 commande/périphériques et P5 hacheur de freinage. P3 à P5 ont été créées vides pour préserver la séparation fonctionnelle.
- P5 reste volontairement vide tant que la mesure **M15** n'a pas déterminé si le BMS accepte la charge par le port de décharge. Aucun seuil ni composant de puissance n'a été inventé.
- Sur P2, `U4 = LM5164DDAR` (**C477928**) et `U5 = AP2112K-3.3TRG1` (**C51118**) ont été placés avec leurs symboles et empreintes constructeur vérifiés.
- `D3 = SS34` (**C8678**) a été placée pour l'alimentation protégée du module ESP32.
- Deux condensateurs 1 µF `C1`/`C2` (**C15849**) ont été placés autour de `U5`. Au moment de la revue, la disponibilité directe LCSC était nulle mais le stock d'assemblage JLCPCB était disponible ; revalider ce point avant commande.
- Quatre condensateurs d'entrée 2,2 µF / 100 V `C3` à `C6` (**C106116**) ont été placés pour `U4`. Leur faible stock observé impose un remplacement qualifié avant gel de la BOM si la quantité disponible devient insuffisante.
- Le bloc 8 est désormais saisi avec `C7 = 2,2 nF`, `C9 = 3,3 nF`, `C10 = 270 pF`, `C11`/`C12 = 10 µF`, `R4 = 220 kΩ`, `R5 = 41,2 kΩ`, `R6 = 100 kΩ`, `R7 = 31,6 kΩ`, `R8 = 1 MΩ`, `R9 = 68,1 kΩ` et `L1 = 33 µH`. `C7` est le condensateur bootstrap. Le doublon `C8 = 2,2 nF`, créé accidentellement par le mode de placement répétitif, a été supprimé ; la BOM Power Interface est revenue à 149 composants.
- **Nettoyage et remise en page P2 :** un point de restauration nommé `Codex_sheet2_before_wiring_2026-09-14` a été créé avant intervention. Les segments parasites identifiés autour des passifs superposés ont été retirés progressivement. `C5`, `R5` et `C12`, initialement projetés hors de la page, ont été ramenés dans la zone du convertisseur ; `R5` est placé près de `RON` et `C12` côté sortie. Le nettoyage doit encore être suivi d'une inspection des connexions recréées automatiquement lors des déplacements, puis du câblage fonctionnel complet et du DRC de P2.

---

## 14. CE QUI N'EST PAS DANS LE PÉRIMÈTRE DE CETTE CARTE

- Le coupe-batterie (externe, entre le pack et `CN1`)
- Les signaux moteur : Hall, `VR`, `DIR`, `EL`, `STOP` → carte `motor_interface`
- Le bloc BMS de la feuille 1 : **validé au banc sur le matériel réel, gelé.** `GPIO16`/`GPIO17`, `R1`/`R2` à 1 kΩ, `R3` à 10 kΩ. Ne rien y changer.
