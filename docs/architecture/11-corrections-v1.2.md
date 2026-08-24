# Dossier d'architecture — corrections v1.1 → v1.2

**Version** : 1.2 — 24 août 2026
**Objet** : corrections à apporter au dossier d'architecture v1.1 (8 août / 10 août 2026) après la revue de conception PCB et la validation des composants par datasheet.

> **Ce document fait autorité.** Partout où il contredit `00-index-A-B.md` à `07-interface-operateur.md`, **c'est cette version qui s'applique.**

**Pourquoi un errata plutôt qu'une réécriture** : le dossier fait 140 KB répartis sur huit fichiers, avec des renvois croisés entre sections. Une réécriture intégrale introduirait plus d'erreurs qu'elle n'en corrigerait, et rendrait impossible de voir *ce qui a changé*. Un errata à texte exact se relit, se vérifie et se discute. C'est la pratique normale sur un dossier technique vivant.

**Origine des corrections** :

| Source | Documents |
|---|---|
| Revue de conception des trois PCB | `08-revue-conception-pcb.md` (section X) |
| Validation des composants par datasheet | `09-validation-composants.md` (section Y) |
| Spécification `motor_interface` révisée | `10-spec-motor-interface.md` (section Z) |
| Étude du contacteur statique | `12-contacteur-statique.md` (section AA) |
| Décisions de projet du 24 août 2026 | ci-dessous, **C1 à C17** |

---

# Partie 1 — ⚠️ Corrections critiques de sécurité

Ces trois corrections portent sur la chaîne d'arrêt d'urgence. **À traiter avant toute autre.**

## C-S1 — §H.7 et §A.1-2 : `/SAFE` ne freine pas

**Sections concernées** : `00-index-A-B.md` §A.1-2 · `02-comms-esp32-moteurs.md` §H.7 · `04-securite-watchdogs-capteurs.md` §L.6

**Texte actuel (§A.1-2)** :

> ⚠️ **Point de conception critique** : couper la puissance à un ZS-X11H met le moteur en roue libre — **pas de freinage**. Sur une pente, cela aggrave la situation. C'est pourquoi la boucle d'arrêt d'urgence agit en deux temps : `/SAFE` déclenche d'abord le **freinage électrique** des variateurs, et le contacteur ne s'ouvre qu'après une temporisation matérielle (~1 s).

**Texte actuel (§H.7)** :

> ```
> t = 0 ms     champignon enfoncé
>              ├──► ligne /SAFE tirée à la masse DIRECTEMENT (par le contact)
>              │      └──► entrées EL/STOP des 4 ZS-X11H → freinage électrique
> ```

**Ce qui est faux** : les polarités réelles des entrées du ZS-X11H 🟡 sont

| Entrée | Polarité | Effet réel |
|---|---|---|
| `STOP` | **actif BAS** | *« disable the drive signals. This could be considered a **coast or free spin** mode »* |
| `EL` / `BRAKE` | **actif HAUT** | *« shorting it to 5V will apply the motor's brake. Leaving the pin floating or applying a logic 0 will **disconnect** the brake »* |

Source : banc de test publié à l'analyseur logique ([MAD-EE](https://mad-ee.com/easy-inexpensive-hoverboard-motor-controller/)) + deux listings vendeurs concordants. Voir `09-validation-composants.md` §Y.4.3.

📐 **Conséquence** : tirer `/SAFE` à la masse met `STOP` à 0 → **roue libre**. Et `BRAKE` étant actif-haut, une ligne relâchée, une alimentation perdue ou un fil coupé **relâchent le frein**.

⚠️ **La chaîne de sécurité décrite en v1.1 ne freine jamais.** Elle produit une roue libre — exactement le comportement que le §A.1-2 identifie lui-même comme dangereux en pente. Et le frein n'est pas fail-safe : relâché au repos et à la mise sous tension.

**Texte de remplacement (§H.7)** :

> ```
> t = 0 ms     champignon enfoncé
>              ├──► ligne /SAFE tirée à la masse DIRECTEMENT (par le contact)
>              │      └──► la porte ET du frein retombe
>              │            └──► EL/BRAKE tiré à 5 V par le faisceau → FREINAGE
>              │      (STOP n'intervient PAS : il dépend du firmware, donc
>              │       il n'a aucun rôle de sécurité — voir ci-dessous)
>              │
>              └──► la chaîne d'alimentation de la bobine du contacteur est coupée
>                     └──► réseau RC : le contacteur tient ~1 s
>
> t ≈ 1000 ms  le contacteur s'ouvre — le robot est déjà arrêté ou très ralenti
> ```
>
> ⚠️ **Le freinage ne vient pas de `/SAFE` directement.** Les entrées du ZS-X11H
> ne sont pas fail-safe : `STOP` actif-bas donne une roue libre, `EL`/`BRAKE`
> actif-haut se relâche à la moindre perte de signal. Le freinage est obtenu par
> une **résistance de tirage vers 5 V placée sur le faisceau, côté variateur**,
> qu'un MOSFET commandé par une porte ET matérielle contredit tant que tout va
> bien. Voir `10-spec-motor-interface.md` §Z.1.
>
> ⚠️ **`STOP` n'a aucun rôle de sécurité.** Il est piloté par un GPIO : un ESP32
> mort ne l'assertera jamais. Ne jamais le faire figurer dans une analyse de
> sécurité.
>
> ⚠️ **Le 5 V du frein doit être pris en amont du contacteur.** S'il venait du
> 78L05 du variateur, l'ouverture du contacteur à t ≈ 1 s couperait le rail et
> **relâcherait le frein au moment précis où il faut freiner**.

**Ajout au §L.6** (« Ce qui doit fonctionner même si le PC principal plante ») — la ligne « Freinage électrique des moteurs / ✅ Oui / Ligne `/SAFE` → entrées `EL`/`STOP` des variateurs » devient :

> | **Freinage électrique des moteurs** | 🟡 Oui, sous réserve | Résistance de tirage vers `5V_BRAKE` **sur le faisceau côté variateur** : perte de `/SAFE`, du 3,3 V, de l'ESP32, du câble, ou carte débranchée → le MOSFET se bloque → `EL` remonte à 5 V → frein serré. ⚠️ **Réserve** : après ouverture du contacteur, la *commande* est maintenue mais la *capacité physique* à freiner dépend de l'énergie restante dans le variateur — à valider par essai |

## C-S2 — §H.6 et §D.8 : le hacheur de freinage n'est pas une évolution de phase 4

**Sections** : `02-comms-esp32-moteurs.md` §H.6 · `01-inventaire-electrique.md` §D.8

**Texte actuel (§H.6)** :

> | **Évolution** | Un **hacheur de freinage** (MOSFET + résistance de puissance sur le bus, déclenché au-dessus de 41,5 V) est la solution propre. À prévoir en phase 4 si les freinages fréquents deviennent un problème. |

**Ce qui manque** : ⚠️ **ouvrir le contacteur pendant que les moteurs tournent crée une surtension garantie.** La batterie disparaît du bus, les moteurs tournent encore, les diodes de corps des ponts redressent la force contre-électromotrice dans les seuls condensateurs de bus (~10 mF 📐). Plus rien n'absorbe l'énergie.

C'est **exactement la séquence d'arrêt d'urgence** du §H.7. Si le robot est encore en mouvement à t = 1 s, on ouvre sous force contre-électromotrice.

Et le §D.8 démontre lui-même qu'aucune TVS ne peut couvrir ce cas. Les chiffres constructeur le confirment ✅ :

| TVS | Standoff | **Écrêtage** |
|---|---|---|
| `SMCJ43A` | 43 V | **69,4 V** |
| `SMCJ45A` | 45 V | **72,7 V** |
| `SMCJ48A` | 48 V | **77,4 V** |

📐 Une TVS dont le standoff dépasse 42 V (nécessaire pour ne pas conduire à pleine charge) écrête à 69–77 V, **au-dessus de la tenue de condensateurs 63 V**. Ce n'est pas un mauvais choix de référence : **c'est une impossibilité physique.**

**Texte de remplacement** :

> ⚠️ **Le hacheur de freinage est un organe de sécurité de phase 1, pas une évolution.**
> Il est le seul dispositif capable d'écrêter à 42,5 V, là où aucune TVS ne le peut.
> ⚠️ **Et depuis la décision C17, il est carrément indispensable** : le contacteur
> statique tête-bêche bloque la régénération dans les deux sens, donc à l'ouverture
> l'énergie des moteurs n'a plus AUCUNE autre issue que le hacheur.
>
> **Trois exigences non négociables** :
>
> | Exigence | Raison |
> |---|---|
> | **En aval du contacteur**, côté bus | Sinon il est déconnecté au moment précis où on en a besoin |
> | **Entièrement analogique** — comparateur à hystérésis | Doit fonctionner ESP32 planté, en reset, ou absent |
> | **Alimenté par le bus lui-même** | Aucune dépendance à un rail logique |
>
> **Dimensionnement** 📐 (35 kg, 1,5 m/s) :
>
> | Cas | Calcul | Puissance |
> |---|---|---|
> | Arrêt d'urgence depuis 1,5 m/s | ½ × 35 × 1,5² | **39 J** — négligeable |
> | Descente continue 10 % à 1,5 m/s | 35 × 9,81 × 0,0995 × 1,5 | **51 W** |
> | Descente continue 15 % à 1,5 m/s | 35 × 9,81 × 0,148 × 1,5 | **76 W** |
>
> → Résistance **22 Ω / 100 W** (`TT WH100-22RJI`, impédance thermique 1 °C/W ✅),
> **sur châssis ou dissipateur, jamais sur le PCB**.
>
> ⚠️ **Pourquoi 22 Ω et pas 15 Ω** : la puissance *instantanée* pendant la conduction
> vaut V²/R, pas la moyenne de descente. À 15 Ω : 42,5²/15 = **120 W**, soit +120 °C
> sur l'élément à 1 °C/W — au-dessus du calibre. À 22 Ω : **82 W**, dans le calibre,
> et toujours au-dessus des 76 W d'une descente à 15 %.
> Les 51 W et 76 W ci-dessus sont des **moyennes de descente** ; le cas dimensionnant
> thermique est le bus maintenu au-dessus du seuil (fin de charge, régénération
> prolongée, BMS en coupure de charge). Si ce régime doit être tenu en continu,
> borner le rapport cyclique et l'écrire.
> → MOSFET N 100 V / ≥ 30 A.
> → Seuils **ON 42,5 V / OFF 41,8 V** 📐 — à recaler après M1 et M12.
> → LED dédiée : si elle clignote souvent, c'est un diagnostic gratuit.
>
> **La TVS et le hacheur sont complémentaires, pas redondants** : la TVS pour les
> transitoires sub-µs, le hacheur pour tout le reste.

## C-S3 — §D.3 et §E.7 : les fusibles MIDI standard sont limités à 32 V DC

**Sections** : `01-inventaire-electrique.md` §C.3, §D.3, §E.7, §H.2 · `04-securite-watchdogs-capteurs.md` §L.3

**Texte actuel** : le dossier prescrit « MIDI 58 V — 25 A », « MIDI 58 V — 10 A », « MIDI 58 V — 5 A », mais aussi, à plusieurs endroits, simplement « MIDI 25 A » ou « fusible MIDI ».

**Précision constructeur** ✅ :

| Produit | Tenue DC | Verdict |
|---|---|---|
| Fusible **MIDI/AMI standard** Littelfuse `0498xxx` | **32 V DC** | ❌ **INADAPTÉ** |
| Fusible **ATO/ATC/Mini** lame | **32 V DC** | ❌ **INADAPTÉ** |
| Fusible **`BF1 58V`** (`142.5631.58xx`) | **58 V DC** | ✅ **Retenu** |
| Porte-fusible **MIDI 498 Series** (`04980900S`) | **58 V DC**, montage **in-board M3** | ✅ **Retenu** |
| Blue Sea **MRBF** 5191 | **58 V DC**, goujon M8 | ✅ Point batterie uniquement |

⚠️ **Le piège** : le support et le fusible ne sont pas cotés au même endroit. Un porte-fusible « 500 V » qui reçoit une lame ATO 32 V donne un ensemble à **32 V**.

**Ajout au §E.7 (liste rouge)** :

> | **Fusible MIDI/MEGA « standard » (32 V)** | ⚠️ Il existe des MIDI en 32 V **et** en 58 V, sous des références voisines. Le 32 V est le plus courant en distribution automobile. À 42 V, l'arc ne s'éteint pas franchement | Fusible `BF1 58V` **explicitement**, porte-fusible MIDI 498 Series |

**Remplacer partout** « MIDI 58 V — *n* A » par la référence exacte : **`BF1 58V`, *n* A, porte-fusible MIDI 498 Series in-board M3**.

---

# Partie 2 — Décisions de projet du 24 août 2026

## C1 — Le pack est un **Pro**, 474 Wh, 10S4P

**Section** : `01-inventaire-electrique.md` §C.2.1 · `00-index-A-B.md` §B.1 hypothèse H4, §B.3 question Q3

Le marquage `10INR19/66-4` décodé selon IEC 61960 donne 10S**4**P. Contrôle : 37 × 12,8 = 473,6 Wh contre 474 Wh sur l'étiquette. C'est le pack **M365 Pro / Pro 2** ✅.

| Paramètre | v1.1 (Classic) | **v1.2 (Pro)** |
|---|---|---|
| Marquage | `10INR19/66-3` | **`10INR19/66-4`** |
| Configuration | 10S3P, 30 cellules | **10S4P, 40 cellules** |
| Capacité | 7,8 Ah | **12,8 Ah** |
| Énergie | 280 Wh | **474 Wh** |
| Tension nominale | 36 V | **37 V** |
| Autonomie à 200 W moyens | ≈ 1 h 10 | 📐 **≈ 2 h** (403 Wh utiles à 85 % DoD) |
| Courant admissible cellule | 3P → 20–30 A | **4P → 26–40 A** |

⚠️ **Ne pas déduire le seuil BMS du nombre de cellules.** Le BMS peut limiter au même endroit sur les deux packs. **M1 reste bloquante.**

🔴 **Nouveau point à vérifier** : le BMS du Pro n'est pas forcément identique à celui du Classic (STM8L151K6 + BQ76930 🟡). Le protocole UART du §D.9 est à revalider. → **Photographier l'étiquette du BMS**, pas seulement celle du pack.

**Hypothèse H4 : levée.** **Question Q3 : partiellement répondue** — reste l'origine du BMS (d'origine ou tiers).

## C2 — `motor_interface` passe à 2 moteurs, identique AV et AR

**Section** : `02-comms-esp32-moteurs.md` §G.2

Décision confirmée le 24 août : **une seule référence PCB, 2 exemplaires en service sur un lot de 5, 3 rechanges universelles**. Le rôle AVANT/ARRIÈRE est déterminé par deux straps GPIO lus au boot.

Cela **valide** le raisonnement du §G.2 (« Pourquoi deux nœuds MOTION plutôt qu'un seul pour quatre roues ») et **annule** la carte 4 moteurs du README existant.

⚠️ **Ajout au périmètre** : `motor_interface` porte désormais aussi le **chemin de commande** `VR` / `DIR` / `EL` / `STOP` / NTC, absent des deux README. Sans lui, ces signaux restent en faisceau volant entre l'ESP32 et chaque variateur — précisément le câblage que la carte devait supprimer.

## C3 — Trois cartes, et les périphériques orphelins

**Section** : `05-reseau-code-tests-physique.md` §R

Décision : **trois cartes séparées**, pas de porteur MCU générique. Conséquence : les périphériques absents du dossier v1.1 doivent être attribués.

| Périphérique | Carte d'accueil | Note |
|---|---|---|
| **WS2812B avant + arrière** | `safety_power` | ⚠️ **Totalement absents du dossier v1.1**. `SN74AHCT125` obligatoire (`VIH` = 3,5 V pour un WS2812B en 5 V, un GPIO à 3,3 V est sous le seuil ✅) |
| **Capteur de température 3 broches** | `safety_power` | 🔴 Type non identifié — entrée à double usage, 3 empreintes de 0 Ω |
| 2 ventilateurs PWM | `safety_power` | ⚠️ Tachy à tirer vers **3,3 V, jamais 12 V**, + 4,7 k série et clamp `BAT54S` |
| BNO085 | `safety_power`, **capteur déporté sur mât** | ⚠️ ≥ 25 cm au-dessus des busbars, ≥ 30 cm des moteurs (§N.3) |
| Alimentation X1, YDLIDAR, Kinect | `safety_power` | Branches dédiées et fusées |

⚠️ **Contrainte de conception qui en découle** : `safety_power` mélange 42 V de puissance et des signaux de capteurs. **Séparation physique stricte sur le PCB** — zone puissance et zone signal, plan de masse de séparation, point d'étoile de masse unique et sérigraphié `★ GND STAR`.

## C4 — `can_distribution` devient `safety_bus_distribution`

**Section** : `02-comms-esp32-moteurs.md` §F.2

La ligne `/SAFE` (§H.7) va aux **mêmes quatre nœuds** que le CAN, par le **même chemin physique**. Tirer deux faisceaux parallèles serait absurde.

→ **Le connecteur unique du robot devient Micro-Fit 3.0 6 positions** : `5V_LOGIC`, `CANH`, `CANL`, `/SAFE`, `GND_LOGIC`, `SHIELD`. Deux en parallèle par nœud, le bus traverse sans stub.

⚠️ **La carte n'a aucun microcontrôleur.** Une carte de distribution qui embarque un MCU devient un point de défaillance sur le chemin de la sécurité.

---

# Partie 3 — Corrections de composants

## C5 — §F.2 : transceiver CAN

**Texte actuel** : « Chaque ESP32 nécessite un transceiver CAN 3,3 V : `SN65HVD230` ou `TCAN332`. »

| Paramètre | `SN65HVD230` ✅ | **`TCAN1042HVDR`** ✅ |
|---|---|---|
| **Tenue de défaut bus (abs max)** | **−4 V à +16 V** | **±70 V** |
| Mode commun | −2 à +7 V | ±30 V |
| `VCC` | 3,0–3,6 V | 4,5–5,5 V + `VIO` 3,0–5,5 V |

⚠️ Sur un robot où le CAN, le 12 V et le 42 V cheminent ensemble, un contact entre une ligne de puissance et `CANH` détruit **tous les transceivers du bus simultanément** avec un HVD230. Avec ±70 V, le défaut devient survivable.

**Texte de remplacement** :

> Chaque nœud utilise un **`TCAN1042HVDR`** : bus 5 V, logique 3,3 V par la broche `VIO`, **tenue de défaut ±70 V**.
>
> ⚠️ **Deux pièges de mise en œuvre** :
> - C'est le suffixe **`V`**, pas le `H`, qui apporte la broche `VIO`.
> - **`STB` a un pull-up interne : une broche flottante met le transceiver en STANDBY**, silencieusement. La tirer explicitement à `GND`.
>
> Découplage : `VCC` 4,7 µF + 0,1 µF ; `VIO` 0,1 µF, au plus près ✅.
> Protection : `NUP2105L` (écrête à 40 V @ 5 A) + self de mode commun `ACT45B-510-2P-TL003`.

## C6 — §F.5-L3, §G.3, §H.3 : abandonner le `2N7002`

**Texte actuel** : « DIR : logique, collecteur ouvert (MOSFET 2N7002) »

| Référence | `VGS(th)` ✅ | `RDS(on)` garanti à faible `VGS` ✅ |
|---|---|---|
| `2N7002` | 1,0 min / **2,5 V max** (2,75 V à −55 °C) | 🔴 **rien en dessous de VGS = 4,5 V** |
| **`DMG1012T`** | 0,5 / **1,0 V max** | ✅ **0,5 Ω max à VGS = 2,5 V** |
| `BSS138` onsemi | 0,8 / 1,3 / 1,5 V max | 6,0 Ω max à 4,5 V |
| `DMN601K` | 1,0 / 1,6 / 2,5 V max | ⚠️ **N'est PAS logic-level** malgré son nom |

⚠️ Avec 3,3 V de grille, l'overdrive garanti du `2N7002` dans le pire cas est de **0,8 V**, et rien n'est garanti à froid. Sur un robot extérieur qui démarre à 3 °C, ce n'est pas acceptable sur `DIR`, `EL` et `STOP`.

→ **Remplacer partout par `DMG1012T`** (ou `BSS138` onsemi si SOT-23 et 50 V sont requis).

## C7 — §H.4 : dériver trois lignes Hall, pas deux, et changer de tampon

**Texte actuel** : « dériver **deux des trois lignes Hall** de chaque moteur vers les compteurs matériels (PCNT) de l'ESP32, à travers un tampon 74LVC245 »

**Deux corrections** :

**(a) Trois lignes, pas deux.** Le §M.2 panne 15 **exige** de détecter « une séquence d'états Hall invalide ». ⚠️ **C'est impossible avec deux lignes** : toute combinaison de 2 bits est valide. Avec trois, `000` et `111` sont interdits.

| | 2 lignes | **3 lignes** |
|---|---|---|
| Comptes par tour | 60 | **90** |
| Résolution linéaire | 8,6 mm | **5,8 mm** |
| Détection d'état invalide | ❌ impossible | ✅ |

📐 Coût CPU du décodage 6 états par interruption : 3 lignes × 2 moteurs × 145 Hz = **870 interruptions/s**, soit **0,07 %** d'un ESP32 à 240 MHz. Non contraignant.

**(b) `SN74LVC3G17`, pas `74LVC245` ni `SN74LVC14A`.**

⚠️ **TI a retiré du `SN74LVC14A` la mention `Ioff`** (*« Supports Live Insertion, Partial-Power-Down Mode and Back Drive Protection »*), et écrit désormais : *« The inputs to this device have negative clamping diodes »* ✅. L'exigence dure du README `motor_interface` — le passthrough survit à la perte du 3,3 V — n'est plus garantie.

Le **`SN74LVC3G17`** la garantit explicitement ✅, est **non inverseur**, et accepte `VCC` jusqu'à 5,5 V.

⚠️ **À écrire dans la BOM** : ❌ jamais de `74HC14`/`74HCT14` en substitution — clamp vers `VCC`.

**(c) Le pull-up doit être sur la carte.**

**Texte actuel** : « Ne pas alimenter les capteurs Hall depuis l'ESP32. Ils sont déjà alimentés par le variateur »

Correct pour l'alimentation, mais le **pull-up** l'est aussi — et c'est un problème. ⚠️ Variateur débranché ou mort → le pull-up part avec lui → l'entrée du tampon flotte → l'ESP32 compte du bruit. Le §M.2 panne 15 le dit : une odométrie fausse est **pire** qu'une odométrie absente.

📐 **Ajouter 2,2 kΩ vers `+5V_MOT` sur la carte.** Charge vue par le capteur : 2,3 mA contre **20 mA** que sait absorber un `SS41F` ✅ — marge ×8,7. Et τ devient déterministe : **7 µs**, coupure **22 kHz**.

**(d) Le RC est à revoir.** (Le §H.4 du dossier dit 1 kΩ / 1 nF, le README `motor_interface` dit 1 kΩ / 10 nF — les deux valeurs circulent, il faut trancher.) ⚠️ Avec un pull-up distant inconnu, la constante de montée est (R_pullup + R_série) × C — jusqu'à **110 µs** avec un pull-up de 10 kΩ, soit une coupure à **1,4 kHz**, en plein dans la bande du hachage 16–20 kHz qu'il devait rejeter.

→ **2,2 kΩ série + 10 nF / 100 V + clamp `BAT54S`.** Le 2,2 kΩ (au lieu de 1 kΩ) limite à **17 mA** le courant injecté si un fil de phase touche une ligne Hall, contre 37 mA — sous les −50 mA de `IIK` du LVC ✅.

## C8 — §D.3 et §N.6 : ajouter l'`INA228`

**Texte actuel (§N.6)** : « Si tu veux vraiment une mesure de courant précise […] un **shunt de 500 µΩ + INA226/INA228** en I²C donnerait 0,1 % »

⚠️ **L'`INA226` a un mode commun de 36 V maximum** (abs max 40 V) ✅ — **il est physiquement inutilisable sur un bus 30–42 V.** C'est un piège classique : c'est le composant le plus connu de la famille.

| | `INA226` | **`INA228`** |
|---|---|---|
| Mode commun | **0–36 V** ❌ | **−0,3 à +85 V** ✅ |
| Résolution | 16 bits | **20 bits** |
| Adresses I²C | 16 | **16** |
| **Accumulateurs** | ❌ aucun | ✅ **Energy et Charge 40 bits matériels** |

📐 **Deux conséquences qui changent une conclusion du §N.6** :

1. **La conclusion « le comptage coulométrique est inexploitable » vaut pour l'ACS758, pas pour l'INA228.** Avec un offset de ±1 µV max et un accumulateur de charge 40 bits **matériel**, une estimation de SOC indépendante du BMS redevient possible. Le §N.6 doit être nuancé.
2. **16 adresses I²C** permettent de mettre les **4 branches moteur** *plus* la mesure de bus sur le même bus — ce que le §C.2.2 rendait souhaitable, puisque *« le ZS-X11H n'a ni limitation de courant réglable, ni mode couple »*. Entre « tout va bien » et « le fusible de 25 A a fondu », il n'y avait aucune information.

**Ajout au §D.3** :

> **③ bis — Mesure par branche.** Un shunt 2 mΩ + `INA228` sur chacune des 4 branches
> moteur donne le courant réel de chaque roue à 1 % près. Cela permet la **détection
> précoce** d'un moteur qui force ou d'un roulement qui grippe, bien avant le seuil
> de calage à 300 ms du §H.5, et une **limitation de courant par roue** que le
> ZS-X11H ne sait pas faire. 📐 ~2,50 € par branche.
>
> L'ACS758 et l'INA228 mesurent la même grandeur par deux principes indépendants :
> **contrôle de plausibilité gratuit**, qui détecte une sortie figée (§C.1).

## C9 — §D.8 : anti-inversion de polarité

**Texte actuel** : « Si besoin d'une protection active : LTC4359 + 4 MOSFET 100 V / 1,5 mΩ en parallèle »

✅ **Confirmé** : `LTC4359`, 4–80 V, protection en inversion jusqu'à **−40 V**, régulation *ideal diode* à **~30 mV** aux bornes du FET, `IQ` 150 µA.

⚠️ **À ajouter** : le `LTC4359` **n'a aucune protection en surtension**. Sa datasheet renvoie explicitement à un `LT4363` externe. Dans cette architecture, c'est le **hacheur de freinage** et les **eFuses à cutoff ajustable** qui assurent l'OVP — pas ce composant. À écrire, sinon quelqu'un croira la fonction couverte.

## C10 — eFuses : plages de tension

⚠️ Le `TPS2595` proposé en revue plafonne à **2,7–18 V** (abs max 20 V) ✅ — **inutilisable sur le bus 42 V**.

| Rail | Composant ✅ | Plage |
|---|---|---|
| Branches 42 V forte puissance | **`TPS26630`** | 4,5–60 V, 6 A, cutoff **ajustable** |
| Auxiliaires 42 V < 880 mA | **`TPS26620`** | 4,5–60 V, protection inverse **intégrée** |
| Branches 12 V et 5 V | **`TPS259573`** | 2,7–18 V, broche `EN/OVLO` |

⚠️ **Piège** : les variantes à **clamp fixe** (35 / 38 / 39 V) écrêteraient en permanence sur un bus à 42 V. **Prendre impérativement les variantes à cutoff ajustable.**

## C11 — PPTC : les tensions sont un piège

| Référence | `Ihold` | **`Vmax`** ✅ | Verdict |
|---|---|---|---|
| `MF-MSMF010` | 0,10 A | **60 V** | ✅ |
| `MF-MSMF050` | 0,50 A | ⚠️ **15 V** | ❌ sous le rail 12 V |
| **`1812L050/30`** | 0,50 A | **30 V** | ✅ **retenue sur 12 V** |
| `1812L110/16` | 1,10 A | 16 V | ❌ |

⚠️ **Aucune PPTC 1812 de ces séries ne convient au bus 42 V.** Sur le bus : fusibles 58 V ou eFuses, point.

## C12 — §D.5 : XT90-S contre Anderson

**Texte actuel** : « Anderson SB50 ou XT90-S »

🔴 **Amass ne publie aucune spécification de courant** — ni fiche technique publique, ni valeur sur son site. Les chiffres (90 A, 500 V) circulent via les distributeurs. **Ce sont des revendications vendeur.**

| Connecteur | Statut |
|---|---|
| `XT90` / `XT90-S` | 🔴 Aucune donnée constructeur |
| **Anderson `PP45`** | ✅ **45 A, 600 V** (UL 1977), contacts CI disponibles |
| **Anderson `SB50`** | ✅ **50 A wire-to-PCB**, 600 V, **hot-plug qualifié UL**, 10 000 cycles |

📐 Le XT90-S peut rester en usage pratique — il est répandu et fonctionne. Mais il ne peut pas figurer comme choix **justifié** dans une spécification. **Pour tout ce qui doit être défendable : Anderson.** Le `SB50` est en outre le seul des deux dont le hot-plug est qualifié, ce qui est pertinent vu l'interdiction de débranchement sous charge du §D.5.

## C13 — §D.5 : borniers et connecteurs de signal

⚠️ **Deux corrections chiffrées** :

**(a) Un bornier 5,08 mm ne passe pas 25 A.** `MKDS 3/2-5,08` = **24 A** nominal ✅, et Phoenix applique un facteur de déclassement **0,8** documenté → **≈ 19 A**. → **`PC 5/2-STF1-7,62`** (32 A IEC / 41 A cULus, 0,5–0,8 N·m) ou WAGO 2624 au pas 7,5 mm.

**(b) ⚠️ Le JST PH n'est pas un connecteur verrouillable.** ✅ PH et XH ont un maintien **par friction**. Seuls GH et VH ont un verrouillage positif, comme Micro-Fit et Mini-Fit Jr.

Sur un robot qui vibre, un connecteur Hall à friction qui se retire partiellement donne une **odométrie fausse intermittente** — le pire mode de panne (§M.2 panne 15).

→ **Micro-Fit 3.0 6 positions** pour les Hall, au lieu de JST PH 5 positions. Bénéfice secondaire : 6 positions rendent le croisement avec un connecteur 5 broches **physiquement impossible**.

## C14 — §G.3 : affectation des GPIO

**Trois corrections** :

**(a) ⚠️ `GPIO12` doit être abandonné.** Le §G.3 y assigne le PWM ventilateur 1 sur SAFETY. `GPIO12` = `MTDI` = sélection de `VDD_SDIO`. Verbatim esptool ✅ : *« If driven High, flash voltage (VDD_SDIO) is 1.8V not default 3.3V […] may prevent flashing and/or booting if 3.3V flash is used and this pin is pulled high, causing the flash to brownout. »*

Or tu demandes justement des LEDs de debug et des résistances de tirage pour la visualisation d'état. Une LED vers 3,3 V sur cette ligne rend le module **définitivement non démarrable**, avec un symptôme muet.

→ **Ne pas utiliser `GPIO12` du tout.**

⚠️ **Mais attention au décompte** : 32 broches sur les rangées, moins 6 de flash, moins `GPIO12`, moins `GPIO0`/`1`/`2`/`3` réservés = **21 utilisables**. L'affectation initiale en consommait 21 — **zéro réserve, et aucune broche pour le watchdog `TPL5010`** dont dépend pourtant toute la chaîne de freinage. Il a fallu fusionner `EL_1`/`EL_2` en une commande de frein commune et déplacer les straps de rôle sur l'ADS1115 pour libérer les deux broches manquantes. Voir `10-spec-motor-interface.md` §Z.5.4.

**(b) ⚠️ `GPIO16`/`GPIO17` sont inutilisables sur les variantes PSRAM.** Le §G.3 y met l'UART BMS (SAFETY) et les straps de rôle (MOTION). Sur `ESP32-D0WDR2-V3`, **`IO16` est reliée à la PSRAM** ✅. → **Vérifier la variante exacte des modules** (mesure P3) avant de figer.

**(c) `GPIO6-11` sont amenées sur les connecteurs de la DevKitC.** Bien qu'elles soient `NC` sur le module WROOM, la DevKitC les route vers ses headers sous les noms `D0`–`D3`, `CMD`, `CLK` ✅. → **Aucune connexion**, sérigraphie `NC — FLASH`.

## C15 — §L6 : le BNO085 en SPI demande cinq choses de plus

**Texte actuel** : « SPI mode 3 (CPOL=1, CPHA=1), 3 MHz max ✅ + broche H_INTN (actif bas) obligatoire »

Exact, mais **incomplet**. Trois exigences absentes du dossier ✅ :

1. ⚠️ **`PS1` et `PS0` doivent être HAUTS avant le reset** pour sélectionner le SPI : *« Both pins must be high from before reset until after the first assertion of H_INTN. »* Sans cela, le composant démarre **en I²C** — précisément le bus que le §L6 interdit.
2. ⚠️ **`PS0`/`WAKE` doit aller à un GPIO**, pas être câblé en dur : *« Pin 6 must be connected to a GPIO so that the WAKE functionality can be performed. »*
3. ⚠️ **`BOOTN` à tirer haut par 10 kΩ.** Bas au reset = mode bootloader.
4. Il faut attendre **l'assertion de `H_INTN`**, pas un délai fixe de 90 ms.
5. ⚠️ Si le host ne répond pas à `H_INTN` **dans ~10 ms**, le composant timeout et réessaie.

⚠️ **Réserve pratique** : les cartes Adafruit et SparkFun **strappent généralement `PS0`/`PS1` pour l'I²C**. Passer en SPI demande souvent de modifier un pontet sur la carte. À vérifier avant de router le connecteur.

## C16 — §M.1 : le `TPL5010`

**Deux précisions** ✅ :

1. ⚠️ L'intervalle se règle par une **résistance sur `DELAY`** (500 Ω à 170 kΩ, 1 %) via une **table de correspondance, pas une formule** : 10,18 kΩ → 8 s, 22,02 kΩ → 1 min, 170 kΩ → 2 h.
2. ⚠️ **`RSTn` est à drain ouvert** → **pull-up externe obligatoire**. L'oublier donne un watchdog silencieusement inopérant.
3. `DONE` est reconnu sur une **transition bas→haut**, à fournir **au moins 20 ms avant** le prochain front montant de `WAKE`.


## C17 — §D.2, §D.3-④ et §D.3-⑤ : le contacteur devient un bloc statique sur la carte

**Décision du 24 août 2026** : le contacteur est **conçu sur `safety_power`**, pas acheté.

**Texte actuel (§D.3-④)** :

> Un contacteur **normalement ouvert** : au repos, sans alimentation, il est ouvert. […] Choix : contacteur DC dédié (type Gigavac, TE EV200) ou, plus économique, un **relais de puissance automobile DC 48 V / 150 A**. 📐 Prévoir un **économiseur de bobine**.

**Remplacé par** : `TPS4810-Q1` + 2 × `IPB017N10N5` tête-bêche source commune. Étude complète dans `12-contacteur-statique.md`.

| | Contacteur mécanique | **Statique retenu** |
|---|---|---|
| Coût | 80–200 € | ≈ **20 €** |
| Consommation permanente | 2–8 W de bobine | **≈ 4 mW** |
| Coupure sur défaut | ~10 ms | **4,3 µs** ⭐ |
| Auto-test de la protection | ❌ | ✅ broche `SCP_TEST` ⭐ |
| Diagnostic de collage | Mesure de `V_bus` | ✅ par transistor, nœud `DIAG` ⭐ |
| Isolation galvanique | ✅ | ❌ — compensée par le coupe-batterie |

⚠️ **Le choix du contrôleur n'est pas libre.** Le diagnostic de transistor collé exige de commander `Q1` et `Q2` **indépendamment**. Or `LTC7000`, `LTC4368`, `LM5069`, `TPS2492`, `LTC4260`, `LTC4364` et `TPS4811-Q1` n'ont **tous qu'une seule sortie de grille** ✅. Le `TPS4810-Q1` est **le seul composant au-dessus de 40 V** à en offrir deux. Et 🔴 **aucun eFuse à FET intégré n'existe au-dessus de 6 A dans la classe 40–80 V**.

### ⚠️ La précharge n'est PAS supprimée

J'avais annoncé que le statique supprimait la précharge, par rampe de grille. **C'est faux**, et le calcul est sans appel :

| | Valeur | Source |
|---|---|---|
| SOA d'un trench moderne | **0,5 A à 54 V / 10 ms** | ✅ Infineon, comparaison publiée à `RDS(on)` identique |
| Besoin pour une rampe de 0,1 s | **4,2 A à 42 V** | 📐 `I = C·V/T` |
| **Écart** | **facteur 16 à 40** selon la durée | |

Et le parallélisme n'aide pas : ⚠️ **en mode linéaire le die le plus chaud attire plus de courant** — c'est l'[effet Spirito](https://www.nexperia.com/applications/interactive-app-notes/IAN50006_Power_MOSFETs_in_linear_mode), et aucun fabricant ne publie de facteur de dérating pour ce cas. S'y ajoute que la rampe de grille est en **boucle ouverte** : `V_th` dérive de −5 mV/K, donc ni le courant crête ni la durée ne sont maîtrisés.

📐 **Le §D.5 reste donc valable tel quel** : résistance **10 Ω / 25 W**, τ = 0,1 s, 8,8 J contre 625 J de tenue, timeout 1,5 s. C'est aussi la topologie de référence de TI pour les disjoncteurs de BMS ([SLUAAS8](https://www.ti.com/lit/an/sluaas8/sluaas8.pdf)).

⚠️ **Mais le FET de précharge est piloté par l'ESP32-SAFETY, pas par le `TPS4810`** : la note TI utilise `INP2` pour la précharge, alors que le diagnostic a besoin de `INP1`/`INP2` sur `Q1` et `Q2` séparément. Un seul contrôleur ne peut pas faire les deux ; la sécurité passe avant.

### ⚠️ Trois conséquences ailleurs dans le dossier

1. **Le hacheur de freinage devient indispensable, pas seulement recommandé.** Le montage tête-bêche **bloque la régénération dans les deux sens** — c'est son intérêt, mais cela signifie qu'à l'ouverture l'énergie des moteurs n'a plus **aucune** autre issue. Renforce C-S2.
2. **La décharge active du §D.8 devient une condition du diagnostic**, plus seulement une commodité de maintenance : le test de collage n'est exécutable qu'une fois les 10 mF déchargés.
3. ⚠️ **Le §A.1 doit préciser que le statique remplace le niveau N5 commandé, pas le N6.** Un MOSFET stressé claque **en court-circuit** ; un contacteur mécanique qui a fermé sur charge **se soude**. Aucun des deux ne défaille en sécurité. **Le coupe-batterie manuel et le fusible MRBF restent le vrai dernier recours**, et c'est cette condition qui rend le choix statique acceptable.

### `di/dt` — le piège du statique

Couper 25 A dans 1,5 µH de faisceau en 100 ns donne `L·di/dt` = **375 V** sur un transistor qui tient 100 V.

📐 **Résistance de grille de coupure 100–220 Ω** (→ 56 à 73 V de crête), **TVS `SMCJ48A` drain-à-drain** (`VC` 77,4 V, 22 V de marge), et **diode de roue libre du nœud de charge vers la masse** — TI documente explicitement le transitoire **négatif** dû à l'inductance du faisceau côté charge.

⚠️ Ce `Rg` ralentit aussi la coupure sur défaut d'environ 3 µs. **Le TVS est la vraie protection, pas le `Rg`.**

---

---

# Partie 4 — Ce qui est confirmé sans changement

Utile à savoir, pour ne pas rouvrir des débats tranchés.

| Point du dossier | Statut après vérification |
|---|---|
| §A.1-1 : bus CAN 500 kbit/s plutôt qu'USB | ✅ Confirmé |
| §A.1-3 : le X1 ne fait jamais de temps réel | ✅ Confirmé |
| §C.2.3 : **15 paires de pôles, 90 états Hall/tour** | ✅ **Consensus fort** — doc ODrive officielle + mesure indépendante à l'analyseur logique |
| §H.3 option B : `MCP4728` + ampli plutôt que le cavalier J1 | ✅ Confirmé, et **renforcé** : la sortie EEPROM au démarrage est explicitement garantie (*« provides analog outputs with the saved settings immediately »*) |
| §F.5-L3 : ne pas alimenter l'ESP32 depuis le 5 V du variateur | ✅ Confirmé — 50 mA sur un `78L05`, et des blocages rapportés |
| §F.5-L3 : 10 kΩ vers GND sur `VR` | ✅ Confirmé, non optionnel |
| §D.6 : table cuivre 2 oz | ✅ Confirmé — IPC-2221 donne 6,5 mm à 25 A et 12,5 mm à 40 A ⚠️ (extrapolation hors domaine au-dessus de 35 A) |
| §D.9 : isolation galvanique du lien BMS | ✅ Confirmé, non négociable |
| §E.7 : fusibles ATO interdits sur le bus | ✅ Confirmé — 32 V DC |
| §N.3 : le magnétomètre est le maillon faible | ✅ Confirmé |
| §N.7 : PWM ventilateur à 25 kHz, commande drain ouvert | ✅ Confirmé, + ⚠️ **ne jamais hacher l'alimentation d'un ventilateur 4 fils** |
| §A.5 : l'ACS758 est NRND et inadapté au SOC | ✅ Confirmé — mais voir C8, l'INA228 rouvre la porte |

---

# Partie 5 — Mesures : état consolidé

| # | Mesure | Statut | Bloque |
|---|---|---|---|
| **PACK_ID** | Étiquette du pack **et du BMS** | 🆕 **Nouvelle** | `safety_power` |
| **P1** | Entraxe des rangées du module ESP32, au pied à coulisse | 🆕 **Nouvelle** | Tous les PCB |
| **P2** | Ordre des broches au multimètre sur le module réel | 🆕 | Affectation GPIO |
| **P3** | Variante du module : PSRAM ou non ? | 🆕 | `GPIO16`/`17` |
| **TEMP_ID** | Type du capteur de température 3 broches | 🆕 | ✅ contourné par double empreinte |
| **HALL_OC** | Les Hall sont-ils vraiment à collecteur ouvert ? | 🆕 | Pull-up |
| **HALL_COL** | Code couleur réel, fil par fil | 🆕 | Sérigraphie |
| M1 | Seuil de coupure BMS | inchangée | `safety_power` |
| M2 | Résistance interne du pack | inchangée | `safety_power` |
| M3 | Niveau logique BMS | inchangée | ✅ contournée — isolateur de toute façon |
| M4 | Polarité `EL`/`STOP` | 🟡 **consensus fort acquis**, à confirmer | ✅ contournée — étage agnostique |
| M5 | Cavalier `J1` | 🔴 contradiction | ✅ sans objet — option DAC retenue |
| M6 | Paires de pôles | ✅ **répondue** — 15, consensus fort | — |
| M7 | Niveaux Hall et `SC` | 🔴 **aucune source** | `motor_interface` |
| M9–M11 | X1, GPS, lidar | inchangées | logiciel |
| M12 | Condensateurs d'entrée ZS-X11H | inchangée | `safety_power` |

---

# Partie 6 — Ordre de fabrication

```
   Étape 1   safety_bus_distribution ──► lot de 5
             AUCUNE mesure bloquante. Débloque tout le banc de test.
                 │
   Étape 2   M4, M5, M7 sur un moteur + un ZS-X11H, sur établi
             Alimentation de labo limitée à 3 A / 24 V
                 │
   Étape 3   Maquette du tap Hall sur plaque à trous, UN canal
                 │
   Étape 4   motor_interface ──► lot de 5  (2 en service, 3 rechanges)
                 │
   Étape 5   PACK_ID, M1, M2, M10, M12
             ⚠️ M1 : fusible 40 A en série, pack ≤ 50 % SOC, hors habitation,
                sable sec ou extincteur classe D à portée
                 │
   Étape 6   safety_power ──► lot de 5
```

⚠️ **Deux règles de lot** :
1. **Commander deux fois la BOM.** Cinq PCB nus sans composants ne sont pas cinq rechanges.
2. **Vérifier la disponibilité des composants chez l'assembleur AVANT de figer le schéma.** Choisir un `TCAN1042HVDR` absent du stock oblige à tout re-router.

---

*Ce document remplace les passages correspondants du dossier v1.1. À lire avec `08-revue-conception-pcb.md`, `09-validation-composants.md`, `10-spec-motor-interface.md` et `hardware/icd.yaml`.*
