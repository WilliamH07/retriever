# X. Revue de conception des trois PCB — avant schéma

> ## ⚠️ Ce document a été partiellement invalidé — lire d'abord `09` et `11`
>
> Cette revue a été écrite **avant** la validation des composants par datasheet.
> Cinq de ses recommandations sont périmées :
>
> | Dans ce document | À remplacer par | Pourquoi |
> |---|---|---|
> | `SN74LVC14A` | **`SN74LVC3G17`** | TI a retiré la garantie `Ioff` du LVC14A |
> | `2N7002` | **`DMG1012T`** | Non garanti passant sous 3,3 V de grille |
> | `INA226` | **`INA228`** | 36 V de mode commun, inutilisable sur 42 V |
> | `TPS2595` sur le bus 42 V | **`TPS26630`** | Le TPS2595 plafonne à 18 V |
> | Entraxe ESP32 « 22,86 mm » | **25,40 mm** | 22,86 est l'entraxe des cartes tierces 30 broches |
>
> ⚠️ **Et une erreur d'architecture** : le §X.6 suppose, comme le dossier v1.1, que
> tirer `/SAFE` à la masse freine les moteurs. **C'est faux** — cela met les
> variateurs en roue libre et relâche le frein.
> Voir `10-spec-motor-interface.md` §Z.1 et `11-corrections-v1.2.md` §C-S1.
>
> Le raisonnement d'architecture de cette revue reste valable ; ce sont les
> références de composants et la chaîne de freinage qui ont changé.


**Version** : 1.0 — 24 août 2026
**Objet** : audit des deux README existants et du dossier d'architecture v1.1, avant gel des schémas de `motor_interface`, `safety_power` et `can_distribution`.
**Convention de marqueurs** : identique au dossier (`00-index-A-B.md`).

| Marqueur | Signification |
|---|---|
| ✅ **VÉRIFIÉ** | Datasheet constructeur, norme, ou source primaire |
| 🟡 **CONSENSUS** | Rétro-ingénierie communautaire recoupée |
| 🔴 **À MESURER** | Inconnu ou contradictoire — **interdiction de router avant mesure** |
| ⚠️ **RISQUE** | Point dangereux si mal traité |
| 📐 **HYPOTHÈSE** | Valeur que je pose faute de donnée |

---

# X.0 Résumé : ce qu'il faut trancher avant d'ouvrir KiCad

Sept points bloquants. Tant qu'ils ne sont pas tranchés, tout schéma sera à refaire.

| # | Décision | Pourquoi c'est bloquant | Ma recommandation |
|---|---|---|---|
| **D1** | **Le pack est-il un Classic ou un Pro ?** Le README `safety_power` dit `10INR19/66-4` → 10S4P, 474 Wh. Le dossier v1.1 (hypothèse H4) dit 10S3P, 280 Wh. | Change le budget de courant, le calibre de fusible, l'autonomie, et le BMS (donc le protocole UART) | Le marquage `-4` est décisif : c'est un pack **Pro / Pro 2, 37 V, 12800 mAh, 474 Wh** ✅. Le dossier doit être corrigé, pas le README. |
| **D2** | **Contacteur DC ou pas ?** Le README `safety_power` dit « manual isolator, no contactor, précharge sur le loom ». Le dossier fait du contacteur le **niveau 5** de la chaîne de sécurité. | Deux architectures incompatibles. Sans contacteur, il n'existe **aucune coupure matérielle commandable** : le dernier recours redevient un message logiciel — exactement ce que tu voulais éviter. | **Garder le contacteur.** Le README décrit une carte de distribution, pas une carte de sécurité. |
| **D3** | **Fusible de tête 40 A ou 80 A ?** | Le README dit 40 A ; le dossier dit MRBF 80 A. | **80 A.** Voir X.1.2 : un 40 A viole la sélectivité avec les 4 fusibles de branche à 25 A, et fondrait sur un franchissement d'obstacle légitime. Le budget de courant est imposé par l'ESP32-SAFETY, pas par le fusible. |
| **D4** | **`motor_interface` = 4 moteurs ou 2 moteurs ?** Le README décrit une carte 4 moteurs (8 connecteurs, 12 taps, 120 × 79 mm, déjà routée). Ta demande dit « même carte pour traction **ou** propulsion ». | Incompatible. Le dossier prévoit 2 nœuds MOTION × 2 roues. | **2 moteurs par carte, fabriquée en 5 exemplaires** : 2 en service, 3 rechanges universelles. |
| **D5** | **Où passent les signaux de commande VR / DIR / EL / STOP ?** | ⚠️ **Ni l'un ni l'autre des README ne les traite.** `motor_interface` n'a aujourd'hui aucun chemin de commande vers les ZS-X11H. Sans ça, il reste un faisceau volant qui annule l'intérêt de la carte. | Voir X.6.1 — c'est l'omission la plus importante de la revue. |
| **D6** | **`can_distribution` ne distribue-t-il que le CAN ?** | La ligne `/SAFE` traverse les trois cartes et n'est possédée par aucune. | La renommer **`safety_bus_distribution`** : CAN + `/SAFE` + alimentation des nœuds. Voir X.7. |
| **D7** | **Sépare-t-on le porteur MCU de la carte de fonction ?** | C'est la vraie réponse à « matériel générique + lots de 5 ». | Voir X.2 — je présente les deux options honnêtement, la décision t'appartient. |

---

# X.1 Contradictions entre les README et le dossier v1.1

## X.1.1 Identité de la batterie

Le README `safety_power` décode `10INR19/66-4` correctement : 10 cellules série, NMC cylindrique, format 18650, 4 en parallèle. Le contrôle 37 × 12,8 = 473,6 Wh contre 474 Wh sur l'étiquette est concluant.

Le dossier v1.1 (§C.2.1) décrit au contraire un `10INR19/66-3`, 10S3P, 7,8 Ah, 280 Wh — le pack **Classic**.

Les deux packs existent ✅ ; ce sont deux références commerciales différentes. Le `-4` / 474 Wh correspond au **M365 Pro / Pro 2**.

**Conséquences si c'est bien un Pro** :

| Grandeur | Classic (dossier) | Pro (README) | Impact |
|---|---|---|---|
| Énergie | 280 Wh | **474 Wh** | Autonomie ×1,7 |
| Cellules en parallèle | 3 | **4** | Courant admissible ×1,33 → 26–40 A au niveau cellule |
| BMS | STM8L + BQ76930 🟡 | 🔴 **à vérifier** — le BMS Pro n'est pas forcément identique | Le protocole UART §D.9 est à revalider |
| Seuil de coupure | 🔴 non publié | 🔴 non publié | **La mesure M1 reste obligatoire** |

⚠️ **Ne pas déduire le seuil BMS du nombre de cellules.** Le BMS peut très bien limiter au même endroit sur les deux packs. M1 reste bloquante pour `safety_power`.

📐 **Action** : photographier l'étiquette du pack **et** l'étiquette du BMS, puis corriger §C.2.1 du dossier. Cinq minutes, et ça débloque tout le dimensionnement.

## X.1.2 Fusible de tête : pourquoi 80 A et pas 40 A

Le raisonnement du README est séduisant : « une protection qui n'agit jamais avant la chose qu'elle protège est décorative ». Il est juste dans son principe et faux dans son application, parce qu'il confond deux fonctions :

| Fonction | Qui la remplit | Grandeur de dimensionnement |
|---|---|---|
| Empêcher le pack de se décharger trop fort | Le **BMS**, puis le **budget de courant logiciel** (ESP32-SAFETY + ACS758) | 20 A continu 📐 |
| Protéger le **câble de 25 mm²** contre un court-circuit franc | Le **fusible de tête** | Ampacité du câble et I_cc du pack |

Un fusible ne protège pas une batterie : il protège un conducteur. Le 25 mm² admet 92 A après déclassement (§D.6). Le fusible doit se placer sous ce chiffre, au-dessus des pointes légitimes.

**Et surtout, il y a un problème de sélectivité** que ni l'un ni l'autre des documents ne relève :

> Les branches moteur sont protégées par **4 × MIDI 25 A**. Un fusible de tête à 40 A est à **1,6×** le calibre d'une branche. La règle usuelle de sélectivité entre fusibles de même famille est **≥ 2:1**, en pratique 2,5 à 3:1 sur l'I²t de fusion totale.

Conséquence concrète d'un 40 A en tête : sur un court-circuit franc d'un ZS-X11H, **le fusible de tête et le fusible de branche fondent tous les deux**. On perd le robot entier au lieu d'une roue, et on perd surtout l'information de localisation du défaut. Le mode `DEGRADED` sur trois roues décrit au §M.2 devient impossible.

Ajouté à ça : 4 × 20 A de pointe = 80 A est un **franchissement d'obstacle légitime**, pas un défaut. Un 40 A fond en usage normal.

→ **MRBF 80 A / 58 V confirmé.** Le README doit être corrigé sur ce point.

## X.1.3 Le contacteur — la contradiction la plus grave

Le README `safety_power` :

> « The build uses a manual isolator, no contactor. […] It lives on the loom, not on the board. »

Le dossier §A.1-2 :

> « Le dernier niveau capable d'arrêter physiquement les moteurs est le contacteur DC principal, dont la bobine est alimentée à travers le contact NF du champignon. »

Ce ne sont pas deux variantes de la même carte, ce sont deux philosophies. Tu poses toi-même la question dans ta demande :

> « Un système où le dernier recours est uniquement un message CAN n'est pas équivalent à une coupure matérielle de puissance. »

C'est exactement le point. **Sans contacteur, il n'y a plus aucune coupure de puissance commandable** : le champignon ne fait plus que tirer `/SAFE`, c'est-à-dire demander poliment aux ZS-X11H de freiner. Si un MOSFET d'un variateur est collé en conduction — le mode de défaillance le plus courant d'un pont BLDC — `/SAFE` ne fait rien du tout. Seul un fusible qui fond ou une main sur le coupe-batterie arrête le robot.

→ **Le contacteur reste**, et `safety_power` doit l'intégrer : commande de bobine, économiseur PWM, diode de roue libre, temporisation RC ~1 s, relais de précharge, mesure V_pack et V_bus de part et d'autre, détection de contacteur collé (§M.2 panne 31).

## X.1.4 Périmètre de `motor_interface`

Le README décrit une carte 4 moteurs / 8 connecteurs / 12 taps / 1 ESP32, **déjà routée**. Ta nouvelle demande impose une carte générique traction **ou** propulsion, et le dossier §G.2 justifie longuement pourquoi il faut 2 nœuds MOTION de 2 roues et pas 1 nœud de 4.

La carte routée contredit les deux. Elle est aussi le pire des deux mondes : un seul ESP32 pour 4 roues (sa panne immobilise le robot), 8 unités PCNT saturées, et des faisceaux Hall longs qui traversent tout le châssis pour rejoindre une carte unique — alors que le §L4 du dossier insiste pour les garder courts, torsadés et blindés.

→ **2 moteurs par carte.** Le lot de 5 devient : 2 en service (AV, AR), 3 rechanges, et la même carte peut plus tard servir une 5ᵉ/6ᵉ roue, un bras ou une tourelle sans nouveau PCB.

## X.1.5 Le RC du tap Hall : le calcul du README est incomplet

Le README annonce :

> « RC : 1 kR + 10 nF, 10 us, corner near 16 kHz »

C'est vrai **uniquement sur le front descendant**. Sur le front montant, le condensateur est chargé par le pull-up, qui est **dans le ZS-X11H** et de valeur inconnue 🔴 :

```
τ_descente = R_série × C           = 1 k × 10 n  = 10 µs
τ_montée   = (R_pullup + R_série) × C
             si R_pullup = 4,7 k   → 57 µs
             si R_pullup = 10 k    → 110 µs  (t_r 10–90 % ≈ 242 µs)
```

À 110 Hz, une demi-période vaut 4,5 ms, donc 242 µs restent acceptables (5 %). Mais :

1. La constante annoncée est fausse d'un facteur 10 dans le pire cas, et la fréquence de coupure de 16 kHz — précisément celle qui devait rejeter le PWM des variateurs — descend à **1,4 kHz**, soit *en plein* dans la bande du hachage 16–20 kHz. Le filtre ne fait plus le travail qu'on lui demande.
2. L'asymétrie montée/descente introduit un **biais de rapport cyclique** qui se traduit, en décodage quadrature, par une erreur systématique de position par comptage.
3. ⚠️ **Et surtout : si un ZS-X11H est débranché ou mort, sa résistance de pull-up part avec lui.** L'entrée du tampon flotte, oscille, et l'ESP32 compte du bruit. L'odométrie devient fausse — ce qui, comme le note le §M.2 panne 15, est *pire* qu'une odométrie absente.

**Correctif** : mettre le pull-up **sur `motor_interface`**, pas dans le variateur.

```
+5 V_MOT ──[ 2,2 k ]──┬── ligne Hall ──[ 2,2 k ]──┬──[ 10 nF / 100 V ]── GND
                      │                           │
                      │                           ├──[ TVS 5,6 V ]── GND
                 (vers ZS-X11H)                   │
                                                  └──> tampon Schmitt ──> GPIO
```

- 2,2 k vers +5 V en parallèle du pull-up du variateur → τ déterministe (≈ 7 µs), coupure ≈ 22 kHz, et l'entrée ne flotte **jamais**, variateur branché ou non.
- Charge vue par le capteur Hall : 5 V / 2,2 k = 2,3 mA, à comparer aux ~20 mA que sait absorber un Hall à collecteur ouvert 🟡. Confortable.
- Le budget de 30 mA de la sortie logique du ZS-X11H 🔴 n'est pas entamé : les entrées LVC consomment quelques µA.

## X.1.6 Le tampon : famille logique et sens

Le README retient le `SN74LVC14AD` (hex Schmitt **inverseur**). Trois remarques.

**a) La famille LVC est le bon choix, et pour une raison précise qu'il faut écrire.** Les entrées/sorties LVC n'ont **pas de diode de clamp vers V_CC** (fonctionnalité *Ioff* / partial power-down) ✅. C'est exactement ce qui garantit l'exigence dure du README : 3,3 V absent, le tap ne charge pas la ligne Hall, le passthrough survit.

⚠️ **Corollaire à écrire en gras dans la BOM** : ne **jamais** substituer un `74HC14` ou `74HCT14` en cas de rupture de stock. Ces familles ont un clamp vers V_CC : hors tension, elles tirent la ligne Hall vers un rail mort et cassent la garantie.

**b) 12 canaux = 2 × LVC14A exactement, zéro réserve.** Sur une carte 2 moteurs (D4), c'est 6 canaux = 1 boîtier + 0 spare, ou 2 × `SN74LVC3G17` (triple Schmitt **non inverseur**) ✅ si tu veux éviter l'inversion.

**c) L'inversion est acceptable mais doit être documentée**, pas subie. En décodage quadrature, inverser les deux voies conserve le sens (les deux fronts se déplacent ensemble). En décodage 6 états — que je recommande, voir X.6.2 — la table d'états doit être complémentée, et la détection des états interdits `000` / `111` fonctionne toujours (ils s'échangent).

📐 Recommandation : **`SN74LVC3G17` non inverseur** si disponible dans le catalogue de ton assembleur, sinon LVC14A avec un commentaire explicite dans `protocol.h`.

## X.1.7 Le montage anti-backfeed USB : ingénieux mais fragile

Le README compte sur la chute du `SS14` (≈ 0,35 V) pour garantir 4,65 V < VBUS. Trois failles :

1. **Vf d'une Schottky s'effondre à faible courant.** À 20 mA, un SS14 chute ≈ 0,25 V ✅, pas 0,35. On arrive à 4,75 V.
2. **VBUS n'est pas 5,00 V.** La spec USB 2.0 autorise un port hôte à 4,40 V ✅. Un portable en fin de batterie sur 4,75 V face à une carte à 4,75 V : le sens du courant devient indéterminé.
3. **Marge d'entrée du régulateur du module.** 4,65 V − 1,3 V de dropout d'un AMS1117-3.3 à 1 A ✅ = 3,35 V. Ça tient avec le Wi-Fi désactivé (§G.2), mais il n'y a plus de marge.

Le silkscreen `5,0 V MAX` est une bonne idée, mais c'est une consigne humaine sur un chemin critique — exactement le genre de chose qu'on te demande d'éliminer.

**Alternatives, par robustesse croissante** :

| Option | Coût | Verdict |
|---|---|---|
| SS14 + consigne 5,0 V (README) | ~0,05 € | 🟡 Fonctionne si tout va bien |
| **Cavalier de déconnexion** du 5 V carte, à retirer avant de brancher l'USB | ~0,10 € | ✅ Simple, visible, mais manuel |
| **ORing à diode idéale** (`LM66100`, 1,5–5,5 V, ~79 mΩ, blocage inverse actif) | ~0,60 € | ✅✅ **Recommandé** — supprime totalement la contrainte de réglage du convertisseur |
| Multiplexeur d'alimentation (`TPS2116`) | ~1 € | ✅✅ Idem + priorité configurable |

→ Une diode idéale à ~0,60 € élimine une note de silkscreen qui pouvait détruire le port USB d'un PC. C'est le meilleur euro du projet.

---

# X.2 La proposition structurante : séparer le porteur MCU de la fonction

C'est la réponse la plus complète à ton double objectif « matériel générique » + « lots de 5 » + « remplacement sans dessouder ». Je te la présente honnêtement, avec ses inconvénients.

## Option A — trois cartes monolithiques (ce que tu proposes)

Chaque carte embarque son socket ESP32, son transceiver CAN, son protection d'entrée, ses LEDs, ses points de test.

| Pour | Contre |
|---|---|
| Moins de connecteurs, moins de hauteur | Le bloc ESP32 (socket, découplage, ESD, keepout antenne, strapping) est **conçu et re-vérifié 2 à 3 fois** |
| Un seul PCB à commander par fonction | Une erreur dans ce bloc est reproduite sur toutes les cartes |
| Chemins courts | Une carte de fonction grillée emporte la partie MCU avec elle |
| | 3 conceptions RF/4 couches au lieu d'une |

## Option B — un porteur générique + des cartes filles

```
   ┌──────────────────────────────────────────────────┐
   │  esp32_node  (×5, identique partout)             │
   │  socket 2 × 1×19 · CAN · protection d'entrée     │
   │  3,3 V · LEDs état · points de test · ID carte   │
   │  ── connecteur d'extension 2 × 20 ──             │
   └───────────────────┬──────────────────────────────┘
                       │  mezzanine
        ┌──────────────┼──────────────┬────────────────┐
        ▼              ▼              ▼                ▼
   motor_iface    io_periph      (libre)          (libre)
   Hall + VR/DIR  ventilos,      5ᵉ roue,         banc de
   + EL/STOP      WS2812B,       bras,            test
   ×2 moteurs     NTC, BNO085    tourelle
```

| Pour | Contre |
|---|---|
| ✅ Le bloc ESP32 est conçu **une fois**, validé une fois, corrigé une fois | ❌ Un connecteur mezzanine de plus (~2 €/paire, `Samtec` / `Hirose DF40` / simple 2×20 au pas 2,54) |
| ✅ Un porteur en panne se remplace sans toucher à la carte de fonction, et réciproquement | ❌ Hauteur d'empilement +10 à 15 mm |
| ✅ Les cartes de fonction deviennent simples : 2 couches, pas de contrainte RF | ❌ Une interface de plus à spécifier et à figer |
| ✅ 5 porteurs couvrent **tous** les nœuds actuels et futurs | ❌ Impédance/inductance du connecteur — non critique ici (aucun signal > 1 MHz sauf SPI 3 MHz) |
| ✅ Le nœud SAFETY, le nœud MOTION AV, le nœud MOTION AR et l'IO deviennent **le même matériel**, différenciés par la carte fille + un strap | |
| ✅ Une carte de banc de test se conçoit en une soirée | |

📐 **Mon avis** : l'option B correspond mieux à ce que tu décris. Tu dis vouloir que « le rôle soit déterminé uniquement par le firmware/configuration » — l'option B pousse ce principe jusqu'au bout : le nœud de calcul est *le même objet physique* partout, et seule la carte fille change.

Mais c'est un choix qui t'appartient, et l'option A est parfaitement défendable si tu veux limiter le nombre de références et de connecteurs. **Décide ça avant tout schéma** : ça change le découpage complet.

La suite du document est écrite pour rester valable dans les deux cas.

---

# X.3 Découpage révisé et périphériques orphelins

## X.3.1 Les périphériques qui n'ont aujourd'hui aucune carte

Ta liste de composants contient des éléments qui n'apparaissent **ni dans les README, ni dans le dossier v1.1** :

| Périphérique | Où est-il aujourd'hui ? | Où doit-il aller ? |
|---|---|---|
| **WS2812B avant + arrière** | ⚠️ **Nulle part.** Absent du dossier v1.1 | `io_peripherals` — voir X.9.4 |
| **Capteur de température 3 broches** (Power/GND/Data) | 🔴 Type non identifié | `io_peripherals`, avec entrée à double usage — X.9.5 |
| **2 ventilateurs PWM 12 V** | §N.7 : sur ESP32-SAFETY | `io_peripherals` ou `safety_power` |
| **BNO085** | §L6 : sur ESP32-SAFETY par SPI | ⚠️ **Surtout pas sur `safety_power`** — §N.3 : le magnétomètre doit être à ≥ 30 cm des moteurs et ≥ 25 cm des busbars. Connecteur sur `io_peripherals`, capteur déporté sur mât |
| **Alimentation Youyeetoo X1** | 12 V COMPUTE isolé | `safety_power`, sortie dédiée sur header XH2.54 |
| **Alimentation 5 V du YDLIDAR** (pointe 1 A ✅) | §C.2.5 | `safety_power`, branche 5 V dédiée avec eFuse |
| **Kinect 12 V / 2,67 A** | Bloc secteur séparé | `safety_power` : branche 12 V dédiée, ou renoncer |

⚠️ Si ces sept éléments ne sont attribués à aucune carte, ils atterriront par défaut sur `safety_power`, qui deviendra une carte monstre mélangeant 42 V de puissance et des signaux de capteurs — exactement ce que tu veux éviter.

**→ Il faut une quatrième carte, ou une carte fille `io_peripherals`.** C'est la conséquence directe de l'inventaire, pas une complication gratuite.

## X.3.2 Découpage proposé

| Carte | Responsabilité | Exemplaires en service | Lot de 5 |
|---|---|---|---|
| `safety_power` | Batterie → bus 37 V → 12 V / 5 V · fusibles · précharge · contacteur · mesure courant · hacheur de freinage · `/SAFE` maître | 1 | 1 + 4 rechanges (ou 2 si tu ajoutes un 2ᵉ pack plus tard) |
| `motor_interface` | 2 moteurs : Hall passthrough + tap + **commande VR/DIR/EL/STOP** + NTC variateur | 2 (AV, AR) | 2 + 3 rechanges universelles |
| `safety_bus_distribution` | CAN + `/SAFE` + alim des nœuds, protégés par branche | 1 | 1 + 4 (une par banc de test, une pour l'établi) |
| `io_peripherals` | Ventilos, WS2812B, NTC, BNO085, buzzer, LED de façade | 1 | 1 + 4 |
| *(option B)* `esp32_node` | Porteur MCU générique | 3 à 4 | pile |

---

# X.4 Le bloc « entrée protégée » — à concevoir une fois, à copier partout

Tu demandes que « la défaillance ou le mauvais branchement d'un module ne puisse pas endommager le reste du robot ». Ça se conçoit comme **un bloc unique, validé une fois, instancié sur chaque carte**. Voici la cellule que je recommande, dans l'ordre depuis le connecteur :

```
  Connecteur détrompé
        │
   ① [ PPTC ou eFuse ]        limitation de courant, réarmable
        │
   ② [ MOSFET P canal ]       anti-inversion de polarité, ~10 mΩ
        │   grille via zener 12 V + 100 k
        │
   ③ [ TVS unidirectionnelle ] transitoires, ESD, surtension franche
        │
   ④ [ FET série + comparateur ] OVLO — ouvre au-dessus du seuil
        │
   ⑤ [ ferrite + 10 µF + 100 nF ] découplage, réjection HF
        │
   ⑥ [ LED verte + 10 k ]     présence de rail — X.9.1
        │
        ▼  rail propre
```

Le point ④ est celui qu'on oublie et qui répond directement à ta demande.

⚠️ **Le scénario réel à couvrir** : quelqu'un branche un connecteur 12 V dans une prise 37 V, ou un connecteur 5 V dans une prise 12 V. Un TVS seul ne sauve rien : un TVS SMBJ13A face à 37 V permanents conduit ~30 A, se met en court-circuit et fait fondre le PPTC — dans le meilleur des cas. Il n'est pas conçu pour du continu, seulement pour des transitoires de quelques centaines de µs.

Un **OVLO série** (FET + comparateur à référence zener, ou un composant dédié type `LM5060` / `TPS2595` avec seuil d'OV programmable) **ouvre** au lieu de conduire. La carte ne démarre pas, une LED rouge s'allume, rien ne casse.

**Dimensionnement par rail** 📐 :

| Rail | Seuil OVLO | TVS | Limite de courant |
|---|---|---|---|
| 37 V (entrée bus sur `motor_interface` si tu l'alimentes en 37 V) | 45 V | SMCJ43A | selon branche |
| 12 V | 15,5 V | SMBJ13A | eFuse 2–3 A |
| 5 V | 6,0 V | SMAJ5.0A | eFuse ou PPTC 1 A |

📐 Coût de la cellule complète : environ **1,20 à 1,80 € par entrée**. Sur quatre cartes, c'est le prix d'un seul ESP32 grillé.

---

# X.5 `safety_power` — ce qui manque

## X.5.1 ⚠️ Le hacheur de freinage n'est pas une évolution de phase 4

Le dossier le range en « évolution recommandée » (§H.6). Je pense que c'est une erreur d'appréciation, pour une raison précise que ni le dossier ni le README ne formulent :

> **Ouvrir le contacteur pendant que les moteurs tournent crée une surtension garantie.**

Quand le contacteur s'ouvre, la batterie disparaît du bus. Les moteurs, eux, tournent encore. Les diodes de corps des ponts des ZS-X11H redressent la force contre-électromotrice dans les seuls condensateurs de bus (~10 mF 📐). Il n'y a plus rien pour absorber l'énergie. La tension monte jusqu'à ce que quelque chose cède.

C'est **exactement la séquence d'arrêt d'urgence** décrite au §H.7 : `/SAFE` puis, 1 s plus tard, ouverture du contacteur. La temporisation RC de 1 s a été conçue pour laisser le temps au freinage électrique — mais si le robot est encore en mouvement à t = 1 s (pente, freinage insuffisant, variateur muet), on ouvre sous force contre-électromotrice.

**Le hacheur de freinage doit donc être :**

| Exigence | Raison |
|---|---|
| **Côté bus, en aval du contacteur** | Sinon il est déconnecté au moment précis où on en a besoin |
| **Entièrement analogique** — comparateur à hystérésis, pas de MCU | Doit fonctionner ESP32 planté, ESP32 en reset, ESP32 absent |
| **Alimenté par le bus lui-même** | Aucune dépendance à un rail logique |

**Dimensionnement** 📐 (hypothèse H2 : 35 kg, H3 : 1,5 m/s) :

| Cas | Calcul | Puissance |
|---|---|---|
| Arrêt d'urgence depuis 1,5 m/s | ½ × 35 × 1,5² = **39 J** | négligeable en énergie |
| Descente continue à 10 % à 1,5 m/s | 35 × 9,81 × 0,0995 × 1,5 | **51 W** |
| Descente continue à 15 % à 1,5 m/s | 35 × 9,81 × 0,148 × 1,5 | **76 W** |

→ Résistance **15 à 22 Ω**, **100 W**, boîtier aluminium sur dissipateur ou sur le châssis, **jamais sur le PCB**. À 42 V : 2,0 à 2,8 A, 80 à 118 W crête.
→ MOSFET N canal 100 V / ≥ 30 A, `R_DS(on)` ≤ 10 mΩ, avec dissipateur ou pavé thermique + vias.
→ Comparateur à hystérésis : **ON à 42,5 V, OFF à 41,8 V** 📐 — à recaler après M1 et M12. La fenêtre est étroite parce que la pleine charge est à 42,0 V et le seuil de coupure surtension du BMS juste au-dessus ✅ (4,2 V/cellule).
→ Une LED dédiée « BRAKE CHOPPER ACTIF » : si elle clignote souvent, c'est un diagnostic gratuit sur le style de conduite et le SOC.

⚠️ **Coordination avec la TVS** : le §D.8 note à juste titre qu'une TVS avec standoff > 42 V écrête vers 72–80 V, au-dessus de la tenue de condensateurs 63 V. Le hacheur résout précisément ce trou : il écrête à 42,5 V, bien en dessous. La TVS reste pour les transitoires rapides (< µs) que le hacheur ne peut pas suivre. **Les deux sont complémentaires, pas redondants.**

## X.5.2 Mesure de courant par branche

Absent de ta liste et du dossier. Le §C.2.2 note que **le ZS-X11H n'a aucune limitation de courant réglable** : entre « tout va bien » et « le fusible de 25 A a fondu », il n'y a strictement aucune information.

Un shunt de 2 mΩ + `INA226` (I²C) sur chacune des 4 branches moteur donne :

- Le courant réel de chaque roue, à 1 % près, à quelques centaines de Hz.
- La **détection précoce** d'un moteur qui force, d'un roulement qui grippe, d'un variateur qui dérive — bien avant le seuil de calage à 300 ms du §H.5.
- Une **limitation de courant par roue** en logiciel, que le ZS-X11H ne sait pas faire.
- Un diagnostic de déséquilibre gauche/droite qui distingue le patinage du blocage.

📐 Coût : ~2,50 € par branche, 10 € au total, 4 adresses I²C sur le même bus. C'est probablement le meilleur ajout de tout ce document en rapport information/prix.

⚠️ Attention : le shunt côté **low-side** est plus simple (référence = masse) mais insère une résistance dans le retour, ce qui contredit le point de masse unique du §D.7. En **high-side**, l'INA226 supporte jusqu'à 36 V de mode commun — **insuffisant pour un bus à 42 V**. Il faut alors l'`INA228` (85 V) ✅ ou l'`INA240` avec un ampli séparé. → **`INA228` en high-side** est le bon choix, et il donne en prime 20 bits et un accumulateur de charge intégré.

## X.5.3 Détection de fusible fondu

Trois résistances et un opto par fusible, ou un simple diviseur vers un comparateur. Un fusible fondu devient un événement CAN et une LED rouge au lieu d'une heure de recherche avec un multimètre sur un robot démonté.

📐 ~0,40 € par branche. Sur 6 branches : 2,50 €.

## X.5.4 eFuse plutôt que fusible lame sur les rails logiques

Sur les branches 12 V et 5 V, un fusible lame est un mauvais outil : il est lent, il ne se réarme pas, il n'a pas de démarrage progressif, et il ne dit rien.

Un `TPS2595` / `TPS25940` par branche apporte :

| Fonction | Bénéfice |
|---|---|
| Limitation de courant programmable | Le pic d'appel du YDLIDAR (1 A ✅) ne fait plus sauter la branche |
| Démarrage progressif (`dV/dt` programmable) | Supprime les appels de courant au branchement à chaud |
| Blocage inverse | Une carte en défaut ne refoule pas sur le rail |
| Broche `FAULT` | → LED + événement CAN |
| Réarmement | Pas de fusible à changer au fond du châssis |

📐 ~1,50 € par branche. Les fusibles lame restent en revanche sur les **quatre branches moteur 37 V / 25 A**, où aucun eFuse abordable ne tient.

## X.5.5 Deux rails 5 V, pas un

Le §G.2 note que l'ESP32-SAFETY doit être alimenté **en amont du contacteur**, sinon il ne peut pas le fermer. Cela impose deux sources 5 V distinctes sur `safety_power` :

```
   V_PACK (amont contacteur) ──[ MIDI 2 A ]── DC/DC 42→5 V, 5 W ──► 5V_HOTEL
                                                      └──► ESP32-SAFETY, CAN, /SAFE

   BUS (aval contacteur) ──[ MIDI 5 A ]── DC/DC 42→12 V AUX ──► DC/DC 12→5 V ──► 5V_LOGIC
                                                      └──► ESP32-MOTION ×2, capteurs, lidar
```

⚠️ **Les deux rails 5 V partagent une masse mais pas une source.** Il faut décider explicitement s'ils sont diodés ensemble (le CAN a besoin d'un 5 V continu) ou strictement séparés. Je recommande **séparés**, avec le CAN alimenté depuis `5V_HOTEL` : le bus CAN doit vivre quand le contacteur est ouvert, sinon on perd le diagnostic au moment exact où on en a besoin.

## X.5.6 Divers

| Point | Recommandation |
|---|---|
| **Décharge active du bus** | §D.8 constate que 10 mF mettent 50 s à se décharger passivement. → MOSFET + 47 Ω / 50 W piloté par l'ESP32-SAFETY à l'extinction, **plus** la LED rouge « BUS SOUS TENSION » alimentée directement par le bus (LED + 10 k), sans aucun microcontrôleur. |
| **Le point d'étoile de masse est un pad nommé** | Ne pas laisser la jonction des masses puissance/logique se faire « quelque part » à la frontière de deux plans. Un via/pad unique, sérigraphié `★ GND STAR`, et un plan logique relié là et nulle part ailleurs. |
| **ACS758 : NRND et surdimensionné** | §A.5 est catégorique. Si tu redessines de toute façon, c'est le moment : soit conserver la carte externe existante (ton choix) **et** ajouter l'`INA228` sur le bus principal pour la précision, soit passer en `ACS772`/`ACS773`. La combinaison ACS758 (rapide, isolé, seuil de défaut) + INA228 (précis, lent) est en réalité la meilleure : deux capteurs indépendants sur la même grandeur = **contrôle de plausibilité gratuit**, qui détecte une sortie figée (§C.1). |
| **Contacteur collé** | §M.2 panne 31 : mesurer V_bus alors que la commande est ouverte. Le diviseur V_bus le permet déjà — s'assurer qu'il est **en aval** du contacteur et V_pack **en amont**. |
| **Bornier de sortie 37 V à 25 A** | Un bornier à vis 5,08 mm est typiquement donné pour 17,5 A ✅ — insuffisant. Il faut du **7,62 mm** (≈ 41 A), des **inserts M4 + cosses à œil**, ou des **XT60 embarqués**. À trancher avec X.9.3. |
| **Thermique** | Les FET de diode idéale, le hacheur, les shunts et les DC/DC concentrent la dissipation. Prévoir les pavés, les vias thermiques, et **ne pas router de piste 37 V sous un composant chaud**. |
| **Coordination avec le §D.4** | 🔴 M12 (tension des condensateurs d'entrée des ZS-X11H) reste bloquante. Si ils sont marqués 50 V, le hacheur de freinage à 42,5 V devient **obligatoire et non plus recommandé**, et il faut plafonner la charge. |

---

# X.6 `motor_interface` — ce qui manque

## X.6.1 ⚠️ Le chemin de commande est absent des deux documents

C'est l'omission la plus importante de la revue.

Tu écris : « Cette carte fait l'interface entre les moteurs, les contrôleurs moteur et les microcontrôleurs. » Or les fonctions listées ne couvrent que le **retour** (Hall) et le CAN. Le chemin **aller** — ESP32 → ZS-X11H — n'apparaît nulle part :

| Signal | Nature | Source dossier |
|---|---|---|
| `VR` | analogique 0–5 V, démarrage à ≈ 0,07 V 🟡 | §H.3 : MCP4728 + AOP rail-to-rail |
| `DIR` / `ZF` | logique, actif bas 🟡 | §F.5-L3 : MOSFET collecteur ouvert |
| `EL` / `BRAKE` | 🔴 **polarité contradictoire — M4** | idem |
| `STOP` | logique, actif bas 🔴 source unique | idem |
| NTC dissipateur variateur | analogique | §H.5 |

Si ces signaux ne sont pas sur `motor_interface`, ils sont sur un faisceau volant entre l'ESP32 et chaque variateur — c'est-à-dire précisément le câblage désordonné que la carte devait supprimer.

→ **`motor_interface` doit porter, pour 2 moteurs** :

```
   ESP32 ──I²C──► MCP4728 ──► AOP (alim 5 V) ──► VR ×2   + 10 k vers GND ★
         ──GPIO─► 2N7002 ×2 ──────────────────► DIR ×2
         ──GPIO─► 2N7002 ×2 ──────────────────► EL ×2    ⚙ polarité configurable
         ──GPIO─► 2N7002 ×2 ──────────────────► STOP ×2  ⚙ polarité configurable
         ──ADC◄── diviseur + filtre ◄────────── NTC ×2
```

★ **La résistance de 10 k vers la masse sur `VR`, au plus près du connecteur variateur, n'est pas optionnelle** (§F.5-L3) : elle transforme une rupture de fil en consigne nulle plutôt qu'en entrée flottante à comportement indéfini.

⚙ **L'astuce qui débloque M4** : la mesure M4 (polarité de `EL`/`STOP`) bloque aujourd'hui toute la chaîne de sécurité. Tu peux la contourner **au niveau du PCB** en dessinant un étage de sortie agnostique en polarité :

```
   GPIO ──► MOSFET drain ouvert ──┬── [ R_PU option, vers 5 V ] ── ⚙ JP1
                                   ├── [ R_PD option, vers GND ] ── ⚙ JP2
                                   └── ► sortie EL / STOP
   + empreinte d'inverseur SOT-23 en série, montée ou pontée par 0 Ω
```

Trois empreintes non peuplées, ~0,15 €. Le PCB devient valable quelle que soit la réponse de M4, et la mesure ne bloque plus que le **firmware** et le **peuplement**, pas le routage. C'est exactement le genre d'option qu'il faut poser quand une mesure bloquante n'est pas encore faite.

## X.6.2 Dériver les trois Hall, pas deux

Le §H.4 propose de tapper 2 lignes sur 3 pour un décodage quadrature (60 comptes/tour, 8,6 mm de résolution). Le README `motor_interface` tappe les 3. Le README a raison, pour trois raisons :

| Avantage des 3 lignes | Détail |
|---|---|
| **Résolution** | Décodage 6 états = **90 comptes/tour** au lieu de 60 → 5,8 mm au lieu de 8,6 mm |
| ⚠️ **Détection de panne Hall** | Le §M.2 panne 15 exige de détecter « une séquence d'états Hall invalide ». **C'est impossible avec 2 lignes** : toute combinaison de 2 bits est valide. Avec 3 lignes, `000` et `111` sont interdits et une transition non adjacente est détectable immédiatement. |
| **Diagnostic** | Un capteur mort se localise directement (quelle ligne reste figée) |

Contrainte : le décodage 6 états ne rentre pas dans une unité PCNT en mode quadrature. Deux issues :

1. **Décodage par interruption GPIO.** À 110 Hz max par ligne (§H.4), 3 lignes × 2 moteurs = 660 interruptions/s. Sur un ESP32 à 240 MHz, c'est négligeable — même avec une ISR de 200 cycles, on est à 0,06 % de CPU.
2. **PCNT sur 2 lignes + 3ᵉ ligne en GPIO simple** pour la seule validation d'état. Compromis : garde le comptage matériel, ajoute le diagnostic.

📐 Recommandation : **tapper les 3 lignes en matériel** (le coût est un RC et un canal de tampon par ligne), et laisser le firmware choisir. C'est précisément la philosophie « matériel générique, rôle défini par le firmware » que tu défends.

Sur une carte 2 moteurs : **6 canaux de tap**, soit 1 boîtier hex ou 2 triples.

## X.6.3 ⚠️ Protection contre un contact phase / Hall

Ni l'un ni l'autre des documents ne traite ce scénario, qui est pourtant le plus probable des défauts de câblage sur un robot à hoverboard :

> Un fil de phase (37 V haché à 16–20 kHz, plusieurs dizaines d'ampères) touche une ligne Hall, dans le connecteur, dans le passage de roue, ou par abrasion de la gaine.

Aujourd'hui, avec 1 k en série : 37 V / 1 k = **37 mA** injectés dans les diodes de clamp du tampon LVC, dont l'entrée est donnée pour ±20 à ±50 mA en absolu selon la référence ✅. On est au mieux à la limite, au pire au-dessus. Et le 10 nF voit 37 V : un X7R 25 V n'y survit pas.

**Trois correctifs, cumulables, quasi gratuits** :

| Correctif | Effet |
|---|---|
| Résistance série **2,2 k au lieu de 1 k** | 37 V → 17 mA, sous les limites de clamp |
| **TVS 5,6 V ou diode Zener** de chaque entrée de tampon vers GND | Écrête avant le clamp interne, absorbe l'énergie hors du CI |
| Condensateur **100 V** au lieu de 25 V | Coût identique en X7R 0603 |
| **PPTC 100 mA sur la branche +5 V Hall** de chaque moteur | Un court +5 V ↔ phase ne remonte pas jusqu'au variateur voisin |

Le tout ajoute environ **0,80 € par moteur** et transforme un défaut destructeur en un défaut détectable.

## X.6.4 Le passthrough doit rester métallique — y compris dans le routage

Le README l'énonce comme exigence dure, ce qui est juste. Traduit en contraintes de routage :

- Les pistes de passthrough sont **droites, courtes, sans via si possible**, et suffisamment larges pour être robustes (0,5 mm minimum même si 20 mA suffisent).
- Le tap se branche par un **stub court** sur la piste de passthrough, pas en série.
- **Un plan de masse séparant physiquement** la zone passthrough de la zone tap/ESP32.
- ⚠️ Aucun composant du tap ne doit pouvoir, en défaillant en court-circuit, tirer une ligne Hall vers un rail. Un condensateur en court-circuit à la masse tue la ligne — c'est pourquoi le 10 nF est en aval du 2,2 k série, pas directement sur la ligne. **À vérifier explicitement au schéma.**
- Test de recette : la mesure de continuité en 5 points × 2 connecteurs du README (`Bring up` étape 1) est excellente ; **la généraliser en procédure de recette écrite pour les 5 cartes du lot**.

## X.6.5 Le rôle traction / propulsion

Trois voies, cumulables :

| Mécanisme | Usage |
|---|---|
| **2 GPIO de strap** (§G.3 : GPIO16/17 à la masse ou à 3,3 V) | Détermine l'ID CAN et le rôle au boot |
| **Résistance de config** vers un ADC | Plus de valeurs possibles, une seule broche |
| **Zone de sérigraphie à marquer au feutre** | Pour l'humain qui ouvre le robot dans six mois |

⚠️ **Et il faut que la carte publie son rôle et son ID matériel dans le heartbeat CAN**, à côté du hash de protocole que le §L.5 prévoit déjà. Une carte de rechange issue d'un lot ultérieur ne doit pas pouvoir se glisser silencieusement dans le système avec une révision différente. → 3 straps de révision = 8 révisions, lus au boot, publiés.

---

# X.7 `can_distribution` → `safety_bus_distribution`

## X.7.1 La ligne `/SAFE` n'appartient à aucune carte

Le §H.7 fait de `/SAFE` l'ossature de la sécurité : tirée par le champignon, par l'ESP32-SAFETY et par le watchdog externe ; consommée par les 4 ZS-X11H et les 2 ESP32-MOTION. Elle traverse donc **exactement le même chemin physique que le bus CAN**.

Il serait absurde de tirer deux faisceaux parallèles. → **`can_distribution` distribue le CAN *et* `/SAFE`.**

**Spécification électrique de `/SAFE`** (à figer avant tout schéma) :

```
   Un seul pull-up : 4,7 k vers 5V_HOTEL, sur safety_power
   Tirée à la masse (câblage OU) par :
     · le contact NF du champignon          — directement, sans électronique
     · le drain ouvert de l'ESP32-SAFETY
     · le watchdog externe TPL5010
     · (futur) le contact NF de l'e-stop radio
   Lue / consommée par :
     · les entrées EL/STOP des 4 ZS-X11H    ⚠️ polarité = M4
     · les entrées d'interruption des 2 ESP32-MOTION
```

⚠️ **Sécurité positive** : fil coupé, connecteur débranché, carte absente, alimentation perdue → la ligne tombe à la masse → état sûr. C'est le §G.3 ★, et il faut que **chaque carte** ait un pull-down local qui garantit cet état si le fil de `/SAFE` se débranche de son côté.

⚠️ **Boucle de champignon surveillée** : un contact NF simple ne détecte pas un contact oxydé qui reste fermé (§M.2 panne 32). Le §K prévoit un test actif au boot — bien. Pour aller plus loin : un champignon à **double contact NF** et deux entrées lues séparément permet de détecter la divergence en continu, pas seulement au boot. 📐 Surcoût ~10 €, à ton appréciation.

## X.7.2 Topologie : ne pas fabriquer une étoile sans le dire

Une carte de distribution avec 6 connecteurs **est** une topologie en étoile. ISO 11898-2 demande une ligne avec deux terminaisons aux extrémités et des dérivations courtes.

À 500 kbit/s sur ≤ 2 m (§F.2), une étoile passe sans difficulté : le temps de bit est de 2 µs, la propagation aller-retour sur 30 cm de stub est de ~3 ns. Ce n'est pas un problème technique — c'est un problème de **documentation** : si ce n'est pas écrit, quelqu'un ajoutera un jour un stub de 3 m.

→ Prévoir **les deux** :

```
   Mode chaîne (recommandé)      Mode étoile (dépannage / banc)
   ┌───┐  ┌───┐  ┌───┐           ┌──── hub ────┐
   │IN │──│OUT│  │IN │──...      │  │  │  │  │ │
   └───┘  └───┘  └───┘           n1 n2 n3 n4 n5
   120 Ω aux 2 extrémités        120 Ω sur 2 branches, stubs ≤ 30 cm
```

Chaque nœud reçoit **deux connecteurs CAN en parallèle** (c'est déjà ce que fait le README `motor_interface` ✅ : « two parallel 3 pin connectors so the bus passes through without a stub »). La carte de distribution devient un **point de brassage** plutôt qu'un hub, avec une contrainte de longueur de stub sérigraphiée : `STUB MAX 30 cm`.

## X.7.3 Terminaison : la vérifier, pas seulement la sélectionner

Un cavalier de terminaison est un piège classique : personne ne sait jamais combien il y en a d'actifs dans le système.

| Mesure | Recommandation |
|---|---|
| **Terminaison fendue** (2 × 60 Ω + 4,7 nF vers GND) au lieu d'un 120 Ω unique | Réduit fortement les émissions de mode commun. Coût : un condensateur. Toujours faire ça. |
| **Points de test CANH / CANL / GND** au pas 2,54 sur chaque carte | Une pince de sonde s'y clipse |
| **Sérigraphie du test de recette** : `bus hors tension → 60 Ω entre TP1 et TP2 = OK` | 120 Ω = une seule terminaison, 40 Ω = trois. Diagnostic en 5 secondes, sans outil spécial |
| **LED de terminaison active** à côté du cavalier | On voit d'un coup d'œil combien il y en a |

## X.7.4 ⚠️ Le transceiver : changer de référence

Le §F.2 retient le `SN65HVD230`. C'est un bon composant 3,3 V natif, mais ses broches de bus sont données pour **−4 V à +16 V** ✅.

Or ton exigence est : « la défaillance ou le mauvais branchement d'un module ne doit pas endommager le reste du robot ». Sur un robot où le CAN, le 12 V et le 37 V cheminent ensemble, le scénario « une ligne de puissance touche CANH » n'est pas théorique. Avec un HVD230, il détruit **tous les transceivers du bus simultanément** — les quatre nœuds d'un coup.

→ **`TCAN1042` / `TCAN1051` avec broche `VIO`** ✅ : alimentation bus en 5 V (meilleure amplitude différentielle), niveaux logiques en 3,3 V via `VIO`, et surtout **protection de bus à ±58 V**. Un court avec le bus 42 V devient survivable.

Options complémentaires :

| Ajout | Effet | Coût 📐 |
|---|---|---|
| Réseau de diodes ESD dédié CAN (`PESD1CAN`, `NUP2105L`) | ESD, transitoires | 0,20 € |
| **Self de mode commun** (`ACT45B`, `B82789C`) à chaque nœud | Réjection du bruit des ZS-X11H — vraiment utile ici | 0,50 € |
| Transceiver **isolé** (`ISO1042`) sur le seul nœud SAFETY | Le nœud de sécurité survit à un défaut de masse du bus | 3 € |

⚠️ **Le piège de la masse CAN** : si la masse d'un nœud se débranche côté alimentation mais reste connectée par le fil de masse du câble CAN, tout le courant de retour de ce nœud passe par un conducteur de 0,5 mm² prévu pour quelques dizaines de mA. Il fond, ou il chauffe et fait dériver toutes les masses. → **PPTC 500 mA ou résistance 10 Ω en série sur la masse CAN de chaque branche** sur la carte de distribution. C'est une des protections les plus rentables du document.

## X.7.5 Alimentation des nœuds

La carte distribue 5 V (et/ou 12 V) aux nœuds sur le même connecteur que le CAN. Alors :

- **Un eFuse ou un PPTC par branche**, jamais un rail commun nu. Un nœud en court ne doit pas éteindre les trois autres.
- **Une LED verte par branche** : on voit immédiatement quel nœud est alimenté.
- **Un connecteur détrompé par branche**, et si possible **un nombre de broches différent** de tout autre connecteur du robot.
- 📐 Prévoir **8 branches** (4 nœuds actuels + USB-CAN + 3 pour l'évolution du §A.4).

---

# X.8 Le module ESP32 sur supports — les pièges du brochage que tu as fourni

Ton brochage (19 + 19 = 38 broches, avec `CLK / SD0 / SD1 / SD3 / SD2 / CMD`) correspond à une **DevKitC ESP32-WROOM-32** ou un clone. Points à traiter avant de router.

## X.8.1 ⚠️ GPIO 6 à 11 = bus de flash SPI — interdits

`CLK`, `SD0`, `SD1`, `SD2`, `SD3`, `CMD` sont GPIO6–11, connectés à la puce flash du module ✅. **Aucune connexion, jamais.** Les brocher, même en test point, dégrade le bus flash et empêche le boot.

→ Prévoir les positions dans le support (mécanique) mais les laisser en `NC`, et **le sérigraphier explicitement** : `NC — FLASH — NE PAS UTILISER`. Six positions perdues, c'est le prix du support amovible.

## X.8.2 ⚠️ Les broches de strapping

| GPIO | État requis au boot ✅ | Conséquence si violé | Où c'est utilisé dans le dossier |
|---|---|---|---|
| **GPIO0** | HAUT (pull-up interne) | Passe en mode téléchargement | libre |
| **GPIO2** | BAS ou flottant | Boot bloqué si tiré haut alors que GPIO0 bas | §G.3 : `WAKE` du TPL5010 (SAFETY) et NTC (MOTION) |
| **GPIO5** | HAUT | Timing SDIO | §G.3 : **CAN TX** — vérifier que l'entrée `TXD` du transceiver ne le tire pas bas au repos |
| **GPIO12 (MTDI)** | ⚠️ **BAS** | Tension flash à 1,8 V → **le module ne démarre plus** | §G.3 : **PWM ventilateur 1 (SAFETY)** |
| **GPIO15 (MTDO)** | HAUT (pull-up interne) | Journal de boot supprimé (non bloquant) | §G.3 : CS du BNO085 (SAFETY), entrée `/SAFE` (MOTION) |

⚠️ **GPIO12 est le piège mortel.** Avec le PWM ventilateur assigné dessus, si tu mets une LED de debug vers 3,3 V ou une résistance de pull-up sur cette ligne — ce que tu demandes justement pour la visualisation d'état — **le module ne démarrera plus jamais**, et le symptôme (boot loop silencieux) est déroutant.

→ Sur GPIO12 : **pull-down 10 k obligatoire**, LED en montage low-side uniquement, grille de MOSFET avec pull-down. Ou plus simple : **déplacer le PWM ventilateur sur une autre broche** et laisser GPIO12 non connecté. Je recommande la seconde solution.

## X.8.3 GPIO 34–39 : entrées seules, sans pull-up interne

Le §G.3 le note déjà ✅. À ajouter : `SVP` = GPIO36 et `SVN` = GPIO39 dans ton brochage. GPIO37 et GPIO38 ne sont pas sortis sur WROOM-32. Toute entrée sur 34/35/36/39 exige un **pull-up ou pull-down externe 10 k**, sinon elle flotte — et une entrée `/SAFE` flottante est un défaut de sécurité, pas un désagrément.

## X.8.4 ⚠️ Keepout d'antenne

Avec un module sur supports, l'antenne PCB du WROOM surplombe ta carte. Espressif demande **aucun cuivre, aucun plan, aucune piste** sous la zone d'antenne ✅.

→ Deux solutions : placer le module en **bord de carte, antenne en débord**, ou **fraiser une découpe** sous l'antenne. La première est préférable — elle rend aussi le connecteur USB accessible, ce qui est indispensable pour reflasher sans démonter le robot (c'est ton exigence de maintenance).

Le Wi-Fi étant désactivé (§G.2), le keepout est moins critique — mais un plan de cuivre sous une antenne active peut aussi provoquer des courants de mode commun sur la carte. Le respecter coûte zéro.

## X.8.5 🔴 Vérifications physiques avant routage

| # | À vérifier | Comment | Bloque |
|---|---|---|---|
| **P1** | **Entraxe réel des deux rangées** | Pied à coulisse. 22,86 mm (0,9") sur DevKitC 38 broches, **25,4 mm (1,0")** sur certains clones | Le PCB entier |
| **P2** | **Ordre exact des broches**, sur **le** module que tu as | Continuité au multimètre : `5V` ↔ VBUS du connecteur USB ; `3V3` ↔ sortie du régulateur ; `GND` ↔ blindage USB | L'affectation complète |
| **P3** | Nombre de broches par rangée | Compter | Choix des supports |
| **P4** | Présence d'un condensateur sur `EN` | Inspection | Fiabilité du reset |

⚠️ **Insertion à l'envers** : deux rangées de 19 broches symétriques se laissent insérer retournées à 180°, ce qui met le 5 V sur GPIO23 et détruit le module. Rendre l'erreur impossible :

- **Boucher une position** (broche coupée côté module ou trou non métallisé côté carte) en un endroit asymétrique,
- ou décaler les deux supports d'un pas l'un par rapport à l'autre,
- ou placer un composant haut qui gêne l'insertion inversée,
- **et** une grosse flèche sérigraphiée `USB →` plus un liseré épais sur le côté broche 1.

## X.8.6 Alimentation et découplage

- **Ne pas alimenter par la broche `5V`** si tu adoptes l'ORing à diode idéale de X.1.7 ; ou alors avec cette diode idéale.
- Découplage : **10 µF + 100 nF au plus près de chaque broche d'alimentation** du support, plus 100 µF de bulk sur le rail 3,3 V.
- **TVS ESD** sur le rail d'alimentation à l'entrée de la carte (X.4).
- Si tu montes le module en 3,3 V direct (alimentation par la broche `3V3`), sache que tu **court-circuites le régulateur AMS1117 du module** et sa protection. C'est pratique courante, ça fonctionne, mais ce n'est pas une configuration validée par Espressif — à documenter comme choix conscient.
- **4 couches recommandé** pour la carte portant l'ESP32 : `Signal / GND / Alim / Signal`, avec un plan de masse continu sous le module ✅.

---

# X.9 LEDs, points de test, connectique, maintenance

## X.9.1 Plan de LEDs

Tu as listé les fonctions. Voici ce que j'ajoute, avec les pièges.

| Carte | LED | Couleur | Pilotage |
|---|---|---|---|
| Toutes | Présence rail d'entrée | vert | ⚠️ **Directement sur le rail, jamais par le MCU** — sinon elle ne dit rien quand le MCU est mort |
| Toutes | 3,3 V présent | vert | idem, direct |
| Toutes | Défaut d'entrée (OVLO / inversion) | rouge | Sortie du comparateur |
| Toutes | Heartbeat MCU | bleu | Clignotement 1 Hz par le firmware — **l'absence de clignotement est l'information** |
| Toutes | Activité CAN | jaune | Sur `RXD` du transceiver, via monostable ou par firmware |
| `safety_power` | **BUS SOUS TENSION** | **rouge, gros** | ⚠️ Alimentée par le bus lui-même via 10 k. Aucune électronique. §D.8 |
| `safety_power` | `ARM` / `DISARM` | orange / vert | Reflète l'état du contacteur, **pas** la demande |
| `safety_power` | Contacteur fermé | blanc | Sur la bobine, pas sur la commande |
| `safety_power` | Hacheur de freinage actif | rouge clignotant | Sortie du comparateur |
| `safety_power` | Fusible fondu ×6 | rouge | X.5.3 |
| `motor_interface` | Activité Hall ×6 | vert | Voir la mise en garde ci-dessous |
| `motor_interface` | 5 V moteur présent | vert | Direct sur le rail issu du variateur |
| `motor_interface` | `/SAFE` actif | rouge | Direct sur la ligne, avec un tampon |
| `safety_bus_dist` | Alim par branche ×8 | vert | Direct |
| `safety_bus_dist` | Terminaison active ×2 | bleu | À côté du cavalier |

⚠️ **Les LEDs d'activité Hall du README** (12 LEDs, une par canal, pilotées par les sorties du tampon) : excellentes en recette (« tourne la roue à la main, trois LEDs clignotent en séquence » — étape 5 du bring-up, très bonne idée), mais à 110 Hz en fonctionnement elles ne sont qu'un flou lumineux. Trois précautions :

1. LED **haut rendement à 2 mA** avec 1 kΩ, pas 20 mA : 6 LEDs × 20 mA = 120 mA gaspillés en permanence et une charge non négligeable sur les sorties du tampon.
2. Les mettre sur des sorties de tampon **dédiées**, pas sur celles qui vont aux GPIO — pour ne pas ajouter de capacité sur le chemin de comptage.
3. ⚠️ **Un cavalier ou un MOSFET d'inhibition globale des LEDs.** Un robot autonome en extérieur la nuit avec 20 LEDs allumées est visible de loin, et c'est de l'énergie perdue. Le firmware doit pouvoir tout éteindre.

📐 Ajout utile : une **LED de défaut à mémoire** (bascule RS ou simple latch firmware) qui reste allumée après l'événement. Un défaut fugitif qui allume une LED pendant 5 ms n'est jamais vu par personne.

## X.9.2 Points de test

Au-delà de ta liste :

| Point | Pourquoi |
|---|---|
| **Un point de masse « ressort de sonde » par carte** | Une masse par pince crocodile à 15 cm d'un signal à 20 kHz donne une mesure fausse. Un plot de masse à côté de chaque groupe de points de test |
| Points de test **nommés en sérigraphie**, pas numérotés | `TP_V12`, `TP_CANH` — pas `TP7` |
| Un **header 3 broches CANH / CANL / GND** au pas 2,54 | S'y clipse un adaptateur de sonde |
| Point de test sur `/SAFE`, sur chaque carte | C'est le signal le plus critique du robot |
| Points de test sur **chaque ligne Hall, avant et après le tampon** | Diagnostic direct du tap |
| Une **prise 4 mm ou un plot vissé** pour V_bus sur `safety_power` | Mesurer 42 V sans clipser une pince sur un composant CMS |
| **Boucle de courant** (piste large avec fente) pour pince ampèremétrique | Mesurer un courant de branche sans couper de fil |

## X.9.3 Connectique — la règle anti-erreur

Tu demandes des borniers et du démontable. Le vrai risque n'est pas la fiabilité du contact, c'est **le mauvais branchement**. Une règle simple :

> **Sur une même carte, deux connecteurs ne doivent jamais avoir à la fois la même famille, le même nombre de broches et le même détrompage — sauf s'ils sont électriquement interchangeables.**

Corollaires pratiques :

| Liaison | Proposition | Calibre |
|---|---|---|
| Entrée batterie / bus 37 V, 25 A | **Bornier à vis pas 7,62 mm** (≈ 41 A) ou **insert M4 + cosse à œil** ou **XT60 embarqué** | ⚠️ Le 5,08 mm plafonne à ~17,5 A ✅ — insuffisant |
| Sorties vers ZS-X11H (37 V, 25 A) | Idem, **4 connecteurs identiques mais physiquement séparés et repérés AV-G / AV-D / AR-G / AR-D** | Les 4 sont interchangeables → même connecteur autorisé |
| 12 V / 5 V vers cartes et périphériques | **Molex Micro-Fit 3.0** (verrouillé, détrompé, 8,5 A/circuit ✅) | Nombre de broches différent par fonction |
| CAN + `/SAFE` + alim | **Micro-Fit 6 positions**, deux en parallèle par nœud | Un seul type sur tout le robot |
| Hall moteur (entrée) | JST PH 5 pts, **couleur A** | ⚠️ Doit être **impossible** à confondre avec la sortie variateur — l'ordre des broches diffère |
| Hall variateur (sortie) | JST PH 5 pts, **couleur B, à l'autre bout de la carte** | idem |
| VR / DIR / EL / STOP / NTC | Micro-Fit 6 pts | |
| Ventilateur PWM | Header 4 pts standard ventilateur | Détrompé par nature |
| WS2812B | Micro-Fit 3 pts, **une couleur unique** | |

⚠️ **La paire Hall entrée/sortie est le point le plus dangereux**, précisément parce que le README a raison de dire que « ce sont les deux fils que quelqu'un va se tromper ». Le remapping de couleurs en sérigraphie est une excellente idée ; il faut l'accompagner d'une impossibilité **physique** de croiser les deux connecteurs — pas seulement d'une indication.

## X.9.4 WS2812B — ce qu'il faut prévoir

Absents du dossier v1.1. Points techniques :

| Point | Détail |
|---|---|
| ⚠️ **Niveau logique** | WS2812B en 5 V : V_IH = 0,7 × V_DD = **3,5 V** ✅. Un GPIO ESP32 à 3,3 V est **sous le seuil**. Ça « marche souvent », jusqu'à ce que ça ne marche plus (température, longueur de câble, lot de LEDs). → **`74AHCT125`** (seuils TTL, alimenté en 5 V, accepte 3,3 V en entrée) — la solution standard |
| Résistance série | 330 à 470 Ω sur la ligne de données, au plus près du driver |
| Bulk | **1000 µF** à l'entrée d'alimentation de chaque bandeau |
| Courant | 60 mA/LED en blanc plein. 2 × 16 LEDs = **1,9 A** 📐 — à budgéter sur le rail 5 V, avec sa propre branche eFusée |
| Injection | Bandeau long → injecter le 5 V aux deux extrémités |
| Placement du shifter | Au plus près du bandeau, pas de la carte, si le câble est long |

## X.9.5 Capteur de température 3 broches

🔴 **Type non identifié.** `Power / GND / Data` correspond à au moins trois familles incompatibles :

| Famille | Interface | Ce qu'il faut sur la carte |
|---|---|---|
| **DS18B20** (1-Wire) | Numérique, adressable | Pull-up **4,7 k** vers 3,3 V |
| **LM35 / TMP36** | Analogique, 10 mV/°C | Filtre RC + entrée ADC, éventuellement diviseur |
| Module NTC type KY-013 | Analogique, diviseur intégré | Entrée ADC directe |

→ **Mesure à ajouter à la liste M** : identifier la référence sérigraphiée sur le capteur, ou mesurer la tension de sortie à température ambiante.

📐 **Solution générique en attendant** : dessiner l'entrée pour accepter les deux, avec deux empreintes de 0 Ω qui sélectionnent le mode.

```
   Data ──┬──[ 4,7 k ]── 3V3     ⚙ peupler pour 1-Wire
          ├──[ R_div ]── GND     ⚙ peupler pour analogique
          └──[ 1 k ]──┬── ADC / GPIO
                      └──[ 10 nF ]── GND
```

Coût : trois empreintes. La mesure ne bloque plus le PCB.

## X.9.6 Ventilateurs PWM

| Point | Détail |
|---|---|
| **Fréquence** | 25 kHz ✅ (spec 4 fils). En dessous de 20 kHz, sifflement audible |
| ⚠️ **Type de commande** | La spec attend une commande **drain ouvert**, l'entrée PWM du ventilateur ayant son propre pull-up interne ✅. Une sortie push-pull 3,3 V fonctionne souvent mais n'est pas conforme → **petit MOSFET en drain ouvert** |
| ⚠️ **Ne jamais hacher l'alimentation** d'un ventilateur 4 fils | Ça détruit le tachymètre et peut endommager le ventilateur. Le 12 V est **permanent**, seul le fil PWM est haché |
| ⚠️ **Entrée tachymètre** | Sortie à collecteur ouvert, **pull-up vers 3,3 V** — jamais vers 12 V. Et par prudence : **4,7 k en série + clamp `BAT54S` vers 3V3/GND**, au cas où un ventilateur exotique tire sa sortie vers 12 V. Un GPIO ESP32 à 12 V est mort |
| Démarrage | Impulsion à 100 % pendant 500 ms avant d'appliquer la consigne ✅ (§N.7) |
| Ventilateur 2 fils | MOSFET low-side + **diode de roue libre** obligatoire (charge inductive) |

## X.9.7 Maintenance et mécanique

| Point | Recommandation |
|---|---|
| **Trous de fixation** | ≥ 4, M3, avec **zone de cuivre dégagée** autour si les entretoises sont métalliques ; sinon entretoises nylon |
| **Rigidité sous couple** | Un bornier serré à 0,5 N·m fait fléchir un PCB fin. Fixation à moins de 20 mm de chaque bornier de puissance |
| **Reprise de câble** | ⚠️ §L.4 : « la traction ne doit jamais être reprise par les contacts ». → **trous de collier Rilsan à côté de chaque connecteur**, sur le PCB |
| **Vernis de tropicalisation** | Robot extérieur. Décider maintenant : si oui, masquer connecteurs, points de test et supports ESP32 — et le prévoir dès le placement |
| **Sérigraphie complète** | Nom de carte, révision, date, un **QR code vers la doc**, le brochage de chaque connecteur, les tensions maximales, les flèches d'orientation |
| **ID matériel lisible par le firmware** | 3 straps = 8 révisions, publiés dans le heartbeat CAN à côté du hash de protocole (§L.5) |
| **Interdiction de branchement à chaud** | Le §D.5 prévoit un contact pilote (broche courte) sur les connecteurs de puissance. À appliquer aussi **entre cartes** : broche courte `PRESENT` qui coupe l'`enable` avant les broches de puissance |

---

# X.10 Table d'injection de fautes — « et si quelqu'un… »

C'est le test à faire mentalement sur chaque schéma avant de router. Cochez chaque ligne.

| # | Faute | Conséquence sans protection | Protection retenue |
|---|---|---|---|
| F1 | Connecteur 12 V branché sur une sortie 37 V | Destruction de tous les régulateurs de la carte | **OVLO série** (X.4 ④) + détrompage |
| F2 | Polarité inversée sur une entrée | Destruction immédiate | **MOSFET P série** + connecteurs détrompés |
| F3 | ESP32 inséré à l'envers dans son support | Module détruit | **Broche bouchée asymétrique** + flèche `USB →` (X.8.5) |
| F4 | Fil de phase moteur (37 V PWM) sur une ligne Hall | Tampon + ESP32 + éventuellement le CAN détruits | **2,2 k série + TVS 5,6 V + condensateur 100 V + PPTC 100 mA** (X.6.3) |
| F5 | 37 V sur CANH | **Tous** les transceivers du bus détruits | **`TCAN1042` ±58 V** (X.7.4) |
| F6 | Masse d'un nœud débranchée côté alim | Retour par le fil de masse CAN de 0,5 mm² → fusion | **PPTC 500 mA sur la masse CAN de chaque branche** (X.7.4) |
| F7 | Fil `VR` coupé | Entrée flottante → **comportement indéfini du variateur** | **10 k vers GND au plus près du variateur** ✅ §F.5-L3 |
| F8 | Fil `/SAFE` coupé ou carte débranchée | Aucun freinage | **Pull-down local sur chaque carte**, sécurité positive (X.7.1) |
| F9 | ZS-X11H débranché, ESP32 lit encore les Hall | Lignes flottantes → **odométrie fausse** (pire qu'absente, §M.2-15) | **Pull-up 2,2 k sur `motor_interface`** (X.1.5) |
| F10 | Contacteur ouvert alors que le robot roule | **Surtension bus non bornée** | **Hacheur de freinage en aval du contacteur** (X.5.1) |
| F11 | Un ZS-X11H en court-circuit franc | Fusible de tête ET de branche fondent → robot mort | **Sélectivité 80 A / 25 A** (X.1.2) |
| F12 | LED de debug câblée sur GPIO12 | **Le module ESP32 ne démarre plus jamais** | Ne pas utiliser GPIO12, ou pull-down obligatoire (X.8.2) |
| F13 | Hall moteur branché sur le connecteur variateur | Ordre des broches différent → capteurs alimentés à l'envers | **Impossibilité physique** : connecteurs différents et éloignés (X.9.3) |
| F14 | Trois cavaliers de terminaison CAN actifs | Bus à 40 Ω → communication instable, intermittente | **Test de recette 60 Ω** sérigraphié (X.7.3) |
| F15 | Une carte débranchée sous tension | Arc sur les broches de puissance, appel de courant au rebranchement | **Broche pilote courte** + procédure (X.9.7) |
| F16 | Ventilateur dont le tachy est tiré à 12 V | GPIO ESP32 détruit | **4,7 k + `BAT54S`** (X.9.6) |
| F17 | Convertisseur DC/DC réglé à 5,5 V au lieu de 5,0 | Refoulement dans le port USB du PC | **ORing à diode idéale** (X.1.7) |
| F18 | Convertisseur « 8–40 V » installé sur le bus | Détruit à la première charge complète (42 V) | **60 V minimum**, écrit dans la BOM ✅ (README) |

---

# X.11 Quelle mesure bloque quelle carte

C'est ce qui détermine l'ordre de fabrication.

| Carte | Mesures bloquantes | Contournables par une option de peuplement ? |
|---|---|---|
| **`safety_bus_distribution`** | **Aucune** ✅ | — |
| **`io_peripherals`** | Type du capteur de température | ✅ Oui — double empreinte (X.9.5) |
| **`motor_interface`** | **M7** (niveaux Hall, présence des pull-ups) · **M4** (polarité `EL`/`STOP`) · M5 (cavalier J1) | M4 : ✅ oui — étage agnostique (X.6.1) · M5 : ✅ oui — option B du §H.3 retenue de toute façon · **M7 : 🔴 non, il faut mesurer** |
| **`safety_power`** | **D1** (identité du pack) · **M1** (seuil BMS) · **M2** (R_int → I_cc → pouvoir de coupure) · **M10** (C du bus → R de précharge) · **M12** (tension des condensateurs ZS-X11H) · **M3** (niveau logique BMS) | M3 : ✅ oui — isolateur numérique de toute façon obligatoire (§D.9) · M10 : 🟡 partiellement, R de précharge sur support enfichable · **M1, M2, M12 : 🔴 non** |

⚠️ **`safety_power` est la carte la plus bloquée et la plus dangereuse.** C'est aussi celle qu'il faut concevoir en dernier — ce qui est contre-intuitif, puisqu'elle alimente tout.

---

# X.12 Ordre de fabrication recommandé

```
   Étape 0   Trancher D1 à D7  ─ 1 soirée, aucun matériel
                 │
   Étape 1   Rédiger l'ICD (X.13) ─ un tableau par connecteur, versionné
                 │
   Étape 2   safety_bus_distribution ──► fabriquer, lot de 5
             Aucune mesure bloquante. Coût faible. Débloque tout le banc.
                 │
   Étape 3   Mesures M4, M5, M7 sur un moteur + un ZS-X11H, sur établi
             Alimentation de labo limitée à 3 A / 24 V ⚠️
                 │
   Étape 4   Maquette du tap Hall sur plaque à trous, UN canal
             Vérifier τ, seuils, immunité au PWM du variateur en marche
                 │
   Étape 5   motor_interface ──► fabriquer, lot de 5
                 │
   Étape 6   Mesures M1, M2, M10, M12 + identification du pack
             ⚠️ M1 : charge résistive, fusible 40 A en série, pack ≤ 50 % SOC,
                hors habitation, sable ou extincteur classe D à portée
                 │
   Étape 7   safety_power ──► fabriquer, lot de 5
                 │
   Étape 8   io_peripherals ──► fabriquer, lot de 5
```

**Pourquoi commencer par la distribution CAN** : c'est la carte la moins chère, la moins risquée, la seule que rien ne bloque — et elle permet de mettre trois ESP32 sur un bus CAN sur l'établi dès la semaine prochaine, donc de développer le firmware, le protocole et le `protocol.yaml` pendant que les mesures de puissance se font. C'est le meilleur usage du chemin critique.

⚠️ **Deux règles de lot** :

1. **Commander deux fois la BOM**, pas une. Cinq PCB nus sans composants ne sont pas cinq rechanges.
2. **Vérifier la disponibilité des composants dans la bibliothèque de l'assembleur AVANT de figer le schéma.** Choisir un `TCAN1042` que ton fabricant n'a pas en stock oblige à tout re-router. C'est l'étape que tout le monde saute et qui coûte le plus cher.

---

# X.13 Le document qui manque : l'ICD

Tu as déjà un `protocol.yaml` pour le CAN (§F.3) — l'idée est excellente. Il faut le même objet pour le **matériel**.

> **Un fichier unique décrivant chaque connecteur de chaque carte, broche par broche, versionné avec le schéma, et qui génère à la fois la documentation et les étiquettes de sérigraphie.**

```yaml
# hardware/icd.yaml
boards:
  motor_interface:
    rev: A
    connectors:
      J1:
        name: MOT_A_HALL
        type: JST-PH-5
        colour: blanc
        pins:
          1: {net: GND,      wire: noir}
          2: {net: HALL_A_A, wire: jaune}
          3: {net: HALL_A_B, wire: bleu}    # ⚠️ blanc côté variateur
          4: {net: HALL_A_C, wire: vert}    # ⚠️ orange côté variateur
          5: {net: V5_MOT,   wire: rouge}
```

Ce que ça produit automatiquement :

- Les tableaux de brochage du dossier d'architecture ;
- **Les étiquettes de sérigraphie** — donc plus de divergence entre la doc et la carte ;
- La liste de câbles du faisceau ;
- Les fiches de recette (X.6.4) ;
- Un **diff entre révisions** : ce qui a changé entre `rev A` et `rev B` devient une ligne de git, pas une découverte au moment du branchement.

C'est ce qui empêche la dérive silencieuse entre les trois cartes — le mode de défaillance le plus pénible de ce type de projet, exactement comme le note le §F.3 pour le protocole CAN.

---

# X.14 Composants à valider par datasheet avant schéma

Tu demandes explicitement que chaque composant soit validé. Voici la liste, avec le point précis à vérifier — c'est ce point-là qui compte, pas la datasheet dans son ensemble.

| Composant | Ce qu'il faut vérifier précisément |
|---|---|
| `TCAN1042` / `TCAN1051` | Présence de la broche `VIO`, tenue de bus ±58 V, courant de repos en mode veille |
| `SN74LVC3G17` ou `SN74LVC14A` | ⚠️ **Confirmer la spécification `Ioff`** (absence de clamp vers V_CC) — c'est ce qui garantit le passthrough hors tension |
| `LM66100` ou `TPS2116` | Plage d'entrée ≥ 5,5 V, `R_DS(on)`, temps de commutation, courant inverse au blocage |
| `TPS2595` / `TPS25940` | Plage d'entrée, précision de la limite de courant, comportement de la broche `FAULT`, temps de `dV/dt` |
| `INA228` | ⚠️ Mode commun **85 V** confirmé (l'INA226 plafonne à 36 V — insuffisant sur 42 V) |
| DC/DC 42→12 V | ⚠️ **Plage d'entrée 20–60 V minimum**, et vérifier si les 60 V sont « continus » ou « absolus » — la différence est la vie de la carte |
| DC/DC 12→5 V | Courant de sortie ≥ 5 A, ondulation, comportement en court-circuit |
| MOSFET hacheur de freinage | V_DS ≥ 100 V, `R_DS(on)`, **énergie d'avalanche**, SOA en régime répétitif |
| Résistance de freinage | ⚠️ **Courbe d'énergie impulsionnelle**, pas seulement la puissance continue |
| Résistance de précharge | Idem — tenue impulsionnelle ≥ 50 J (§D.5) |
| Contacteur DC | ⚠️ **Pouvoir de coupure en DC déclaré**, tenue vibratoire, courant de bobine et économiseur |
| TVS bus | Standoff > 42 V **et** tension d'écrêtage — vérifier la cohérence avec 63 V (§D.8) |
| Bornier de puissance | ⚠️ **Courant admissible réel** au pas choisi, couple de serrage, section de fil acceptée |
| Molex Micro-Fit 3.0 | Courant par circuit selon la section de fil et le nombre de circuits — il **décroît** avec le nombre de contacts |
| ESP32-WROOM-32 | Broches de strapping ✅, keepout d'antenne ✅, découplage ✅, courant de pointe |
| `MCP4728` | ⚠️ **Valeur EEPROM au démarrage** — la mettre à 0 (§H.3) : la sortie est alors nulle avant même le boot du firmware |
| `BNO085` | Séquencement `VDD` avant `VDDIO` ✅, 90 ms d'init ✅, ⚠️ **SPI uniquement, jamais I²C** ✅ |
| `74AHCT125` | Seuils d'entrée TTL confirmés à V_CC = 5 V (c'est tout l'intérêt) |
| `TPL5010` | Fenêtre de watchdog, comportement au démarrage, courant |
| ACS758 ou `ACS772` | ⚠️ Statut NRND ✅, sensibilité, dérive d'offset en température, `R_LOAD` ≥ 4,7 k et `C_LOAD` ≤ 10 nF ✅ |

---

# X.15 Ce que je n'ai pas tranché

Par cohérence avec la règle n°1 du dossier :

- **Je n'ai pas choisi entre l'option A et l'option B** (X.2). C'est une décision de projet, pas une décision technique — elle dépend de ton appétit pour un connecteur mezzanine supplémentaire.
- **Je n'ai pas figé la polarité de `EL`/`STOP`** : M4 n'est toujours pas faite. J'ai proposé un contournement au niveau du PCB, pas une réponse.
- **Je n'ai pas dimensionné la résistance de précharge** : elle dépend de M10, qui n'est pas faite.
- **Je n'ai pas donné de seuil de coupure du BMS** : il n'est publié nulle part, pour aucun des deux packs.
- **Je n'ai pas vérifié le brochage réel de ton module ESP32** : je n'ai que ta transcription. P1 à P4 (X.8.5) sont des mesures, pas des recherches.
- **Je n'ai pas identifié le capteur de température**, ni les références de tes ventilateurs, ni celles de tes bandeaux WS2812B. Ce sont des éléments physiques à lire sur les composants.
- **Je n'ai pas produit le README de `can_distribution`** que tu demandes : il doit être écrit après la décision D6 (CAN seul, ou CAN + `/SAFE`), sinon il décrira la mauvaise carte.

---

*À rattacher au dossier d'architecture comme section X, après `07-interface-operateur.md`.*
