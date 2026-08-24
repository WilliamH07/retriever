# Z. `motor_interface` — spécification, révision 2 moteurs

**Version** : 1.0 — 24 août 2026
**Remplace** : la carte 4 moteurs décrite dans le README existant (120 × 79 mm, 2 couches, routée)
**Exemplaires** : 2 en service (AVANT, ARRIÈRE) sur un lot de 5 — 3 rechanges universelles
**Statut** : bloquée par **M7** uniquement. M4 et M5 sont contournées par options de peuplement.

---

# Z.0 Ce qui change par rapport à la carte existante

| Point | Carte existante | Carte révisée | Raison |
|---|---|---|---|
| Nombre de moteurs | 4 | **2** | Le rôle AV/AR est déterminé par un strap → une seule référence PCB, 2 en service, 3 rechanges. Et §G.2 : la panne d'un nœud ne doit pas immobiliser 4 roues |
| Chemin de commande `VR`/`DIR`/`EL`/`STOP` | ❌ absent | ✅ **intégré** | Sans lui, la carte ne fait que la moitié du travail et les commandes restent en faisceau volant |
| Tampon Hall | `SN74LVC14AD` | **`SN74LVC3G17`** ×2 | ⚠️ TI a **retiré** la garantie `Ioff` du LVC14A — l'exigence dure de la carte n'était plus tenue |
| Entraxe socket ESP32 | 22,86 mm | **25,40 mm** | ⚠️ 22,86 est l'entraxe des cartes tierces 30 broches. Une DevKitC V4 ne rentrera jamais |
| Pull-up des lignes Hall | dans le ZS-X11H | **sur la carte, 2,2 kΩ** | Variateur débranché → lignes flottantes → odométrie fausse, pire qu'absente |
| Anti-backfeed USB | `SS14` + consigne « 5,0 V MAX » | **`LM66100`** diode idéale | Supprime une note de sérigraphie sur un chemin critique |
| Connecteurs Hall | JST PH 5 pos | **Micro-Fit 3.0 6 pos** | ⚠️ Le JST PH n'a qu'un maintien **par friction** — inacceptable en vibration |
| Freinage de sécurité | ❌ inexistant | ✅ **tirage 5 V + porte ET** | ⚠️ Voir Z.1 — la découverte la plus importante |
| Couches | 2 | **4** | Recommandation Espressif, et séparation puissance/signal |
| MOSFET de niveau | `2N7002` | **`DMG1012T`** | Le 2N7002 n'est pas garanti passant sous 3,3 V de grille |

---

# Z.1 ⚠️ Le freinage de sécurité — à lire avant tout le reste

## Z.1.1 Le problème

Le §H.7 du dossier décrit la chaîne d'arrêt d'urgence ainsi :

> `/SAFE` tirée à la masse → entrées `EL`/`STOP` des 4 ZS-X11H → **freinage électrique**

La recherche documentaire (banc de test publié à l'analyseur logique, plus deux listings vendeurs concordants) donne les polarités réelles 🟡 :

| Entrée | Polarité | Effet |
|---|---|---|
| `STOP` | **actif BAS** | *« disable the drive signals. This could be considered a **coast or free spin** mode. You can easily rotate the motor by hand »* |
| `EL` / `BRAKE` | **actif HAUT** | *« shorting it to 5V will apply the motor's brake. Leaving the pin floating or applying a logic 0 will **disconnect** the brake »* |

📐 **Conséquence** : tirer `/SAFE` à la masse met `STOP` à 0 → **roue libre**. Et `BRAKE` étant actif-haut, une ligne relâchée, une alimentation logique perdue ou un fil coupé **relâchent le frein**.

⚠️ **La chaîne de sécurité actuelle produit donc une roue libre, jamais un freinage** — exactement le comportement que le §A.1-2 identifie comme dangereux en pente pour un robot de 35 kg. Et le frein n'est **pas** fail-safe : il est relâché au repos et à la mise sous tension.

## Z.1.2 La parade

Une **résistance de tirage vers 5 V, placée sur le faisceau côté variateur**, et un MOSFET drain ouvert sur la carte qui la contredit tant que tout va bien :

```
   ── SUR LE FAISCEAU, AU PLUS PRÈS DU VARIATEUR ──
   5V_BRAKE ──[ 2,2 k ]──┬──► EL/BRAKE du ZS-X11H
                         │
   ── SUR motor_interface ──
                         └──► drain du DMG1012T
                                    │
   GPIO26 ──┐                       │
            ├─► 74LVC1G08 (ET) ─────┘ grille   + pull-down 100 k sur la grille
   /SAFE ───┘      alimentée en 3,3 V

   MOSFET passant → EL tiré bas  → frein RELÂCHÉ → marche normale
   MOSFET bloqué  → EL tiré haut → FREIN SERRÉ
```

📐 **Pas de relais.** Une résistance de tirage et un MOSFET donnent exactement la même logique fail-safe, sans pièce mécanique, sans courant de bobine permanent, sans diode de roue libre, et pour un dixième du prix. La porte ET `74LVC1G08` réalise la condition matérielle : **si elle perd son alimentation, sa sortie tombe**, et le pull-down de 100 kΩ garantit le blocage du MOSFET.

⚠️ **Le placement de la résistance de tirage est le point décisif.** Sur la carte, débrancher `J5`/`J6` laisserait l'entrée `EL` **flottante** — et flottant = frein désactivé. **Sur le faisceau, débrancher la carte serre le frein.**

**Ce qui fait tomber la bobine** — et donc serre le frein :

| Événement | Mécanisme |
|---|---|
| Champignon enfoncé | Le contact NF **en série avec l'unique pull-up** s'ouvre → `/SAFE` retombe → porte ET à zéro |
| Perte du 5 V logique | Plus d'alimentation de la porte ET |
| ESP32 planté | `TPL5010` → `/SAFE` bas → porte ET à zéro |
| ESP32 retiré du support | `GPIO26` flottant → pull-down 100 k → MOSFET bloqué |
| Câble `/SAFE` coupé | Pull-up déconnecté → `/SAFE` bas → porte ET à zéro |
| Perte du 3,3 V | La sortie de la porte ET tombe |
| **Carte débranchée** | La résistance de tirage est **côté variateur** → `EL` reste haut |

📐 Coût : une porte ET, un MOSFET, deux résistances par moteur — **moins de 1 € pour les deux**.

⚠️ **Ce montage ne couvre pas tout, et il faut le dire.** Deux réserves honnêtes :

| Mode | Statut |
|---|---|
| **Ouverture du contacteur à t ≈ 1 s** | 🟡 **Conditionnel.** `5V_BRAKE` est pris **en amont du contacteur** et survit, donc la *commande* de frein est maintenue. Mais la capacité *physique* à freiner dépend de l'énergie restante dans les condensateurs du variateur. **À valider par essai, pas à supposer.** |
| Perte de `5V_BRAKE` | ❌ Ne freine pas. Mitigé par la prise en amont du contacteur et un réservoir de 2200 µF. |

⚠️ **`5V_BRAKE` ne doit surtout pas venir du 78L05 du ZS-X11H.** Ce régulateur est alimenté par le bus : à l'ouverture du contacteur il s'arrête, la résistance de tirage se retrouve sur un rail mort, et **le frein se relâche exactement au moment où il faut freiner**. Le freinage ne doit jamais dépendre d'une tension que la séquence d'arrêt supprime.

## Z.1.3 Ce que cela change en aval

La temporisation RC de ~1 s avant l'ouverture du contacteur (§H.7) **garde tout son sens** — mais pour la première fois elle protège quelque chose de réel. Sans ce montage, elle laissait une seconde à un freinage qui n'existait pas.

⚠️ **Et il faut désormais la valider par la mesure** : robot sur cales puis au sol, lancer à pleine vitesse, déclencher l'arrêt d'urgence, chronométrer la décélération et mesurer la distance d'arrêt réelle, avec et sans ouverture du contacteur à t = 1 s. C'est le seul moyen de savoir si 1 s est le bon chiffre.

🔴 **M4 reste à faire sur tes propres cartes.** Les polarités ci-dessus sont un consensus fort, pas une spécification constructeur, et le ZS-X11H existe en au moins deux révisions matérielles (`STOP` est absent des cartes v1). Mais tu n'es plus dans le noir : tu as une hypothèse précise à confirmer, et l'étage agnostique en polarité (Z.4.3) fait que la mesure ne bloque plus le routage.

---

# Z.2 Architecture de la carte

```
  ┌───────────────────── ZONE MOTEUR (gauche) ──────────────────────┐
  │  J1 MOT_HALL_A ─┐                                                │
  │  J2 MOT_HALL_B ─┤  passthrough métallique, pistes droites        │
  └─────────────────┼────────────────────────────────────────────────┘
                    │   ┌── plan de masse de séparation ──┐
  ┌─────────────────┼───┼──────────────────────────────────┼─────────┐
  │  ZONE TAP       ▼   │                                  │         │
  │   6 × [ 2,2 k ∥ pull-up 2,2 k → +5V_MOT ]              │         │
  │   6 × [ 2,2 k série + 10 nF/100 V + BAT54S ]           │         │
  │   4 × SN74LVC3G17 (3,3 V, Ioff garanti, non inverseur) │         │
  └──────────────────────┬──────────────────────────────────┘         │
                         ▼  6 × GPIO                                  │
  ┌──────────────────────────────────── ZONE LOGIQUE ────────────────┐│
  │  ESP32-DevKitC V4, sockets Mill-Max 310, entraxe 25,40 mm        ││
  │  antenne en débord de carte, aucun cuivre dessous                ││
  │  TCAN1042HVDR + NUP2105L + ACT45B ─── CAN + /SAFE                ││
  │  MCP4728 + TLV9062 ─── VR ×2      ADS1115 ─── NTC ×2             ││
  │  4 × DMG1012T ─── DIR/STOP        74LVC1G08 ×2 ─── frein          ││
  └──────────────────────┬───────────────────────────────────────────┘│
  ┌─────────────────────▼────────── ZONE VARIATEUR (droite) ─────────┐│
  │  J3 CTRL_HALL_A   J4 CTRL_HALL_B   J5 CTRL_CMD_A   J6 CTRL_CMD_B ││
  └──────────────────────────────────────────────────────────────────┘│
```

**Trois zones, de gauche à droite** : moteur → traitement → variateur. Le faisceau ne se croise jamais, et les connecteurs Hall d'entrée sont physiquement à l'opposé de ceux de sortie.

---

# Z.3 Le tap Hall

## Z.3.1 Schéma par voie (×6)

```
  +5V_MOT ──[ 2,2 k ]──┐                    ① pull-up sur la carte
                       │
  Hall (moteur) ───────┼──────────────────► Hall (variateur)     ② passthrough
                       │
                       └──[ 2,2 k ]──┬──[ 10 nF / 100 V ]── GND  ③ RC + limitation
                                     │
                                     ├──[ BAT54S ]── 5V_MOT / GND ④ clamp
                                     │
                                     └──► SN74LVC3G17 ──► GPIO   ⑤ tampon Schmitt
```

## Z.3.2 Justification de chaque valeur

**① Le pull-up sur la carte, pas dans le variateur.** Le README d'origine s'appuie sur le pull-up interne du ZS-X11H. Deux problèmes :

- ⚠️ **Variateur débranché ou mort → le pull-up part avec lui.** L'entrée du tampon flotte, oscille, et l'ESP32 compte du bruit. Le §M.2 panne 15 le dit : une odométrie fausse est **pire** qu'une odométrie absente.
- La constante de temps devient **indéterminée et asymétrique**. Le README annonce « 10 µs, coupure 16 kHz » — vrai seulement sur le front **descendant**. Sur le front montant, τ = (R_pullup + R_série) × C, et avec un pull-up de 10 kΩ inconnu 🔴 on monte à **110 µs**.
  ⚠️ **L'erreur n'est pas celle qu'on croit** : une coupure plus basse rejette *mieux* le hachage, pas moins. Le vrai défaut est le **retard et la gigue de front** — 110 µs sur un intervalle de 1,15 ms, soit près de 10 %, et **uniquement sur le front montant**. C'est l'asymétrie qui biaise le comptage, pas le filtrage.

Avec 2,2 kΩ sur la carte, en parallèle du pull-up du variateur, les deux constantes deviennent bornées et connues :

| Front | Chemin de charge | τ | Coupure |
|---|---|---|---|
| Descendant | 2,2 kΩ série × 10 nF | **22 µs** | 7,2 kHz |
| Montant | (2,2 k + 2,2 k ∥ R_var) × 10 nF, R_var = 10 k → 4,0 kΩ | **40 µs** | 4,0 kHz |

📐 **Vérification** : à 578 tr/min (pleine vitesse à vide), l'intervalle entre transitions est de **1,15 ms**. Un τ de 40 µs représente **3,5 %** de cet intervalle — sans conséquence sur le comptage. Et à 4–7 kHz de coupure, le hachage à 16 kHz est atténué de **12 à 15 dB**, ce qui est le but recherché.

📐 **Charge vue par le capteur Hall** : 5 V / 2,2 kΩ = **2,3 mA**, contre **20 mA** que sait absorber un `SS41F` à collecteur ouvert ✅. Marge d'un facteur 8,7.

⚠️ **Vérification à faire** : certains clones « 41F » sont **push-pull**, pas à collecteur ouvert. Un test de pull-up au multimètre avant de compter dessus.

**③ 2,2 kΩ en série et non 1 kΩ.** C'est la protection contre le scénario le plus probable de défaut de câblage : **un fil de phase touche une ligne Hall**.

| Résistance série | Courant injecté par 37 V | Verdict |
|---|---|---|
| 1 kΩ (README) | **37 mA** | ⚠️ `IIK` du LVC est de −50 mA en absolu : à la limite, sans marge |
| **2,2 kΩ** | **17 mA** | ✅ Confortable |

Et le condensateur passe en **100 V** — coût identique en X7R 0603.

📐 **Vérification du filtre** : à 1,5 m/s (174 tr/min), une ligne Hall commute à **43,5 Hz**. Même à vide à pleine vitesse (578 tr/min mesurés), on est à **144,5 Hz** 🟡. La coupure à 22 kHz laisse **plus de deux décades** de marge au signal utile tout en rejetant le hachage. Confirmé.

**④ Le clamp `BAT54S`.** C'est la seule configuration **série** de la famille : le nœud commun (broche 3) réunit une cathode et une anode ✅.

```
   Broche 3 (K1 ; A2) ──► le signal
   Broche 1 (A1)      ──► GND      : conduit sous ≈ −0,24 V
   Broche 2 (K2)      ──► +5V_MOT  : conduit au-dessus de ≈ 5,24 V
```

⚠️ **Le clamp haut va sur le 5 V, pas sur le 3,3 V.** C'est une correction importante : la ligne Hall **repose à 5 V** en état haut. Un clamp vers 3,3 V conduirait **en régime établi**, injectant (5 − 3,54)/4,4 k = **0,33 mA par voie**, soit ~2 mA en permanence dans le rail 3,3 V pour six voies — et il annulerait le seul intérêt d'avoir choisi une entrée tolérante 5,5 V.

Avec le clamp sur `5V_MOT`, il ne conduit **jamais** en fonctionnement normal, et il écrête à ≈ 5,3 V sur un défaut de phase — dans la plage tolérée par le `SN74LVC3G17` (5,5 V) ✅.

❌ **Ni `BAT54A` (anode commune) ni `BAT54C` (cathode commune)** ne peuvent clamper des deux côtés avec un seul boîtier.

**⑤ Le tampon `SN74LVC3G17`.** Trois raisons, dans l'ordre d'importance :

| Raison | Détail ✅ |
|---|---|
| **`Ioff` garanti** | *« Ioff Supports Live Insertion, Partial-Power-Down Mode and Back Drive Protection »*. ⚠️ Le `SN74LVC14A` **ne le garantit plus** — TI a retiré la mention et écrit désormais que les entrées ont des diodes de clamp |
| **Non inverseur** | La table d'états Hall n'a plus à être complémentée en firmware |
| `VCC` 1,65 – **5,5 V** | Le LVC14A plafonne à 3,6 V. Marge si l'on décide un jour de référencer le tampon au rail moteur |

Hystérésis garantie à 3,3 V : `VT+` 1,50–1,87 V, `VT−` 0,84–1,14 V, **hystérésis 0,56–0,87 V** ✅.

⚠️ **Interdits de substitution, à écrire dans la BOM** :
- ❌ `74HC14` / `74HCT14` — clamp vers `VCC`, cassent l'exigence hors tension
- ❌ `SN74LVC14A` — `Ioff` non garanti
- 🟡 `74LV17A` Nexperia acceptable en repli, mais `IIK` limité à −20 mA et seuils donnés en typiques seulement

**Combien de boîtiers** : six voies de comptage = **2 × `SN74LVC3G17`**. ⚠️ Mais si l'on veut les six LEDs d'activité Hall sur des **sorties dédiées** (voir Z.6), il en faut six de plus, soit **4 boîtiers au total**.

📐 **Choix retenu : 4 boîtiers.** Le surcoût est de ~0,60 € et il évite d'ajouter la capacité d'une LED sur le chemin de comptage. Si le budget de place l'interdit, supprimer les LEDs Hall plutôt que de les mettre sur les sorties de comptage.

## Z.3.3 Trois voies et non deux

Le §H.4 du dossier propose de tapper 2 lignes sur 3 (décodage quadrature, 60 comptes/tour). Tapper les 3 est meilleur :

| | 2 lignes | **3 lignes** |
|---|---|---|
| Comptes par tour | 60 | **90** |
| Résolution linéaire (roue 165 mm) | 8,6 mm | **5,8 mm** |
| ⚠️ **Détection d'état Hall invalide** | ❌ **impossible** — toute combinaison de 2 bits est valide | ✅ `000` et `111` sont interdits, une transition non adjacente est détectable |
| Localisation d'un capteur mort | ❌ | ✅ quelle ligne reste figée |

⚠️ Le §M.2 panne 15 **exige** de détecter « une séquence d'états Hall invalide ». Avec deux lignes, c'est impossible. La spécification et le matériel étaient incohérents.

**Contrainte** : le décodage 6 états ne rentre pas dans une unité PCNT en mode quadrature. 📐 Décodage par interruption GPIO :

> 6 lignes × 145 Hz × **2 fronts par période** = **1 740 interruptions/s**

Sur un ESP32 à 240 MHz avec une ISR de 200 cycles, c'est **0,15 % de CPU**. Non contraignant.

⚠️ Ne pas confondre avec les « 867 transitions/s » mesurées sur la sortie `SC` — c'est le chiffre d'**un seul moteur**, agrégé sur ses trois capteurs.

Le matériel tappe les 3 voies ; le firmware choisit son décodage. C'est la philosophie « matériel générique » appliquée.

## Z.3.4 Le passthrough — contraintes de routage

| Règle | Raison |
|---|---|
| Pistes **droites, courtes, sans via si possible**, 0,5 mm minimum | 20 mA suffisent électriquement ; la largeur est pour la robustesse |
| Le tap est un **stub court**, jamais en série | Exigence dure |
| Le 10 nF est **en aval** du 2,2 kΩ série | ⚠️ Un condensateur en court-circuit **ne doit pas pouvoir tuer la ligne Hall**. À vérifier explicitement au schéma |
| **Plan de masse séparant** zone passthrough et zone tap/ESP32 | Bruit des variateurs |
| **PPTC `MF-MSMF010`** (100 mA / 60 V) sur chaque branche `+5V_MOT` | Un court +5 V ↔ phase ne remonte pas au variateur voisin |
| Aucun composant du tap ne doit, en défaillant, tirer une ligne vers un rail | Revue de schéma dédiée |

---

# Z.4 Le chemin de commande

## Z.4.1 Consigne `VR`

```
  ESP32 ──I²C──► MCP4728 ──► 0–5 V ──► TLV9062 ──► VR      + [ 10 k → GND ] ★
                 (VREF = VDD = 5 V)   (rail-to-rail)
```

| Point | Détail ✅ |
|---|---|
| **Sortie au démarrage** | *« When the device is first powered-up, it automatically loads the stored data in its EEPROM to the DAC input **and output** registers […] This event does not require an LDAC or UDAC bit condition. »* Défaut usine : code `0x000` → **0 V** |
| ⚠️ **Étape de recette** | **Programmer l'EEPROM à 0 en production.** La consigne est alors nulle dès la mise sous tension, **avant même le démarrage du firmware** |
| Pleine échelle 5,0 V | ⚠️ `VREF` = `VDD` avec `VDD` = 5 V. La référence **interne plafonne à 4,096 V** même en gain ×2 |
| `LDAC` | Câblé à **GND** en permanence — un seul DAC par carte, pas de reprogrammation d'adresse |
| Ampli | **`TLV9062`** : 20 mV des rails à 10 kΩ, **et 60 mV garantis à 2 kΩ** — le `MCP6002` ne spécifie rien sous 10 kΩ |
| Plein échelle réel | 📐 ≈ **4,98 V**. Sans importance : le variateur démarre à 0,07 V 🟡 et la boucle PI sur la vitesse mesurée compense |

★ ⚠️ **La résistance de 10 kΩ vers la masse sur `VR`, au plus près du connecteur variateur, n'est pas optionnelle.** Elle transforme une rupture de fil en consigne nulle plutôt qu'en entrée flottante à comportement indéfini (§F.5-L3).

## Z.4.2 ⚠️ Abandonner le `2N7002`

| Référence | `VGS(th)` ✅ | `RDS(on)` garanti à faible `VGS` ✅ |
|---|---|---|
| `2N7002` | 1,0 min / **2,5 V max** (2,75 V à −55 °C) | 🔴 **rien en dessous de VGS = 4,5 V** |
| **`DMG1012T`** | 0,5 min / **1,0 V max** | ✅ **0,5 Ω max à VGS = 2,5 V** · 0,7 Ω à 1,8 V |
| `BSS138` onsemi | 0,8 / 1,3 / 1,5 V max | 6,0 Ω max à 4,5 V |
| `DMN601K` | 1,0 / 1,6 / **2,5 V max** | ⚠️ **N'est PAS logic-level** malgré son nom |

⚠️ Avec 3,3 V de grille, l'overdrive garanti du `2N7002` dans le pire cas est de **0,8 V**, et rien n'est garanti à froid. Sur un robot extérieur qui démarre à 3 °C, ce n'est pas acceptable sur `DIR`, `EL` et `STOP`.

→ **`DMG1012T`** partout (20 V suffisent largement), **`BSS138` onsemi** si un boîtier SOT-23 est requis.

## Z.4.3 L'étage agnostique en polarité

M4 n'est pas faite, et deux révisions matérielles existent. Trois empreintes non peuplées lèvent le blocage :

```
  GPIO ──► DMG1012T drain ouvert ──┬── [ R_PU option → 5V_MOT ]  ⚙ JP1
                                    ├── [ R_PD option → GND ]     ⚙ JP2
                                    └──► sortie EL / STOP
  + empreinte d'inverseur SOT-23 en série, montée ou pontée par un 0 Ω  ⚙ JP3
```

📐 Coût : **~0,15 €**. Le PCB devient valable quelle que soit la réponse de M4 ; seuls le firmware et le peuplement restent à trancher. C'est la bonne assurance quand une mesure bloquante n'est pas encore faite.

---

# Z.5 Module ESP32 et affectation des GPIO

## Z.5.1 ⚠️ Entraxe 25,40 mm

| Cote | Valeur ✅ |
|---|---|
| Carte DevKitC V4 | 48,26 × 27,94 mm |
| **Entraxe des deux rangées** | **25,40 mm (1,000")** |
| Pas | 2,54 mm, 19 positions par rangée |

⚠️ Le README annonce **22,86 mm**. C'est l'entraxe des cartes **tierces 30 broches**, qu'Espressif ne documente pas du tout. Un PCB percé à 22,86 n'acceptera **jamais** une DevKitC V4.

✅ **Bonne nouvelle** : ton brochage transcrit correspond **exactement** aux rangées J2 et J3 officielles d'une DevKitC V4, lues dans l'autre sens. Le brochage est documenté et stable.

🔴 **Réserve** : le plan Espressif ne légende aucune cote — le 25,40 est établi par géométrie. **Mesure P1 au pied à coulisse avant de percer.** Deux minutes contre cinq PCB inutilisables.

## Z.5.2 Supports

**Mill-Max `310-43-119-41-001000` ou `311-…`**, tournés, contact BeCu 4 doigts ✅.

⚠️ **Ne pas prendre la série `315`** (bas profil) : elle n'accepte que **0,015–0,022"**, or **les broches d'une DevKitC sont carrées 0,025"**. Le module « rentre » mais force et abîme les contacts — erreur silencieuse.

Les séries tournées haute fiabilité sont cotées **1000 cycles** contre 100 pour l'estampé ✅ — c'est exactement ton exigence de remplacement sans dessoudage. Les 3 A par contact sont sans conséquence.

## Z.5.3 Insertion à l'envers

Deux rangées de 19 broches symétriques se laissent insérer retournées à 180°. Une rotation envoie la broche *k* de J2 sur la position *20 − k* de J3 : le **`5V` du module arrive sur la position du `GND`**, ce qui **court-circuite franchement le rail 5 V de la carte à la masse du module**. (Et la position normalement occupée par `IO23` reçoit `IO11`, une broche de flash.)

📐 **Rendre l'erreur impossible** : boucher une position à un endroit asymétrique (broche coupée côté module ou trou non métallisé côté carte), **plus** une grosse flèche sérigraphiée `USB →` et un liseré épais côté broche 1.

## Z.5.4 Affectation

| GPIO | Net | Note |
|---|---|---|
| 5 | `CAN_TX` | ⚠️ strapping, **doit être HAUT au boot** → pull-up 10 k vers 3V3 |
| 4 | `CAN_RX` | ⚠️ `STB` du TCAN1042 **doit être tiré à GND** — pull-up interne = standby silencieux |
| 21 / 22 | `I2C_SDA` / `SCL` | MCP4728 + ADS1115 |
| 34 / 35 / 36 | `HALL_A1` / `B1` / `C1` | entrées seules — **aucun pull externe requis**, la sortie du LVC3G17 est push-pull |
| 39 / 32 / 33 | `HALL_A2` / `B2` / `C2` | idem |
| 25 / 14 | `DIR_1` / `DIR_2` | `DMG1012T` drain ouvert |
| 27 / 23 | `STOP_1` / `STOP_2` | Séparés par roue : permet de désactiver **une** roue en mode `DEGRADED` (§M.4). ⚠️ Commande seule, **aucun rôle de sécurité** |
| 26 | `BRAKE_RELEASE` | **Commun aux deux roues** — entrée de la porte ET `74LVC1G08`. **Pull-down 100 k obligatoire** |
| 19 | `SAFE_N_IN` | Interruption. Ligne référencée **3,3 V**, pull-down faible **1 MΩ** local |
| 18 / 13 | `WD_DONE` / `WD_WAKE` | `TPL5010`. ⚠️ `DONE` est reconnu sur une transition **bas→haut**, ≥ 20 ms avant le prochain `WAKE` |
| 15 | `LED_STATUS` | LED anode→3V3, cathode→GPIO : pull-up interne = LED éteinte au boot, log préservé |
| — | `ROLE0` / `ROLE1` | ⚠️ **Déplacés sur l'ADS1115** (diviseur résistif, canal 2). Voir ci-dessous |

**Réserve** : `GPIO16` et `GPIO17` restent libres — conditionnellement, car ⚠️ **inutilisables sur les variantes PSRAM** (mesure P3). `GPIO0` et `GPIO2` sont récupérables sous contrainte de strapping.

📐 **Pourquoi le rôle passe sur l'ADS1115** : en v1.0 les straps `ROLE0`/`ROLE1` occupaient `GPIO16`/`GPIO17`, les deux broches justement inutilisables sur les modules à PSRAM — et **il ne restait alors aucune broche pour le watchdog `TPL5010`**, dont dépend pourtant toute la chaîne de freinage. Un diviseur résistif sur un canal ADS1115 donne quatre rôles sur **une** entrée, supprime la dépendance à P3, et libère les deux GPIO qui manquaient.

📐 Et `EL_1`/`EL_2` fusionnent en un `BRAKE_RELEASE` commun : le freinage est une fonction de sécurité **globale**, pas par roue. Cela libère la broche qui manquait.

**Interdits** :

| Broches | Raison ✅ |
|---|---|
| **GPIO6–11** | Flash SPI (`CLK`/`SD0`/`SD1`/`SD2`/`SD3`/`CMD`). ⚠️ La DevKitC les amène **quand même** sur ses connecteurs. **Aucune connexion**, sérigraphie `NC — FLASH` |
| **GPIO12** | ⚠️ MTDI. Un pull-up externe force la flash à 1,8 V et **le module ne démarre plus**. Verbatim esptool : *« may prevent flashing and/or booting […] causing the flash to brownout »*. **Non utilisé sur cette carte, par précaution** |
| GPIO0, GPIO2 | Bouton boot et download boot — laissées NC |
| GPIO1, GPIO3 | Console USB — **laissées libres pour flasher sans démonter le robot** |

📐 **Aucun ADC de l'ESP32 n'est utilisé.** Les 2 NTC passent par un `ADS1115` en I²C. Cela libère les broches ADC1 pour les Hall, évite la non-linéarité de l'ADC ESP32, et écarte l'erratum d'interruptions parasites sur GPIO36/39 en présence d'ADC actif ✅.

## Z.5.5 Contraintes RF et alimentation

| Point | Exigence Espressif ✅ |
|---|---|
| Alimentation | 3,0–3,6 V, **capable de fournir ≥ 500 mA** |
| `EN` | RC **10 kΩ + 1 µF** |
| Entrée d'alimentation | *« ESD protection diode and at least 10 µF capacitor at the main power entrance »* |
| Broches numériques | 0,1 µF au plus près |
| **Couches** | *« It is recommended to use a four-layer PCB design »* — L1 signal, **L2 masse pleine**, L3 alim, L4 signal |
| Masse | ≥ **9 vias** ; les pastilles de masse en contact plein avec le plan, pas par des pistes |
| **Placement** | *« place the module's on-board PCB antenna **outside the base board** »*. ⚠️ *« the module should not be placed in the center of the board »* |
| Autour de l'antenne | ⚠️ Nuance : Espressif demande *« sufficient ground copper and dense ground vias […] **near** the antenna »*. **Évider sous le corps de l'antenne, entourer de cuivre maillé** |

📐 **Placer le module en bord de carte, antenne en débord.** Bénéfice secondaire décisif : le connecteur USB devient accessible **sans démonter le robot** — c'est ton exigence de maintenance.

## Z.5.6 Anti-backfeed USB

Le `SS14` du README fonctionne mais dépend de trois choses fragiles : un `Vf` qui s'effondre à faible courant (0,25 V à 20 mA, pas 0,35), un VBUS hôte qui peut légalement descendre à **4,40 V**, et un réglage de convertisseur à 5,0 V exact — une consigne humaine sur un chemin critique.

→ **`LM66100`**, diode idéale ✅ : 1,5–5,5 V, **1,5 A**, `RDS(on)` **95 mΩ max**, `IQ` 150 nA, blocage inverse actif, SC-70-6.

📐 À 800 mA : chute **76 mV**, dissipation **61 mW** — quatre fois moins que le `SS14`. **La contrainte « 5,0 V MAX » du silkscreen disparaît complètement.**

⚠️ Pas de limitation de courant intégrée → à combiner avec le PPTC de la cellule d'entrée.

---

# Z.6 LEDs, points de test, mécanique

| LED | Couleur | Pilotage |
|---|---|---|
| Présence 5 V logique | vert | ⚠️ **directement sur le rail**, jamais par le MCU |
| Présence 3,3 V | vert | direct |
| Présence 5 V moteur ×2 | vert | direct sur le rail issu de chaque variateur |
| Défaut d'entrée (OVLO / inversion) | rouge | sortie du comparateur |
| Heartbeat MCU | bleu | 1 Hz — **l'absence de clignotement est l'information** |
| Activité CAN | jaune | sur `RXD` du transceiver |
| `/SAFE` actif | rouge | direct sur la ligne, via tampon |
| **Frein serré ×2** | rouge | sur la ligne `EL`, via tampon |
| Activité Hall ×6 | vert | voir ci-dessous |

⚠️ **Les LEDs d'activité Hall** : excellentes en recette (« tourne la roue à la main, trois LEDs clignotent en séquence »), inutiles en marche à 145 Hz. Trois précautions :

1. LED **haut rendement à 2 mA** avec 1 kΩ — pas 20 mA. 6 × 20 mA = 120 mA gaspillés en permanence.
2. Sur des sorties de tampon **dédiées**, pas celles qui vont aux GPIO — pour ne pas ajouter de capacité sur le chemin de comptage.
3. ⚠️ **Cavalier ou MOSFET d'inhibition globale.** Un robot autonome de nuit avec 15 LEDs allumées est visible de loin, et c'est de l'énergie perdue.

**Points de test** : `TP_5V`, `TP_3V3`, `TP_5V_MOT`, `TP_SAFE`, `TP_CANH`, `TP_CANL`, `TP_VR1`, `TP_VR2`, **6 × `TP_HALL` avant et après tampon**, et **un plot ressort de sonde à côté de chaque groupe**. Nommés en sérigraphie, jamais numérotés.

**Mécanique** : 4 trous M3, entretoises nylon, fixation à moins de 20 mm de chaque bornier, **trous de collier Rilsan à côté de chaque connecteur** (§L.4 : la traction ne doit jamais être reprise par les contacts).

---

# Z.7 Recette — ne jamais brancher une carte neuve sur les moteurs

1. **Continuité du passthrough.** `MOT_HALL_n` broche *k* ↔ `CTRL_HALL_n` broche *k* : 2 connecteurs × 5 broches utiles = **10 mesures**. Puis absence de continuité entre voisines.
2. **5 V logique seul**, rien d'autre connecté. LED verte allumée. Consommation de quelques mA — **au-dessus de 100 mA, arrêter et chercher le court**.
3. **Vérifier la diode idéale.** ⚠️ **À vide, la chute est de 0,5 mV** (5 mA × 95 mΩ) : on lira 5,00 V sur une carte parfaitement saine. Le test à vide ne prouve rien.
   → Charger à **800 mA** (résistance 6,2 Ω / 5 W) et mesurer : attendu **4,92 ± 0,03 V**. Puis vérifier le **blocage inverse** : appliquer 5,2 V en sortie, aucun courant ne doit remonter vers l'entrée. C'est cette seconde mesure qui protège le port USB.
4. **Monter l'ESP32.** Rien ne doit chauffer. Vérifier que le module démarre et que le log de boot sort sur l'USB — si le module ne démarre pas, suspecter en premier un pull-up parasite sur GPIO12.
5. **CAN en bouclage** : ponter `TX` et `RX` du TWAI, tester sans transceiver. Puis avec transceiver, `STB` à la masse vérifié.
6. **`/SAFE`** : au repos, ligne haute et `BRAKE_RELEASE` haut → les MOSFET conduisent, `EL` est bas, freins relâchés. Tirer `/SAFE` à la masse → **`EL` remonte à 5 V sur les deux voies, les deux LEDs « frein serré » s'allument**. C'est le test le plus important de la carte.
7. **Retirer l'ESP32 de son support, `/SAFE` toujours haut** → `EL` doit **remonter quand même**. C'est le pull-down de 100 kΩ sur la grille et la porte ET qui sont vérifiés ici.
   ⚠️ Puis **débrancher `J5`** : `EL` doit **rester haut**, tiré par la résistance du faisceau. Si `EL` retombe, la résistance de tirage est du mauvais côté du connecteur.
8. **Consigne `VR`** : mise sous tension, mesurer `TP_VR1` et `TP_VR2` **avant tout flashage**. Attendu **0 V** — c'est l'EEPROM du MCP4728. Toute autre valeur = EEPROM non programmée.
9. **Un seul moteur, robot éteint, roue tournée à la main.** Trois LEDs clignotent en séquence. Une LED morte est un capteur ou un fil, pas la carte.
10. **Avec le variateur**, moteur sur cales, alimentation de labo limitée à **3 A / 24 V** : retirer l'ESP32 de son support et vérifier que **le lien Hall moteur → variateur reste continu** et que le variateur conserve son retour de position. C'est l'exigence dure de la carte, et c'est le seul moment où on la teste vraiment.
    ⚠️ **Le moteur, lui, sera freiné** — c'est le comportement attendu : retirer l'ESP32 fait tomber la porte ET et serre le frein (étape 7). L'exigence dure porte sur le **passthrough Hall**, pas sur la marche du moteur. Pour observer le passthrough seul, court-circuiter temporairement `EL` à la masse.

---

# Z.8 Ce qui reste bloquant

| # | Mesure | Bloque | Contournable ? |
|---|---|---|---|
| **M7** | Niveau, impédance et type de sortie des lignes Hall et de `SC` | **Le tap entier** | 🔴 **Non.** Aucune source ne publie ces valeurs |
| — | Les Hall sont-ils réellement à collecteur ouvert ? | Le dimensionnement du pull-up | 🔴 Non — certains clones « 41F » sont push-pull |
| — | Code couleur Hall réel, fil par fil | La sérigraphie | 🔴 Non. ⚠️ Sources contradictoires, et **« orange » n'apparaît dans aucune source** — le tableau du README est à refaire au multimètre |
| **P1** | Entraxe du module au pied à coulisse | Tout le PCB | 🔴 Non |
| **P3** | Variante du module (PSRAM ?) | Réserve `GPIO16`/`GPIO17` uniquement | ✅ Oui — le rôle est passé sur l'ADS1115, plus aucune fonction n'en dépend |
| **M4** | Polarité `EL`/`STOP` | Firmware et peuplement | ✅ Oui — étage agnostique (Z.4.3) |
| **M5** | Cavalier `J1` | — | ✅ Sans objet : l'option DAC + ampli est retenue |

📐 **M7 est la seule vraie porte.** Elle se fait sur établi, avec un moteur, un ZS-X11H, une alimentation de labo limitée à 3 A / 24 V et un oscilloscope. Une soirée.

---

*À rattacher au dossier d'architecture comme section Z. Brochage complet dans `hardware/icd.yaml`, justification des composants dans `09-validation-composants.md`.*
