# Y. Validation des composants par datasheet

**Version** : 1.0 — 24 août 2026
**Objet** : vérification, source primaire à l'appui, de chaque composant retenu pour `safety_power`, `motor_interface` et `safety_bus_distribution`. Aucune valeur de ce document ne provient de mémoire : chaque ligne a été relue dans une datasheet constructeur récupérée, dont l'URL est citée.

**Statut de chaque fait** :

| Marqueur | Signification |
|---|---|
| ✅ **VÉRIFIÉ** | Lu dans une datasheet constructeur |
| 🟡 **CONSENSUS** | Plusieurs sources indépendantes non officielles concordantes |
| 🔴 **CONTRADICTOIRE / INCONNU** | Sources divergentes, ou aucune source |
| ⚠️ **CONSÉQUENCE** | Impact direct sur la conception |
| 📐 **DÉRIVÉ** | Calcul ou raisonnement de ma part à partir des faits ci-dessus |

---

# Y.0 Les cinq découvertes qui changent la conception

Avant le détail, les cinq points qui invalident ou corrigent ce qui était écrit dans les README ou le dossier v1.1.

| # | Découverte | Ce qui était écrit | Ce qu'il faut faire |
|---|---|---|---|
| **Y.0.1** | ⚠️ **Le `SN74LVC14A` ne garantit plus `Ioff`.** TI a **retiré** de la datasheet la mention « Ioff Supports Live Insertion, Partial-Power-Down Mode and Back Drive Protection », et écrit désormais : *« The inputs to this device have negative clamping diodes. The outputs to this device have both positive and negative clamping diodes. »* ✅ | Le README `motor_interface` fonde son **exigence dure** sur ce composant : « le passthrough survit à la perte du 3,3 V » | **Changer de référence.** Le `SN74LVC3G17` garantit explicitement *« Ioff Supports Live Insertion, Partial-Power-Down Mode and Back Drive Protection »* ✅. C'est le seul choix compatible avec l'exigence. |
| **Y.0.2** | ⚠️ **L'entraxe des rangées d'une DevKitC V4 38 broches est 25,40 mm (1,0"), pas 22,86 mm.** Le plan coté officiel Espressif donne carte 48,26 × 27,94 mm, rangées à 1,27 mm de chaque bord ✅ | Le README `motor_interface` annonce « 22.86 mm row pitch » | **Corriger le footprint.** 22,86 mm (0,9") est l'entraxe des cartes **tierces 30 broches**, qu'Espressif ne documente pas. Un PCB percé à 22,86 n'acceptera jamais la carte. |
| **Y.0.3** | ⚠️ **Sur le ZS-X11H, `BRAKE`/`EL` est actif-HAUT et `STOP` est actif-BAS = roue libre.** Mesure au banc publiée + 2 listings vendeurs concordants 🟡 | Le dossier §H.7 suppose que tirer `/SAFE` à la masse **freine** les moteurs | **Toute la chaîne d'arrêt d'urgence est à revoir.** Voir Y.4.3 — c'est la découverte la plus importante du document. |
| **Y.0.4** | ⚠️ **Le `2N7002` n'est pas garanti passant sous 3,3 V de grille.** `VGS(th)` jusqu'à **2,5 V max** à 25 °C et **2,75 V à −55 °C**, et **aucun `RDS(on)` spécifié en dessous de VGS = 4,5 V** ✅ | Le dossier §F.5-L3 et §H.3 retiennent le 2N7002 pour tous les étages collecteur ouvert | Remplacer par **`DMG1012T`** (`RDS(on)` ≤ 0,5 Ω garanti à VGS = 2,5 V ✅) ou **`BSS138` onsemi** (`VGS(th)` ≤ 1,5 V ✅). |
| **Y.0.5** | ⚠️ **Aucun eFuse `TPS2595` ne tient le bus 42 V** : plage 2,7–18 V, abs max 20 V ✅ | X.5.4 de la revue proposait le TPS2595 | Sur le bus : **`TPS2663x`** ou **`TPS2662x`** (4,5–60 V, abs max 67 V / 62 V ✅). Le TPS2595 reste valable sur les rails **12 V et 5 V** uniquement. |

---

# Y.1 Bus CAN

## Y.1.1 Transceiver — `TCAN1042HV` retenu

[Datasheet TI TCAN1042HV](https://www.ti.com/lit/ds/symlink/tcan1042hv.pdf)

**Signification des suffixes** ✅ — à lire attentivement, c'est la source d'erreur de commande la plus fréquente :

| Suffixe | Effet |
|---|---|
| `H` | Tenue de défaut bus **±70 V** au lieu de ±58 V |
| `V` | **Broche `VIO` présente** — c'est ce suffixe, et lui seul, qui donne l'adaptation de niveau |
| `G` | CAN FD 5 Mbit/s au lieu de 2 |
| `D` / `DR` / `DRB` | SOIC-8 / SOIC-8 bobine / VSON-8 3 × 3 mm |

→ **Référence retenue : `TCAN1042HVDR`** — ±70 V, `VIO`, SOIC-8, bobine.

| Paramètre | Valeur ✅ |
|---|---|
| `VCC` | **4,5 – 5,5 V** — ⚠️ c'est un transceiver **5 V**, pas 3,3 V |
| `VIO` | 3,0 – 5,5 V |
| Tenue de défaut `CANH`/`CANL` | **±70 V** (±58 V sans le `H`) |
| Mode commun récepteur | **±30 V** en mode normal ; ±12 V en standby ; ⚠️ **−2 à +7 V seulement si 3,0 V ≤ VIO ≤ 4,5 V** |
| Brochage SOIC-8 | 1 `TXD` · 2 `GND` · 3 `VCC` · 4 `RXD` · 5 `VIO` · 6 `CANL` · 7 `CANH` · 8 `STB` |
| Débit | 2 Mbit/s (5 sur suffixe `G`) — largement au-dessus des 500 kbit/s retenus |
| Courant standby | 0,5 µA typ / 5 µA max |
| ESD | ±16 kV HBM, ±15 kV IEC |

**Composants externes exigés par le schéma d'application** ✅ :

- `VCC` : **4,7 µF** bulk + **0,1 µF** au plus près de la broche
- `VIO` : **0,1 µF** au plus près
- ⚠️ **`STB` a un pull-up interne : la broche laissée flottante met le composant en STANDBY.** Elle **doit** être tirée explicitement à `GND` pour le mode normal. Une erreur classique qui donne un bus muet sans message d'erreur. La valeur du pull-up n'est pas publiée.
- Terminaison fendue : 2 × 60 Ω avec `CSPLIT` au point milieu vers `GND`

🔴 **Deux points non vérifiables** :
1. La valeur de `CSPLIT` n'apparaît pas dans le **texte** de la datasheet, seulement dans la figure 10-2. La convention du secteur est 4,7 nF — ce n'est pas une spécification TI. → **À lire visuellement sur la figure avant de figer.**
2. La révision courante du PDF semble avoir retiré les variantes non-`H`. → **Vérifier les références commandables actives sur ti.com avant BOM.**

## Y.1.2 Pourquoi abandonner le `SN65HVD230`

[Datasheet TI SN65HVD230](https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf)

| Paramètre | SN65HVD230 ✅ | TCAN1042HV ✅ |
|---|---|---|
| **Tenue de défaut bus (abs max)** | **−4 V à +16 V** | **±70 V** |
| Mode commun | −2 à +7 V | ±30 V |
| `VCC` | 3,0 – 3,6 V (3,3 V natif) | 4,5 – 5,5 V + `VIO` |
| CAN FD | non | oui |

⚠️ **C'est un facteur ~4,4 sur la tenue de défaut.** Sur un robot où le CAN, le 12 V et le 42 V cheminent dans le même faisceau, un contact accidentel entre une ligne de puissance et `CANH` détruit **simultanément les quatre nœuds** avec un HVD230. Avec le TCAN1042HV, ±70 V couvre largement les 42 V du bus.

Le HVD230 garde un avantage : son alimentation 3,3 V unique. Le TCAN1042HV impose un rail 5 V sur chaque carte — ce qui est de toute façon nécessaire ailleurs (Hall, DAC, ampli VR).

**Note utile pour la broche `Rs` du HVD230**, si tu en gardes en stock : `Rs` < 1,2 V = haute vitesse ; 10 k–100 k vers `GND` = contrôle de pente ; `Rs` > 0,75 × `VCC` = standby ✅.

## Y.1.3 `TCAN1051` — la différence à connaître

[Datasheet TI TCAN1051HV](https://www.ti.com/lit/ds/symlink/tcan1051hv.pdf)

Brochage identique sur 1–7. **La seule différence est la broche 8** : `S` (Silent mode) au lieu de `STB` (Standby). En silent mode le **récepteur reste actif** — on continue d'écouter le bus sans pouvoir émettre. Coût : **1,5 à 2,5 mA** au lieu de quelques µA ✅.

📐 **Pour ce robot, le `TCAN1042` suffit** : aucun nœud n'a besoin d'un mode « écoute seule », et le standby très basse consommation est plus utile sur le nœud SAFETY alimenté en amont du contacteur.

## Y.1.4 Protection ESD du bus

| Composant | `VRWM` | `VBR` | Clamp | Capacité | Boîtier |
|---|---|---|---|---|---|
| [**Nexperia PESD1CAN**](https://assets.nexperia.com/documents/data-sheet/PESD1CAN.pdf) | 24 V | 25,4–30,3 V @ 5 mA | 40 V @ 1 A · 70 V @ 3 A | **11–17 pF** | SOT-23 |
| [**onsemi NUP2105L**](https://www.onsemi.com/download/data-sheet/pdf/nup2105l-d.pdf) | 24 V | 26,2–32 V @ 1 mA | **33–40 V @ 5 A** · 37–44 V @ 8 A | 26 pF | SOT-23 |

📐 **Choix : `NUP2105L`.** À 500 kbit/s la capacité n'a aucune importance (le PESD1CAN ne prend l'avantage qu'en CAN FD rapide), et le NUP2105L **clampe nettement plus bas à fort courant** — 40 V à 5 A contre 70 V à 3 A. C'est la robustesse en surtension qui compte ici, pas la bande passante.

## Y.1.5 Self de mode commun

[Datasheet TDK ACT45B](https://www.tdk-electronics.tdk.com/inf/30/ds/act45b.pdf) — série explicitement destinée au CAN bus.

| Référence | L | Impédance @ 10 MHz | Courant nominal | DCR max | Boîtier |
|---|---|---|---|---|---|
| `ACT45B-510-2P-TL003` | 51 µH | 2 800 Ω typ | **200 mA** | **1,0 Ω** | EIA 1812 |
| `ACT45B-101-2P-TL003` | 100 µH | 5 800 Ω typ | 150 mA | 2,0 Ω | EIA 1812 |

→ **`ACT45B-510`** : 200 mA et 1 Ω de DCR, largement au-dessus du courant de signal CAN, et 2,8 kΩ à 10 MHz suffisent contre le hachage des variateurs. Disponible chez JLCPCB (C88056) ✅.

⚠️ Le Würth `744232222` est un composant data-line générique dont la datasheet ne tabule **aucune impédance de mode commun** (courbe seulement) 🔴. La série TDK est la seule des deux dont la documentation cite explicitement le CAN.

---

# Y.2 Tampon des signaux Hall — le point critique

## Y.2.1 ⚠️ `SN74LVC14A` : `Ioff` n'est plus garanti

[Datasheet TI SN74LVC14A](https://www.ti.com/lit/ds/symlink/sn74lvc14a.pdf)

| Paramètre | Valeur ✅ |
|---|---|
| `VCC` | **1,65 – 3,6 V** — ⚠️ pas 5,5 V |
| Tolérance d'entrée | `VI` abs max 6,5 V ; « Inputs accept voltages to 5,5 V » → **5,5 V tolérant à VCC = 3,3 V** |
| `VT+` / `VT−` à VCC = 3,0 V | 0,9–2,0 V / 0,6–1,5 V (hystérésis 0,3–1,2 V) |
| `IIK` abs max | −50 mA |
| `IOK` abs max | ±50 mA |
| Courant continu total par `VCC` ou `GND` | **±100 mA** |
| **`Ioff` / partial-power-down** | ❌ **NON.** Les mots `Ioff`, *partial-power-down* et *live insertion* **n'apparaissent plus** dans la révision courante, et l'historique de révision note explicitement leur **suppression**. §8.3.3 : *« The inputs to this device have negative clamping diodes. The outputs to this device have both positive and negative clamping diodes. »* |

⚠️ **Conséquence directe** : le README `motor_interface` écrit son exigence dure comme suit —

> « Si l'ESP32 est retiré, non alimenté ou défaillant, le lien moteur → contrôleur doit continuer de fonctionner. »

Cette garantie repose entièrement sur le comportement du tampon quand `VCC` = 0. **TI ne la garantit plus sur le LVC14A.** Ce n'est pas une subtilité de datasheet : c'est le fondement de la carte.

## Y.2.2 `SN74LVC3G17` — le remplaçant

[Datasheet TI SN74LVC3G17](https://www.ti.com/lit/ds/symlink/sn74lvc3g17.pdf)

| Paramètre | Valeur ✅ |
|---|---|
| Fonction | **Triple buffer Schmitt NON inverseur** (Y = A) |
| `VCC` | **1,65 – 5,5 V** |
| Tolérance d'entrée | `VI` abs max 6,5 V, 5,5 V tolérant à VCC = 3,3 V |
| `VT+` à 3,3 V | **1,50 – 1,87 V** |
| `VT−` à 3,3 V | **0,84 – 1,14 V** |
| **Hystérésis garantie** | **0,56 – 0,87 V** |
| Sortie | ±24 mA à 3,3 V |
| **`Ioff`** | ✅ **OUI, explicitement** : *« Ioff Supports Live Insertion, Partial-Power-Down Mode and Back Drive Protection »* |
| `IIK` abs max | −50 mA |
| Boîtiers | DCT (SSOP-8), DCU (VSSOP-8), YZP (DSBGA-8) |
| Brochage 8 br. | 1 `1A` · 2 `3Y` · 3 `2A` · 4 `GND` · 5 `2Y` · 6 `3A` · 7 `1Y` · 8 `VCC` |

**Trois avantages simultanés sur le LVC14A** :
1. `Ioff` garanti — l'exigence dure est tenue.
2. **Non inverseur** — la table d'états Hall n'a plus à être complémentée en firmware.
3. `VCC` jusqu'à 5,5 V — le tampon peut être alimenté en 5 V si on décide un jour de le référencer au rail moteur.

📐 **Pour 6 canaux (carte 2 moteurs, 3 Hall par moteur) : 2 × `SN74LVC3G17`.** Zéro canal gaspillé, exactement le compte.

**Repli** si rupture : `SN74LVC2G17` (dual) ou `SN74LVC1G17` (single), tous deux avec `Ioff` ✅. Noter que les seuils du 2G17 sont plus lâches (`VT+` jusqu'à 2,0 V) que ceux du 1G17 et du 3G17 — si l'hystérésis minimale compte, préférer **1G17 ou 3G17**.

⚠️ **Interdit de substitution, à écrire dans la BOM** :
- ❌ `74HC14` / `74HCT14` — clamp vers `VCC`, cassent l'exigence hors tension
- ❌ `SN74LVC14A` — `Ioff` non garanti (Y.2.1)
- 🟡 `74LV17A` Nexperia (hex, non inverseur) est un repli acceptable — `IOFF` documenté ✅, `VCC` 2,0–5,5 V, entrées tolérantes 7 V — **mais** `IIK` abs max n'est que **−20 mA** (contre −50 mA en LVC), ce qui réduit la marge de protection contre un contact de phase, et ses seuils à 3,3 V ne sont donnés qu'en **typiques**, pas en min/max garantis 🔴. [Datasheet 74LV17A](https://www.mouser.com/datasheet/2/916/74LV17A-1318318.pdf)

## Y.2.3 Clamp de protection : `BAT54S`

[Datasheet Nexperia BAT54S](https://assets.nexperia.com/documents/data-sheet/BAT54S.pdf)

**Configurations de la série** ✅ — c'est là que tout le monde se trompe :

| Variante | Config | Pin 1 | Pin 2 | Pin 3 |
|---|---|---|---|---|
| `BAT54A` | anode commune | K1 | K2 | **A1+A2** |
| `BAT54C` | cathode commune | A1 | A2 | **K1+K2** |
| **`BAT54S`** | **série** | A1 | K2 | **K1 + A2** |

📐 **Pour clamper un signal entre `GND` et `+3,3 V`, il faut le `BAT54S`** : le nœud commun (pin 3) réunit une cathode et une anode, ce qui est exactement ce qu'exige un clamp bidirectionnel.

```
   Pin 3 (K1 ; A2) ──► le SIGNAL
   Pin 1 (A1)      ──► GND     : conduit sous ≈ −0,24 V
   Pin 2 (K2)      ──► +3,3 V  : conduit au-dessus de ≈ 3,54 V
```

| Paramètre | Valeur ✅ |
|---|---|
| `VF` @ 1 mA | ≤ 320 mV |
| `VF` @ 10 mA | ≤ 400 mV |
| `IR` | **≤ 2 µA** @ 25 V |
| `VR` max | 30 V |
| `Cd` | ≤ 10 pF |
| Boîtier | SOT-23 |

⚠️ **Trois réserves** :
1. **Une résistance série en amont est obligatoire.** Sans elle, le courant de faute n'est limité que par la source.
2. ⚠️ **Clamper vers le rail sur lequel le signal repose, pas vers 3,3 V.** Une ligne Hall au repos est à **5 V** : un clamp vers 3,3 V conduirait **en régime établi** (0,33 mA par voie, ~2 mA pour six voies injectés dans le rail 3,3 V) et annulerait le seul intérêt d'avoir choisi une entrée tolérante 5,5 V. → clamp vers **`5V_MOT`**.
3. ⚠️ **2 µA de fuite** ne sont pas négligeables sur une entrée haute impédance. Sur une entrée ADC à source impédante, ça crée un offset — pas un problème sur une entrée logique.

**Alternative si l'on préfère une TVS à un clamp à diodes** : [`PESD5V0F1BL`](https://assets.nexperia.com/documents/data-sheet/PESD5V0F1BL.pdf) ✅ — bidirectionnelle, `VRWM` 5,5 V, `VBR` 6–10 V, `VCL` ≤ 15 V @ 2,5 A, **`Cd` 0,4 pF typ**, IEC ±10 kV, boîtier DFN1006 1,0 × 0,6 mm. ⚠️ Ne pas confondre avec la `PESD5V0S1BA/BB/BL`, dont la capacité est de **35–45 pF** — réservée aux entrées lentes.

---

# Y.3 Module ESP32 et son support

## Y.3.1 ✅ Ton brochage est bien celui d'une DevKitC V4 officielle

[Guide utilisateur ESP32-DevKitC V4](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html)

Brochage officiel des deux rangées de 19 broches :

- **J2** : `3V3, EN, VP(IO36), VN(IO39), IO34, IO35, IO32, IO33, IO25, IO26, IO27, IO14, IO12, GND, IO13, D2(IO9), D3(IO10), CMD(IO11), 5V`
- **J3** : `GND, IO23, IO22, TX(IO1), RX(IO3), IO21, GND, IO19, IO18, IO5, IO17, IO16, IO4, IO0, IO2, IO15, D1(IO8), D0(IO7), CLK(IO6)`

**Ta transcription correspond exactement à ces deux listes, lues dans l'autre sens.** Ton module est donc une **ESP32-DevKitC V4 38 broches** au brochage officiel Espressif — c'est une bonne nouvelle : le brochage est documenté et stable.

## Y.3.2 ⚠️ Entraxe : 25,40 mm, pas 22,86 mm

[Plan coté officiel ESP32-DevKitC V4](https://dl.espressif.com/dl/schematics/esp32_devkitc_v4_dimensions.pdf)

| Cote | Valeur ✅ |
|---|---|
| Carte | **48,26 × 27,94 mm** (1,900" × 1,100") |
| **Entraxe entre les deux rangées** | **25,40 mm = 1,000 pouce**, d'axe à axe |
| Pas dans une rangée | 2,54 mm |
| Largeur du module WROOM | 18,0 mm (c'est la cote `18.00` du plan — **pas** une cote de connecteur) |

⚠️ **Le README `motor_interface` annonce 22,86 mm.** C'est l'entraxe des cartes **tierces 30 broches**, qu'**Espressif ne documente pas du tout**. Un PCB percé à 22,86 mm n'acceptera jamais une DevKitC V4.

🔴 **Réserve honnête** : le plan Espressif ne **légende** aucune cote. Le 25,40 est établi par géométrie (27,94 de large, rangées à 1,27 mm de chaque bord) et par élimination (18,00 = largeur module). → **Mesure P1 au pied à coulisse sur ton exemplaire, avant de percer.** C'est deux minutes et ça évite cinq PCB inutilisables.

## Y.3.3 Broches de strapping — la liste officielle

[Datasheet ESP32 Series v5.3, §3](https://documentation.espressif.com/esp32_datasheet_en.pdf) · [esptool Boot Mode Selection](https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html)

**Exactement cinq broches** ✅ :

| Broche | Pull interne par défaut ✅ | Rôle | Conséquence d'un mauvais niveau |
|---|---|---|---|
| `GPIO0` | **pull-up** | 1 = boot flash, 0 = bootloader série | Un pull-down externe force le mode téléchargement |
| `GPIO2` | **pull-down** | doit être 0 ou flottant pour entrer en download boot | Sans effet en boot normal |
| **`GPIO12` (MTDI)** | **pull-down** | **Sélection de `VDD_SDIO`** : 0 = 3,3 V, 1 = **1,8 V** | ⚠️ **BOOT IMPOSSIBLE.** Verbatim esptool : *« If driven High, flash voltage (VDD_SDIO) is 1.8 V not default 3.3 V […] may prevent flashing and/or booting if 3.3 V flash is used and this pin is pulled high, causing the flash to brownout. »* |
| `GPIO15` (MTDO) | **pull-up** | 1 = log de boot ROM sur U0TXD, 0 = silencieux | Non bloquant |
| `GPIO5` | **pull-up** | avec MTDO, timing SDIO slave | 🔴 Aucune conséquence documentée hors mode SDIO slave |

Timing : `tSU` min 0 ms, **`tH` min 1 ms** après `CHIP_PU` haut avant que les broches redeviennent des GPIO normaux ✅.

⚠️ **La règle absolue pour ton PCB : aucun pull-up externe sur `GPIO12`, jamais.** Le dossier §G.3 y assigne le PWM du ventilateur 1 sur le nœud SAFETY. Une LED de debug vers 3,3 V ou une résistance de tirage sur cette ligne rend le module **définitivement non démarrable**, avec un symptôme muet et déroutant. → **Recommandation : ne pas utiliser `GPIO12` du tout.** Il reste assez de broches.

Espressif ajoute deux règles de schéma ✅ :
- *« It is recommended to place a pull-up resistor at the GPIO0 pin »*
- *« Do not add high-value capacitors at GPIO0, or the chip may enter download mode »*
- Un bouton de boot exige un **pull-down fort, 10 kΩ vers GND** (le pull-up interne fait 45 kΩ)

## Y.3.4 Broches interdites et entrées seules

| Contrainte | Détail ✅ |
|---|---|
| **Flash SPI intégrée** | `GPIO6`=CLK, `GPIO7`=SD0, `GPIO8`=SD1, `GPIO9`=SD2, `GPIO10`=SD3, `GPIO11`=CMD. Sur le module WROOM elles **ne sont pas sorties** (broches NC), mais ⚠️ **la DevKitC les amène quand même sur ses connecteurs** sous les noms `D0`–`D3`, `CMD`, `CLK`. → **Aucune connexion sur ton PCB**, sérigraphie `NC — FLASH` |
| **PSRAM** | Sur les variantes `ESP32-D0WDR2-V3`, **`IO16` est reliée à la PSRAM et inutilisable** ✅. ESP-IDF ajoute `GPIO16-17` à la liste noire pour ces variantes. ⚠️ Le dossier §G.3 utilise `GPIO16/17` pour l'UART BMS et les straps de rôle → **vérifier la variante exacte de tes modules** |
| **Entrées seules, sans pull interne** | `GPIO34` (VDET_1), `GPIO35` (VDET_2), `GPIO36` (SENSOR_VP), `GPIO39` (SENSOR_VN) ✅. Pull-up ou pull-down **externe obligatoire** — 📐 **sauf** si la broche est pilotée en **push-pull** par un composant présent en permanence, ce qui est le cas des sorties du `SN74LVC3G17` |
| `GPIO37` / `GPIO38` | **Non sorties** sur WROOM-32 ✅ (absentes de la table de brochage du module) |
| **ADC2 / Wi-Fi** | *« ADC2 pins cannot be used when Wi-Fi is used »* ✅ — sans objet ici, Wi-Fi désactivé |
| Interruptions | *« do not use the interrupt of GPIO36 and GPIO39 when using ADC or Wi-Fi and Bluetooth with sleep mode enabled »* ✅ |

⚠️ **Point de conception** : une entrée `/SAFE` sur `GPIO34-39` sans pull externe **flotte**. Sur le signal le plus critique du robot, c'est un défaut de sécurité, pas un désagrément.

## Y.3.5 Alimentation, découplage, antenne

[Datasheet ESP32-WROOM-32E](https://documentation.espressif.com/esp32-wroom-32e_esp32-wroom-32ue_datasheet_en.pdf) · [Hardware Design Guidelines](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/pcb-layout-design.html)

| Point | Exigence Espressif ✅ |
|---|---|
| Tension | 3,0 – 3,6 V, typ 3,3 V |
| **Courant de l'alimentation** | `IVDD` **min 0,5 A** — c'est une **exigence sur l'alimentation**, pas la consommation du module. Verbatim : *« the recommended power supply voltage is 3.3 V and the output current is no less than 500 mA »* |
| **RC sur `EN`** | *« R = 10 kΩ and C = 1 µF »*, à ajuster selon la séquence de montée |
| Entrée d'alimentation | *« add an ESD protection diode and at least 10 µF capacitor at the main power entrance »* |
| Broches d'alimentation numériques | *« add a 0.1 µF capacitor close to the digital power supply pins »* |
| **Nombre de couches** | *« It is recommended to use a four-layer PCB design »* : L1 signal, **L2 plan de masse intégral**, L3 alimentation, L4 quelques signaux |
| Plan de masse | *« at least nine ground vias »* sous le pavé de masse ; *« ground pads […] should make full contact with the ground copper pour rather than being connected via traces »* |
| **Placement du module** | *« It is suggested to place the module's on-board PCB antenna outside the base board, and the feed point of the antenna close to the edge »*. Sinon : *« cut off the base board on both sides of the antenna and below it »*. ⚠️ *« the module should not be placed in the center of the board »* |
| Autour de l'antenne | ⚠️ Nuance importante : Espressif ne dit **pas** « pas de cuivre près de l'antenne ». Il dit l'inverse pour la zone environnante — *« sufficient ground copper and dense ground vias should be placed on the base board near the antenna »*. **La règle est : évider sous le corps de l'antenne (ou la faire déborder), et entourer ce vide de cuivre de masse maillé de vias.** |
| Pistes RF | *« Do not route any traces under the RF trace »* ; *« The RF trace should be routed on the outer layer without vias »* |

🔴 **Non publié** : aucune cote numérique de zone d'exclusion d'antenne sur carte porteuse — Espressif ne la donne que graphiquement (fig. 23–24). Le seul chiffre publié est **15 mm de dégagement dans toutes les directions** à l'intérieur du boîtier du produit fini.

🔴 **Non publié** : aucune valeur de découplage pour la broche `3V3` **du module**. Le schéma « Peripheral Schematics » montre un 10 µF. 📐 Appliquer 10 µF + 0,1 µF au support est une extrapolation raisonnable du guide au niveau puce, pas une règle citée.

## Y.3.6 TWAI (CAN) sur ESP32

[ESP-IDF TWAI](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/twai.html)

- ✅ Transceiver externe **obligatoire** : *« The ESP32 does not integrate an internal TWAI transceiver. »*
- ✅ Quatre lignes : `TX`, `RX`, `BUS_OFF`, `CLKOUT`, routables par la GPIO Matrix.
- 🔴 **Espressif ne publie aucune liste de GPIO autorisés ou interdits** pour TWAI — vérifié sur les versions *latest*, v5.3.1 et v4.4.8. La seule contrainte réelle (`TX` doit être une broche capable de sortie, donc **pas `GPIO34-39`**) vient du chapitre GPIO, pas du chapitre TWAI. À traiter comme une déduction, pas comme une citation.
- ✅ `TX` et `RX` peuvent être pontés pour un test de bouclage mono-nœud sans transceiver — pratique en recette.

---

# Y.4 Chaîne moteur

## Y.4.1 `MCP4728` — DAC de consigne `VR`

[Datasheet Microchip DS22187E](https://ww1.microchip.com/downloads/en/devicedoc/22187e.pdf)

| Paramètre | Valeur ✅ |
|---|---|
| Alimentation | 2,7 – 5,5 V |
| Sortie | **rail-to-rail**, 0 à `VDD` si `VREF` = `VDD` |
| Référence interne | **2,048 V**, sélectionnable **par canal**. Gain ×1 ou ×2 → 0–2,048 V ou 0–4,096 V. ⚠️ **Le gain ×2 n'est pas applicable si `VDD` est la référence** |
| Temps d'établissement | 6 µs typ pleine échelle ; slew 0,55 V/µs. ⚠️ Changement de référence : 26 µs (ext→int), 44 µs (int→ext), **non inclus** |
| Courant de sortie | **24 mA max** ; `ISC` 15 mA typ / 24 mA max ; durée de court-circuit « infinie » ; `RL` > 5 kΩ recommandé, `CL` jusqu'à 1000 pF sans oscillation |
| **Adressage I²C** | Code `1100` + 3 bits A2/A1/A0 → **8 adresses, donc 8 composants max par bus**. ⚠️ Les bits d'adresse sont **en EEPROM**, il n'y a **pas de broches d'adresse**. Défaut usine `000` |
| ⚠️ **Quirk `LDAC`** | La commande « Write I²C Address Bits » n'est valide que si `LDAC` fait une transition **haut→bas pendant le temps bas du 8ᵉ coup d'horloge du 2ᵉ octet et reste bas jusqu'à la fin du 3ᵉ**. Max 400 kHz. → `LDAC` sert à **sélectionner physiquement le composant** quand plusieurs partagent une adresse |
| **Sortie au démarrage** | ✅ **CONFIRMÉ** : *« When the device is first powered-up, it automatically loads the stored data in its EEPROM to the DAC input and output registers, and provides analog outputs with the saved settings immediately. This event does not require an LDAC or UDAC bit condition. »* Défaut usine : code 0x000 → **0 V sur les 4 canaux** |
| Brochage MSOP-10 | 1 `VDD` · 2 `SCL` · 3 `SDA` · 4 `LDAC` · 5 `RDY/BSY` (drain ouvert, pull-up ~100 kΩ requis) · 6-9 `VOUT A-D` · 10 `VSS` |

⚠️ **C'est exactement le comportement voulu** : à la mise sous tension, avant même que le firmware ne démarre, la consigne `VR` des deux roues est à **0 V**. Le §H.3 du dossier avait raison de le retenir pour cette raison. **À condition de programmer l'EEPROM à 0 en production** — c'est une étape de fabrication à écrire dans la procédure de recette.

📐 **Pour atteindre 5,0 V pleine échelle** : utiliser `VREF` = `VDD` avec `VDD` = 5 V, **pas** la référence interne (qui plafonne à 4,096 V même en gain ×2).

## Y.4.2 Amplificateur de mise à l'échelle

| Paramètre | [`MCP6002`](https://ww1.microchip.com/downloads/en/DeviceDoc/MCP6001-1R-1U-2-4-1-MHz-Low-Power-Op-Amp-DS20001733L.pdf) ✅ | [**`TLV9062`**](https://www.ti.com/lit/ds/symlink/tlv9062.pdf) ✅ |
|---|---|---|
| Alimentation | 1,8 – 6,0 V | 1,8 – 5,5 V |
| Entrée RRI | `VSS`−0,3 à `VDD`+0,3 | (V−)−0,1 à (V+)+0,1 |
| **Sortie, marge aux rails @ RL = 10 kΩ** | **25 mV max** @ 5,5 V | **20 mV max** @ 5,5 V |
| **Sortie @ RL = 2 kΩ** | 🔴 **Non spécifié** (courbe seulement) | ✅ **60 mV max** |
| GBW | 1,0 MHz | 10 MHz |
| `VOS` | — | ±0,3 mV typ / ±1,6 mV max |
| `ISC` | ±23 mA @ 5,5 V | ±50 mA |

📐 **Retenu : `TLV9062`.** Il gagne sur les deux plans qui comptent ici — marge aux rails plus serrée, **et** une limite garantie sous 2 kΩ que le MCP6002 ne spécifie pas du tout.

⚠️ **Fait à accepter** : aucun ampli rail-to-rail alimenté en 5,00 V ne sortira 5,000 V. Avec le TLV9062, la sortie plafonne à ≈ **4,98 V** (10 kΩ). 📐 Sans importance ici : le ZS-X11H démarre à 0,07 V et le haut de l'échelle n'est pas critique — la boucle PI sur la vitesse mesurée compense. Si le 5,000 V pleine échelle devenait nécessaire, alimenter l'ampli en 5,5 V.

## Y.4.3 ⚠️⚠️ `ZS-X11H` — la chaîne d'arrêt d'urgence est à revoir

Aucun datasheet officiel n'existe. La source la plus solide est un **banc de test publié avec analyseur logique** : [MAD-EE, *Easy Inexpensive Hoverboard Motor Driver*](https://mad-ee.com/easy-inexpensive-hoverboard-motor-controller/). Les autres sont des listings vendeurs ou des retours de makers.

⚠️ **Source à ignorer complètement** : les pages `docs.cirkitdesigner.com` sur le ZS-X11H sont du contenu **généré automatiquement** — leur page « ZS-X11H v1 » publie un brochage de **carte Arduino** (VIN, D0–D7, SCL/SDA, 16 MHz). Aucune valeur n'en est exploitable.

### Polarités des entrées

| Entrée | Verdict | Sources |
|---|---|---|
| **`DIR` / `ZF`** | 🟡 **Actif BAS** | Mesure au banc MAD-EE (*« shorting it to ground […] changes the motor direction »*) + LaskaKit (*« log. 0 = active »*). ElectroPeak contredit (*« high-level signal »*) — listing probablement erroné |
| **`EL` / `BRAKE`** | 🟡 **Actif HAUT** — 3 sources contre 1 | MAD-EE mesure : *« active high — shorting it to 5V or applying a logic 1 will apply the motor's brake. Leaving the pin floating or applying a logic 0 will disconnect the brake. »* + LaskaKit (*« log. 1 = active »*) + ElectroPeak. RoboFoundry dit actif-bas — rapport isolé |
| **`STOP`** | 🟡 **Actif BAS = roue libre (coast), PAS un freinage** | MAD-EE : *« shorting it to ground […] will disable the drive signals. This could be considered a coast or free spin mode. […] You can easily rotate the motor by hand with the stop switch engaged. »* + LaskaKit. ⚠️ **`STOP` est une nouveauté de la révision récente — les cartes v1 ne l'ont pas** |

### ⚠️ La conséquence, et elle est grave

Le dossier §H.7 décrit la séquence d'arrêt d'urgence ainsi :

> « `/SAFE` tirée à la masse DIRECTEMENT (par le contact) → entrées `EL`/`STOP` des 4 ZS-X11H → **freinage électrique** »

📐 **Avec les polarités ci-dessus, cette séquence ne freine pas.** Tirer `/SAFE` à la masse met `STOP` à 0 → **les drives sont coupés → roue libre**. Et `BRAKE` étant actif-**haut**, une ligne relâchée ou une alimentation logique perdue met le frein à 0 → **frein désactivé**.

Autrement dit : **la chaîne de sécurité actuelle produit une roue libre, pas un freinage** — exactement le comportement que le §A.1-2 identifie comme dangereux en pente pour un robot de 35 kg.

⚠️ **Et le frein n'est pas fail-safe** : `BRAKE` actif-haut signifie frein **relâché** au repos, à la mise sous tension, et si le fil casse.

### 📐 Ce qu'il faut concevoir à la place

Un **relais normalement fermé qui force `BRAKE` à 5 V quand il n'est plus alimenté** :

```
   5V_MOT ──┬── contact NF du relais ──► BRAKE du ZS-X11H
            │                              (+ pull-down 10 k vers GND)
   bobine ──┴── alimentée par : /SAFE relâché ET ESP32-MOTION vivant
                → bobine excitée = contact ouvert = frein relâché = marche normale
                → perte de /SAFE, du 5 V, de l'ESP32, ou fil coupé
                  = bobine retombe = contact fermé = BRAKE à 5 V = FREIN SERRÉ
```

C'est le seul montage où **toute** défaillance conduit au freinage. Il coûte un relais de signal et une diode de roue libre par moteur.

📐 **Et il faut alors revoir la temporisation du contacteur** : le §H.7 laisse 1 s au freinage avant l'ouverture. Avec un frein réellement fail-safe, cette temporisation garde son sens ; sans lui, elle ne servait à rien.

🔴 **M4 reste à faire sur tes propres cartes** — les polarités ci-dessus sont un consensus fort, pas une spécification constructeur, et la carte existe en au moins deux révisions. Mais tu n'es plus dans le noir : tu as maintenant une hypothèse précise à confirmer, et l'étage de sortie agnostique en polarité (X.6.1 de la revue) reste la bonne assurance.

### Autres caractéristiques

| Point | Valeur | Statut |
|---|---|---|
| Entrée `VR` | 0–5 V, **démarrage mesuré à ≈ 0,07 V**, pas de bande morte notable | 🟡 mesure MAD-EE |
| ⚠️ Potentiomètre embarqué | **Doit être tourné à fond dans le sens anti-horaire** pour ne pas interférer avec la consigne externe | 🟡 mesure MAD-EE |
| Sortie `SC` | **90 transitions par tour** (15 paires × 6 états). Mesure à vide 36 V : 1,1536 ms entre transitions → **577,9 tr/min** | 🟡 mesure à l'analyseur logique |
| **Niveau électrique de `SC`** | 🔴 **AUCUNE SOURCE.** Ni amplitude, ni impédance, ni type de sortie | **M7 reste obligatoire** |
| Tension d'entrée | 6–60 V (MAD-EE + LaskaKit) **contre** 9–60 V (ElectroPeak) | 🔴 contradictoire → 📐 retenir **9 V** comme borne basse prudente |
| Courant | 16 A nominal / 20 A crête, 400 W. ⚠️ **Aucun fusible embarqué** | 🟡 2 vendeurs concordants |
| **Sortie 5 V** | **50 mA max**, régulateur **78L05** identifié au teardown | 🔴 source vendeur unique — 📐 plausible pour un 78L05 (100 mA typique déclassé) |
| ⚠️ Cavalier `J1` (mode PWM) | **MAD-EE : le ponter.** **ElectroPeak : le retirer.** | 🔴 **CONTRADICTION DIRECTE** — M5 reste obligatoire |
| Pastilles auxiliaires `G/S/V/P/G` | `S` = `SC`, `V` = simple sortie 5 V (pas un tachy), `P` = `PWM`. **Non peuplées, pas standard 2,54 mm** (~1 à 1,5 mm) | 🟡 |
| PWM | 50 Hz – 20 kHz, 2,5 – 5 V d'amplitude. Un maker pilote à 490 Hz en boucle PID fermée avec succès | 🟡 |

⚠️ **Confirmation d'un point du dossier** : un utilisateur alimentant un ESP32 depuis le 5 V du ZS-X11H rapporte *« lockups and erratic behavior »*. Le §F.5-L3 avait raison — **ne jamais alimenter un ESP32 depuis ce rail**.

## Y.4.4 Capteurs Hall des moteurs-roues

| Point | Valeur | Statut |
|---|---|---|
| IC typique | Marquage **« 41F »** (SS41F / SS41 / OH41 et clones) sur un petit PCB en arc dans le stator | 🟡 sources commerciales concordantes |
| ⚠️ Marquage réel | Un teardown publié admet : *« Unfortunately I couldn't see any markings on the sensor »* — remplacement par un **Honeywell SS41** réussi | 🔴 IC d'origine inconnu |
| Alimentation `SS41F` | **4,5 – 24 V** ✅ ([datasheet Honeywell](https://resources.ampheo.com/static/datasheets/honeywell/ss41f.pdf)) — consommation 6,8 mA typ | ✅ |
| Alimentation `SS400` (SS41) | 3,8 – 30 V ✅ ([datasheet](https://www.farnell.com/datasheets/1714710.pdf)) | ✅ |
| **Type de sortie** | **Collecteur ouvert, sinking** ✅ — *« The open-collector sinking output voltage is easily interfaced… »* | ✅ |
| **Capacité de sink** | **20 mA max** continu (SS41F) ; SS400 : 20 mA continu, **50 mA cyclé**. `Vsat` ≤ 0,4 V à 15 mA | ✅ |
| Câblage dans l'application | **5 V** (rouge = 5 V, noir = GND) — ODrive, VESC, MAD-EE concordants | 🟡 |

⚠️ **Point critique** : le `SS41F` exige **4,5 V minimum**. Toute tentative d'alimenter les Hall en 3,3 V, ou un rail 5 V affaissé par la longueur de câble, sort de la plage garantie. → **Alimenter les Hall en 5 V et faire l'adaptation de niveau côté récepteur**, jamais l'inverse. C'est ce que fait déjà le montage retenu.

📐 **Validation du pull-up de 2,2 kΩ proposé en X.1.5** : 5 V / 2,2 kΩ = **2,3 mA**, à comparer aux **20 mA** que le capteur sait absorber ✅. Marge d'un facteur 8,7. Confirmé.

### ⚠️ Code couleur : ne pas câbler en dur

| Source | +5 V | GND | Ha | Hb | Hc |
|---|---|---|---|---|---|
| **ODrive** (doc officielle, moteur hoverboard) | rouge | noir | **jaune** | **bleu** | **vert** |
| **MAD-EE** (câble livré avec le ZS-X11H) | rouge | noir | **blanc** | **jaune** | **vert** |
| VESC (forum) | rouge | noir | bleu/jaune/vert, **ordre non confirmé** | | |

🟡 **Font consensus** : rouge = +5 V, noir = GND, et les trois signaux sont dans {jaune, bleu, vert} côté moteur.
🔴 **Contradictoire** : l'affectation Ha/Hb/Hc.

⚠️ **Le tableau de remapping du README `motor_interface`** annonce côté contrôleur : Hall B = **blanc**, Hall C = **orange**. **Aucune des sources trouvées ne mentionne un fil orange.** → **Le tableau de correspondance doit être refait au multimètre sur tes propres câbles, moteur par moteur, avant de le graver en sérigraphie.** C'est précisément le genre d'information qu'il ne faut pas hériter.

📐 **Bonne nouvelle** : ODrive écrit explicitement que *« the hall signals can be random. Just don't change it after calibration »*, et ElectroPeak conseille la même chose pour le ZS-X11H. **L'ordre n'a pas besoin d'être « correct », il a besoin d'être stable et connu.** → prévoir un connecteur permutable ou une calibration logicielle, et ne pas faire dépendre la carte d'un code couleur.

## Y.4.5 Paires de pôles et fréquences — confirmé

| Grandeur | Valeur | Source |
|---|---|---|
| **Paires de pôles** | **15** (30 aimants) | 🟡 **consensus fort, 2 sources indépendantes** : [ODrive doc](https://github.com/odriverobotics/ODrive/blob/2715cc6db06572cc35b0e9a1700ed77d67f3968a/docs/hoverboard.md) (*« 30 permanent magnet poles, and thus 15 pole pairs »*, `cpr = 15*6 = 90`) + mesure MAD-EE à l'analyseur logique (90 transitions/tour) |
| États Hall par tour | 90 | idem |
| **Fréquence par canal Hall** | 📐 `f = 15 × RPM / 60 = RPM / 4` Hz |
| Point d'ancrage mesuré | 577,9 tr/min à vide sous 36 V → **≈ 144,5 Hz par canal**, 867 transitions/s sur `SC` | 🟡 mesure |
| À 1,5 m/s (174 tr/min) | 📐 **≈ 43,5 Hz par canal** — cohérent avec le §H.4 du dossier |
| Espacement Hall | 120° électriques sur le 6,5" (60° sur le 8"), câblage étoile | 🔴 rapport maker isolé |

📐 **Conséquence pour le filtre RC** : le débit d'événements maximal est inférieur à **150 Hz par canal** même à vitesse maximale à vide. Un RC de quelques µs est très largement compatible, et le filtre proposé en X.1.5 (2,2 k ∥ 2,2 k avec 10 nF, τ ≈ 7 µs, coupure ≈ 22 kHz) laisse **deux décades** de marge au-dessus du signal utile tout en rejetant le hachage 16–20 kHz. Confirmé.

⚠️ **Retour terrain ODrive** : leurs entrées Hall n'ont pas de condensateur de filtrage et ils **recommandent d'en ajouter** pour un signal fiable. Le filtre n'est pas une précaution excessive.

## Y.4.6 ⚠️ MOSFET de niveau : abandonner le `2N7002`

| Référence | `VGS(th)` | `RDS(on)` garanti à faible `VGS` | `VDS` / `ID` | Boîtier |
|---|---|---|---|---|
| [`2N7002` Diodes](https://www.diodes.com/assets/Datasheets/ds11303.pdf) / [Nexperia](https://assets.nexperia.com/documents/data-sheet/2N7002.pdf) | **1,0 min / 2,5 V max** (2,75 V à −55 °C) | 🔴 **rien en dessous de VGS = 4,5 V**. Le plus bas garanti : 5,3 Ω max @ 4,5 V | 60 V / 0,3 A | SOT-23 |
| [**`DMG1012T`** Diodes](https://www.diodes.com/assets/Datasheets/DMG1012T.pdf) | **0,5 min / 1,0 V max** | ✅ **0,5 Ω max @ VGS = 2,5 V** · 0,7 Ω max @ 1,8 V | 20 V / 1 A | SOT-523 |
| [`BSS138` onsemi](https://cdn.sparkfun.com/datasheets/BreakoutBoards/BSS138.pdf) | 0,8 / 1,3 / **1,5 V max** | 6,0 Ω max @ 4,5 V (1,0 Ω typ) | 50 V / 0,2 A | SOT-23 |
| [`DMN601K`](https://www.diodes.com/assets/Datasheets/DMN601K.pdf) | 1,0 / 1,6 / **2,5 V max** | 3,0 Ω @ 5 V | 60 V / 0,3 A | ⚠️ **N'est PAS logic-level** malgré son nom |

⚠️ Avec 3,3 V de grille, l'overdrive garanti du `2N7002` dans le pire cas est de **0,8 V** (3,3 − 2,5), et **rien n'est garanti à froid**. Il fonctionne sur des pièces typiques ; ce n'est pas une spécification. Sur un robot extérieur qui démarre à 3 °C, ce n'est pas acceptable pour les lignes `DIR`, `EL` et `STOP`.

📐 **Retenu** :
- **`DMG1012T`** partout où 20 V suffisent (signaux logiques vers le ZS-X11H, LEDs, commandes) — seul de la liste dont le `RDS(on)` est **contractuel** à 2,5 V de grille.
- **`BSS138` onsemi** si un boîtier SOT-23 et 50 V sont nécessaires.
- ❌ **Jamais le `DMN601K`** : son nom suggère un logic-level, sa datasheet dit l'inverse.

---

# Y.5 Alimentation, protection, mesure

## Y.5.1 ⚠️ eFuses : le `TPS2595` ne tient pas le bus

| Référence | Plage d'entrée | Abs max | Courant | `RON` | OVP | Blocage inverse |
|---|---|---|---|---|---|---|
| [`TPS2595`](https://www.ti.com/lit/ds/symlink/tps2595.pdf) | **2,7 – 18 V** | **20 V** | 0,5 – 4 A | 34 mΩ | clamp fixe 3,8 / 5,7 / 13,7 V, ou OVLO programmable (`TPS259573`, `TPS259533`) | ❌ **NON** |
| [`TPS25940`](https://www.ti.com/lit/ds/symlink/tps25940.pdf) | 2,7 – 18 V | 20 V | 0,6 – 5,3 A | — | coupure | ✅ oui |
| [**`TPS2663x`**](https://www.ti.com/lit/ds/symlink/tps2663.pdf) | **4,5 – 60 V** | **67 V** | 0,6 – 6 A | 31 mΩ | cutoff ajustable (`TPS26630/31`) ou clamp fixe 35/39 V | ✅ avec FET N externe |
| [**`TPS2662x`**](https://www.ti.com/lit/ds/symlink/tps2662.pdf) | **4,5 – 60 V** | **62 V** | 25 – 880 mA | 478 mΩ | cutoff ajustable ou clamp fixe 38 V | ✅ **intégré**, jusqu'à −60 V en inverse |
| [`TPS1663x`](https://www.ti.com/lit/ds/symlink/tps1663.pdf) | 4,5 – 60 V | 67 V | 0,6 – 6 A | 31 mΩ | cutoff ajustable (`TPS16630`) | partiel |

📐 **Affectation retenue** :

| Rail | Composant | Pourquoi |
|---|---|---|
| Branches **42 V** de forte puissance | **`TPS26630`** ou `TPS26631` | 60 V, 6 A, cutoff **ajustable** |
| Auxiliaires **42 V** < 880 mA (ex. 5V_HOTEL) | **`TPS26620`** / `TPS26624` | Protection inverse **intégrée**, VSON-10 3 × 3 |
| Branches **12 V** et **5 V** | **`TPS259573`** | Broche `EN/OVLO`, seuil 1,2 V, auto-retry |

⚠️ **Piège à éviter** : les variantes à **clamp fixe** (35 V, 38 V, 39 V) sont **inutilisables sur un bus 42 V** — elles écrêteraient en permanence à pleine charge. **Prendre impérativement les variantes à cutoff ajustable.**

**Formule de réglage du `TPS2595`** ✅ : `ILIMIT [A] = 2000 / RILM [Ω] + 0,04`, `RILM` valide de **487 Ω à 5000 Ω** → 4,17 A à 0,44 A, précision ±7,5 %. Démarrage progressif : `CdVdt [pF] = 42000 / SR [V/ms]`.

📐 **OVLO 12 V coupant à 16 V** avec le `TPS259573` : diviseur pour ramener 16 V sur le seuil de 1,2 V → **124 kΩ / 10 kΩ ≈ 16,1 V** ✅.

## Y.5.2 ⚠️ INA226 inutilisable — passer à l'INA228

| Paramètre | [`INA226`](https://www.ti.com/lit/ds/symlink/ina226.pdf) | [**`INA228`**](https://www.ti.com/lit/ds/symlink/ina228.pdf) | [`INA229`](https://www.ti.com/lit/ds/symlink/ina229.pdf) |
|---|---|---|---|
| **Mode commun** | **0 – 36 V** (abs max 40 V) ❌ | **−0,3 à +85 V** ✅ | −0,3 à +85 V ✅ |
| Résolution | 16 bits | **20 bits** | 20 bits |
| Shunt pleine échelle | ±81,92 mV | ±163,84 mV (312,5 nV/LSB) ou ±40,96 mV (78,125 nV/LSB) | idem |
| Interface | I²C, **16 adresses** | I²C, **16 adresses** (A0/A1 vers GND/VS/SDA/SCL) | SPI 10 MHz |
| **Accumulateurs** | ❌ aucun | ✅ **Energy 40 bits + Charge 40 bits** matériels | idem |
| Précision | — | offset ±1 µV max, gain ±0,05 % max | idem |

⚠️ **L'`INA226` est physiquement inutilisable sur un bus 30–42 V.** C'est un piège classique : le composant est le plus connu de la famille, et son mode commun s'arrête à 36 V.

📐 **`INA228` retenu**, avec deux bénéfices inattendus :
1. **16 adresses I²C** → largement de quoi mettre les 4 branches moteur **plus** la mesure de bus sur le même bus I²C.
2. **Accumulateur de charge 40 bits matériel** → le comptage coulométrique est fait par le composant, sans dérive logicielle. Combiné à la précision (offset ±1 µV), cela rend enfin possible une estimation de SOC indépendante du BMS — ce que le §N.6 du dossier avait déclaré impossible **avec l'ACS758**. La conclusion du dossier reste juste pour l'ACS758 ; elle ne s'applique pas à l'INA228.

## Y.5.3 Diode idéale / ORing 5 V

[Datasheet TI LM66100](https://www.ti.com/lit/ds/symlink/lm66100.pdf)

| Paramètre | Valeur ✅ |
|---|---|
| `VIN` | 1,5 – 5,5 V (abs max inverse −6 V) |
| Courant continu | **1,5 A max** |
| `RDS(on)` @ 5 V | **79 mΩ typ / 95 mΩ max** (200 mA, 25 °C) |
| `IQ` | 150 nA typ ; shutdown 120 nA |
| Fuite inverse | 0,2 – 0,5 µA |
| Brochage 6 br. | `VIN`, `GND`, `CE` (actif bas), `N/C`, `ST` (drain ouvert), `VOUT` |
| Boîtier | SC-70-6, 2,1 × 2,0 mm |
| Externes | `CIN` ≥ 1 µF, `COUT` ~100 nF, `RCE` série sur `CE` |
| ORing | ✅ Explicitement supporté : deux LM66100, le `CE` de chacun câblé sur l'entrée opposée → *make-before-break*, la tension la plus haute l'emporte, courant inverse bloqué |

📐 **Validation pour le cas USB / convertisseur à 800 mA** : chute = 800 mA × 95 mΩ = **76 mV**, dissipation **61 mW** par composant. ✅ Très confortable, et **quatre fois moins que les 350 mV du `SS14`** — la contrainte « 5,0 V MAX » du silkscreen disparaît complètement.

⚠️ **Réserve** : aucune limitation de courant ni protection de court-circuit intégrée. → à combiner avec un eFuse ou une PPTC en aval.

**Alternative** : [`TPS2116`](https://www.ti.com/lit/ds/symlink/tps2116.pdf) ✅ — 1,6–5,5 V, **2,5 A**, `RDS(on)` 40 mΩ, `IQ` 1,32 µA, blocage inverse à 42 mV en 2 µs, modes *priority* et *manual*, SOT-5X3. Plus capable, un peu plus cher, et permet de **choisir** la source prioritaire par GPIO.

## Y.5.4 ⚠️ PPTC : les tensions sont un piège

| Référence | `Ihold` | `Itrip` | **`Vmax`** | `R1max` | Verdict |
|---|---|---|---|---|---|
| [`MF-MSMF010`](https://www.bourns.com/docs/product-datasheets/mf-msmf.pdf) | 0,10 A | 0,30 A | **60 V** ✅ | 15 Ω | ✅ OK partout |
| `MF-MSMF050` | 0,50 A | 1,00 A | ⚠️ **15 V** | 1,0 Ω | ❌ Marge nulle sur 12 V |
| `MF-MSMF110/16X` | 1,10 A | 2,20 A | ⚠️ **16 V** | 0,20 Ω | ❌ Marge nulle sur 12 V |
| [`1812L010`](https://www.littelfuse.com/assetdocs/resettable-ptcs-1812l-datasheet?assetguid=ca5c80cb-504e-4a8a-8e74-0107520a1717) | 0,10 A | 0,30 A | 30 V | 15 Ω | ✅ OK sur 12 V et 5 V |
| **`1812L050/30`** | 0,50 A | 1,00 A | **30 V** ✅ | 1,0 Ω | ✅ **Retenue sur 12 V** |
| `1812L110/16` | 1,10 A | 1,95 A | 16 V | 0,18 Ω | ❌ |

⚠️ **Trois conclusions** :
1. **Aucune PPTC 1812 de ces séries ne convient au bus 42 V.** Sur le bus, ce sont des fusibles ou des eFuses, point.
2. Sur le 12 V, la `MF-MSMF050` (15 V) est **en dessous** du rail. Prendre la **`1812L050/30`** (30 V).
3. Pour 1 A sur 12 V, aucune référence 1812 listée ne dépasse 16 V → passer en boîtier **2920** ou, mieux, en **eFuse `TPS2662x`**.

📐 **Pour la branche +5 V Hall de chaque moteur** (X.6.3) : `MF-MSMF010` ou `1812L010` — 100 mA de maintien, 60 V ou 30 V de tenue, tous deux largement suffisants.

## Y.5.5 TVS

| Référence | `VRWM` | `VBR` min–max | `VC` max | `IPP` | Puissance | Boîtier |
|---|---|---|---|---|---|---|
| [`SMCJ43A`](https://www.mouser.com/datasheet/2/848/SMCJ_Series-2887249.pdf) | 43 V | 48,2 – 52,4 V | **69,4 V** | 21,7 A | 1500 W | SMC |
| `SMCJ45A` | 45 V | 50,4 – 54,9 V | **72,7 V** | 20,6 A | 1500 W | SMC |
| `SMCJ48A` | 48 V | 53,8 – 58,4 V | **77,4 V** | 19,4 A | 1500 W | SMC |
| [`SMBJ13A`](https://www.littelfuse.com/assetdocs/tvs-diodes-smbj-series-datasheet?assetguid=ba555e99-a12d-4f72-a0b6-86b06c67171e) | 13,0 V | 14,40 – 15,90 V | 21,5 V | 28,0 A | 600 W | SMB |
| [`SMAJ5.0A`](https://www.littelfuse.com/assetdocs/tvs-diodes-smaj-datasheet?assetguid=13c2a823-03b8-4d1f-9ddc-9b44670aed9d) | 5,0 V | 6,40 – 7,00 V | 9,2 V | 43,5 A | 400 W | SMA |

⚠️ **Ces chiffres confirment le raisonnement du §D.8 du dossier, et le rendent quantitatif** :

> Une TVS dont le standoff dépasse 42 V (nécessaire pour ne pas conduire à pleine charge) écrête à **69–77 V** — bien au-dessus de la tenue de condensateurs 63 V.

📐 **Il n'existe donc aucune TVS capable de protéger des condensateurs 63 V sur un bus 42 V.** Ce n'est pas un mauvais choix de référence, c'est une impossibilité physique. **C'est exactement ce que le hacheur de freinage résout** : il écrête à 42,5 V, là où aucune TVS ne le peut. Les deux sont complémentaires — la TVS pour les transitoires sub-µs, le hacheur pour tout le reste.

⚠️ Noter la **fuite de 800 µA** de la `SMAJ5.0A`, typique des TVS basse tension — non négligeable sur un rail 5 V faiblement chargé.

🔴 **Non publié** : la capacité de jonction des séries SMAJ/SMBJ/SMCJ n'est donnée qu'en courbe, jamais tabulée.

## Y.5.6 Surveillance de surtension à FET externe

| Référence | Plage | OVP | Latch | Boîtier |
|---|---|---|---|---|
| [`LM5060`](https://www.ti.com/lit/ds/symlink/lm5060.pdf) | **5,5 – 65 V** (abs max 75 V) | Seuil `OVP` **2,0 V typ**, hystérésis 240 mV, décharge la grille à 80 mA | ❌ **non latché** — redémarre sous 1,76 V | VSSOP-10 |
| [`LTC4359`](https://www.analog.com/media/en/technical-documentation/data-sheets/ltc4359.pdf) | **4 – 80 V** | ❌ **aucune OVP intégrée** | — | DFN-6 / MSOP-8 / SO-8 |

- `LM5060` : `UVLO` à 1,6 V typ, charge pump clampant la grille à ~16,8 V au-dessus de `OUT`, détection de faute `VDS`, temporisation par condensateur externe, **FET N externe obligatoire**.
- `LTC4359` : régulation *ideal diode* à **~30 mV** aux bornes du FET, **protection en inversion jusqu'à −40 V**, `IQ` 150 µA. ⚠️ **Pas d'OVP** — la datasheet renvoie explicitement à un `LT4363` externe pour cette fonction.

📐 **Pour la protection en inversion de polarité du bus 42 V** (§D.8 du dossier) : **`LTC4359` + FET N** est le bon choix — 80 V de plage, −40 V en inverse, 30 mV de chute au lieu des 40–60 W qu'aurait dissipés une Schottky. Pour l'OVP, c'est le hacheur de freinage et les eFuses à cutoff ajustable qui s'en chargent, pas ce composant.

## Y.5.7 Résistances de précharge et de freinage

[Note d'application Vishay Dale, *Wirewound Resistors Pulse Handling Capabilities*](https://www.vishay.com/docs/49076/_wirewound_resistors_pulse_handling_capabilities_vmn_pl0396_1604.pdf)

**Quel paramètre lire** ✅ : l'**Energy Rating (ER), en joules**. Pour des impulsions plus courtes que le *cross-over point*, *« all of the pulse energy is dissipated in the resistance element »* — régime adiabatique, la contrainte est simplement **E ≤ ER**. L'ER est calculé pour que le fil ne dépasse pas **+350 °C**. `Cross-over point [s] = ER / P0`, où `P0` est la capacité de surcharge à 1 seconde.

🔴 **Difficulté pratique** : **aucune** des trois familles examinées (Vishay RH, TT WH, TE HS) ne tabule d'ER en joules. On ne dispose que de la courbe *pulse power vs pulse duration* ou de la spécification *short-time overload*.

| Usage | Référence | Caractéristiques ✅ |
|---|---|---|
| **Précharge 10 Ω / 25 W** | [`Vishay RH02510R00FE02`](https://www.vishay.com/docs/30201/rhnh.pdf) | 25 W sur dissipateur, 20 W en air libre. **Short-time overload : 5 × puissance nominale pendant 5 s** → 125 W × 5 s ≈ **625 J** |
| | [`TT Electronics WH25-10RJI`](https://www.mouser.com/datasheet/2/414/ttelectronics_wh-559251.pdf) | 25 W sur dissipateur, impédance thermique 6 °C/W |
| **Hacheur 15–22 Ω / 100 W** | [`TT WH100-22RJI`](https://www.mouser.com/datasheet/2/414/ttelectronics_wh-559251.pdf) | 100 W sur dissipateur, `Vmax` 1900 V, **impédance thermique 1 °C/W** |
| | [`TE CGS HSC100 15R J` / `HSC100 22R J`](https://www.mouser.com/pdfDocs/ENG_DS_1773035_E.pdf) | 100 W, `Vmax` 1900 V, courbes de surcharge de 100 ms à 60 s |

📐 **Vérification de la précharge** : `E = ½ · C · V²`. Avec 10 mF 📐 et 42 V → **8,8 J**, contre ≈ 625 J de tenue en surcharge courte pour la `RH025`. ✅ Marge d'un facteur 70. La précharge n'est absolument pas le cas dimensionnant.

📐 **Le cas dimensionnant est le freinage régénératif continu** : 51 W à 10 % de pente, 76 W à 15 % (X.5.1). D'où le 100 W, avec une impédance thermique de 1 °C/W qui impose un **montage sur châssis ou dissipateur, jamais sur le PCB**.

---

# Y.6 Watchdog et IMU

## Y.6.1 `TPL5010`

[Datasheet TI TPL5010](https://www.ti.com/lit/ds/symlink/tpl5010.pdf)

| Paramètre | Valeur ✅ |
|---|---|
| Alimentation | **1,8 – 5,5 V** ; **~35 nA typ** @ 2,5 V |
| Boîtier | SOT-23-6 |
| Brochage | 1 `VDD` · 2 `GND` · 3 `DELAY/M_RST` · 4 `DONE` · 5 `WAKE` · 6 `RSTn` |
| **Réglage de l'intervalle** | ⚠️ Par **résistance sur `DELAY`**, `R_EXT` de **500 Ω à 170 kΩ**, résistance **1 %** recommandée. **Table de correspondance (Table 3), pas de formule** : 10,18 kΩ → 8 s · 22,02 kΩ → 1 min · 170 kΩ → 2 h |
| `WAKE` | Normalement bas, **impulsion de 20 ms** au début de chaque intervalle |
| **`DONE`** | ⚠️ *« The TPL5010 recognizes a valid DONE signal as a **low to high transition** »*, à fournir **au moins 20 ms avant le prochain front montant de `WAKE`** |
| **`RSTn`** | **Drain ouvert**, actif bas, **impulsion de 320 ms**. → **pull-up externe obligatoire** |
| Déclenchement | `DONE` manquant avant l'intervalle suivant, ou reset manuel (`M_RST` haut ~20 ms) |

⚠️ **Deux pièges** : l'intervalle se lit dans une table, il ne se calcule pas ; et `RSTn` étant à drain ouvert, oublier son pull-up donne un watchdog silencieusement inopérant.

## Y.6.2 `BNO085` — trois exigences souvent manquées

[Datasheet BNO08X rev 1.17, CEVA](https://www.ceva-ip.com/wp-content/uploads/BNO080_085-Datasheet.pdf)

| Paramètre | Valeur ✅ |
|---|---|
| **Mode SPI** | **CPOL = 1, CPHA = 1** (mode 3). *« the clock idles high and data is captured on the rising edge »* |
| Horloge SPI max | **3 MHz** ; `tck` min 0,33 µs ; `CLK`→`MISO` valide 35 ns max |
| **`H_INTN`** | Actif **bas**, à relier à un GPIO **capable de réveil**. ⚠️ *« if the host fails to respond to the assertion of H_INTN within approximately **10 ms**, the BNO085 will timeout, deassert H_INTN and retry »* |
| **⚠️ `PS1` et `PS0/WAKE`** | *« **Both pins must be high from before reset until after the first assertion of H_INTN** to select the SPI interface. Pin 5 may be tied to VDDIO. **Pin 6 must be connected to a GPIO** so that the WAKE functionality can be performed. »* |
| **`BOOTN`** | À tirer haut par **10 kΩ**. Bas au reset = mode bootloader/DFU |
| `VDDIO` | 1,7 – 3,6 V |
| `VDD` (capteurs) | 2,4 – 3,6 V |
| **Séquencement** | *« it is **mandatory that VDD is powered on and driven to the specified level before or at the same time as VDDIO** »* |
| **Démarrage** | Reset `tnrst` min 10 ns ; **initialisation interne min 90 ms** ; configuration 4 ms typ. *« The host may begin communicating **after it has asserted H_INTN** »* |

⚠️ **Trois points qui ne sont dans aucun des documents du projet** :
1. **`PS1` et `PS0` doivent être hauts avant le reset** pour sélectionner le SPI — sans quoi le composant démarre en I²C, précisément le bus que le §L6 interdit.
2. **`PS0/WAKE` doit aller à un GPIO**, pas être câblé en dur — c'est aussi la ligne de réveil.
3. Il faut attendre **l'assertion de `H_INTN`**, pas un délai fixe de 90 ms.

⚠️ **Et une réserve pratique** : les cartes de dérivation Adafruit et SparkFun ajoutent leur propre régulateur et leur adaptation de niveau, et **strappent généralement `PS0`/`PS1` pour l'I²C**. Passer en SPI demande souvent de **modifier un pontet sur la carte**. → À vérifier sur ta carte avant de router le connecteur.

---

# Y.7 Connectique et porte-fusibles

## Y.7.1 ⚠️ Les fusibles automobiles standard sont limités à 32 V DC

C'est la découverte la plus opérationnelle de cette section, et elle confirme en la précisant la « liste rouge » du §E.7 du dossier.

| Produit | Tenue DC ✅ | Verdict bus 42 V |
|---|---|---|
| [Fusible **MIDI / AMI standard** Littelfuse 0498xxx](https://www.mouser.com/datasheet/2/240/littelfuse_littelfuse_midi_datasheet-1175989.pdf) | **32 V DC**, pouvoir de coupure 1000 A @ 32 V | ❌ **INADAPTÉ** |
| Fusible **ATO / ATC / Mini** lame | **32 V DC** | ❌ **INADAPTÉ** |
| [Fusible **MIDI Slo-Blo 58 V**, `BF1 58V` (142.5631.58xx)](https://www.littelfuse.com/products/fuses-overcurrent-protection/fuses/automotive-fuses/bolt-down-fuses/midi/bf1-58v) | **58 V DC** | ✅ **Retenu** |
| [Porte-fusible **MIDI 498 Series** boulonné (`04980900S`)](https://www.littelfuse.com/assetdocs/midi-498-datasheet?assetguid=5a3ddf39-419e-44c6-91bf-270e6a6d560b) | **58 V DC**, 150 A continu / 200 A max | ✅ **Montable sur carte** — trous **M3 in-board** ou M6 out-board |
| [**Blue Sea MRBF** 5191 / 2151](https://www.bluesea.com/products/5191/MRBF_Terminal_Fuse_Block_-_30_to_300A) | **58 V DC**, 30–300 A | ✅ Mais **goujon M8, pas un composant CI** — reste au point batterie |
| Clip lame CI (ex. [Keystone 3568](https://www.keyelco.com/product.cfm/product_id/306)) | Le support est coté 500 V, **mais le fusible lame plafonne à 32 V DC** | ❌ Le maillon faible est le fusible |

⚠️ **Le piège est que le support et le fusible ne sont pas cotés au même endroit.** Un porte-fusible « 500 V » qui reçoit une lame ATO 32 V donne un ensemble à 32 V. Sur un bus qui monte à 42 V, l'arc d'un fusible 32 V ne s'éteint pas franchement — c'est exactement le scénario d'incendie décrit au §L.3.

📐 **Retenu pour `safety_power`** :
- 4 branches moteur 25 A + 2 branches DC/DC → **porte-fusible MIDI 498 Series montage in-board M3**, garni de fusibles **`BF1 58V`**.
- Point batterie → **MRBF 58 V** sur goujon, hors carte, conformément au §D.3.

## Y.7.2 Borniers de puissance : le 5,08 mm ne passe pas

| Référence | Pas | I nominal ✅ | U ✅ | Section fil | Couple |
|---|---|---|---|---|---|
| [**Phoenix `PC 5/2-STF1-7,62`** (1777833)](https://www.phoenixcontact.com/en-us/products/pcb-plug-pc-5-2-stf1-762-1777833) | **7,62 mm** | **32 A** (IEC) · 41 A (cULus 600 V) | 1000 V | 0,2–10 mm² rigide / 0,2–6 mm² souple | **0,5–0,8 N·m** |
| [**WAGO `2624-…`**](https://pim.galco.com/Manufacturer/Wago/TechDocument/Data%20Sheet/2624-1112_dat.pdf) | 5 et **7,5 mm** | **41 A** (IEC) · 26 A (UL Gr. B) | 630 V | 0,2–6 mm² | levier push-in, **sans couple** |
| WAGO `2604-…` | 5 / 7,5 mm | 4 mm² max | — | AWG 24–12 | ❌ série trop faible |
| [Würth `WR-TBL 691406710002B`](https://www.we-online.com/components/products/datasheet/691406710002B.pdf) | 5 mm | **20 A** | 300 V | 0,2–2,5 mm² | ❌ insuffisant |
| [Phoenix `MKDS 3/2-5,08`](https://www.phoenixcontact.com/en-pc/products/pcb-terminal-block-mkds-3-2-508-1711725) | 5,08 mm | **24 A** | 400 V | 2,5 mm² | ❌ insuffisant |

⚠️ **Le déclassement par nombre de pôles est réel et documenté.** Phoenix publie des courbes à 2, 5, 10 et 15 pôles et applique **`I_admissible = 0,8 × I_base`** ✅. Un `MKDS 3` à 24 A nominal tombe donc à **≈ 19 A** — sous les 25 A d'une branche moteur. WAGO ne documente aucun déclassement par pôle sur les fiches 2624.

📐 **Retenu** : **`PC 5/2-STF1-7,62` en 2 pôles** pour les branches 25 A (32 A après application du 0,8 → 25,6 A, marge juste mais tenue), ou **WAGO 2624 au pas 7,5 mm** si le montage sans clé dynamométrique est préféré. ⚠️ **Un bornier 5,08 mm est exclu du chemin de puissance** — ce que la revue (X.9.3) avait supposé et qui est maintenant chiffré.

## Y.7.3 ⚠️ Amass ne publie aucune spécification de courant

| Connecteur | Courant | Tension | Statut de la donnée |
|---|---|---|---|
| `XT30` / `XT60` / `XT90` / `XT90-S` | 15 / 60 / 90 A couramment cités | 500 V cité | 🔴 **REVENDICATION VENDEUR.** Le site Amass ne contient **ni fiche technique publique, ni valeur de courant** — seulement des images et des noms de modèles. Les chiffres circulent via les distributeurs |
| [**Anderson `PP45`**](https://www.andersonpower.com/content/dam/app/ecommerce/product-pdfs/DS-PP1545.pdf) | **45 A** (#10 AWG), jusqu'à 55 A/pôle selon config | **600 V** (UL 1977) | ✅ Spécification fabricant. **Contacts CI disponibles.** `R_contact` 0,525 mΩ |
| [**Anderson `SB50`**](https://www.mouser.com/datasheet/2/22/DS_SB50_5__1_-1158116.pdf) | **50 A wire-to-PCB** (UL 1977), 120 A wire-to-wire | **600 V** | ✅ Spécification fabricant. Hot-plug UL jusqu'à 50 A. **10 000 cycles** (argent) / 1 500 (étain) |

📐 **Conséquence pour ce dossier** : le §D.5 du dossier retient « Anderson SB50 ou XT90-S ». **Les deux ne sont pas au même niveau de preuve.** Le XT90-S peut rester en usage pratique — il est très répandu et fonctionne — mais il ne peut pas figurer comme choix **justifié** dans une spécification, puisqu'aucune donnée constructeur n'existe. Pour tout ce qui doit être défendable, **Anderson**.

⚠️ Le `SB50` est aussi le seul des deux dont le **hot-plug est qualifié** (UL, jusqu'à 50 A) — ce qui est pertinent vu l'interdiction de débranchement sous charge du §D.5.

## Y.7.4 Connecteurs de signal : attention au verrouillage

| Série | Pas | I / contact ✅ | U ✅ | **Verrouillage** ✅ |
|---|---|---|---|---|
| [JST `GH`](https://www.jst-mfg.com/product/pdf/eng/eGH.pdf) | 1,25 mm | 1,0 A | **50 V** | ✅ **Positif** |
| [JST `PH`](https://www.jst-mfg.com/product/pdf/eng/ePH.pdf) | 2,0 mm | 2 A | 100 V | ⚠️ **Friction seulement** |
| [JST `XH`](https://www.jst-mfg.com/product/pdf/eng/eXH.pdf) | 2,5 mm | 3 A | 250 V | ⚠️ **Friction seulement** |
| [JST `VH`](https://www.jst-mfg.com/product/pdf/eng/eVH.pdf) | 3,96 mm | **10 A** | 250 V | ✅ **Positif** |
| [Molex **Micro-Fit 3.0**](https://www.molex.com/content/dam/molex/molex-dot-com/products/automated/en-us/productspecificationpdf/430/43045/PS-43045-001.pdf) | 3,00 mm | voir table | **600 V** | ✅ **Latch positif**, rétention ≥ 24,5 N |
| [Molex **Mini-Fit Jr.**](https://www.molex.com/content/dam/molex/molex-dot-com/products/automated/en-us/productspecificationpdf/555/5556/PS-5556-002-001.pdf) | 4,20 mm | voir table | **600 V** | ✅ **Latch positif** |

⚠️ **Tu demandes explicitement des « connecteurs verrouillables ». Le JST PH du README `motor_interface` n'en est pas un** — c'est un maintien par friction. Sur un robot qui vibre, un connecteur Hall à friction qui se retire partiellement donne une odométrie fausse intermittente : le pire mode de panne possible (§M.2 panne 15).

**Déclassement Micro-Fit 3.0 selon le nombre de circuits** ✅ :

| AWG | 2 ckt | 6 ckt | 12 ckt | 24 ckt |
|---|---|---|---|---|
| 20 | **7,0 A** | 5,5 A | 5,0 A | 4,5 A |
| 22 | 6,0 A | 4,5 A | 4,0 A | 3,5 A |
| 24 | 5,5 A | 4,5 A | 3,5 A | 3,0 A |

**Mini-Fit Jr.** ✅ : 18 AWG → 9 A (2–3 ckt), 8 A (4–6), 7 A (7–10), 6 A (12–24).

📐 **Stratégie de connectique retenue** :

| Usage | Connecteur | Justification |
|---|---|---|
| Branches 42 V / 25 A | `PC 5/2-STF1-7,62` ou Anderson `PP45` CI | 32–45 A, spécification constructeur |
| 12 V et 5 V vers cartes | **Micro-Fit 3.0**, 20 AWG | 5,5–7 A/circuit, 600 V, latch positif |
| CAN + `/SAFE` + alim | **Micro-Fit 3.0 6 positions** | Un seul type sur tout le robot |
| **Hall moteur et variateur** | ⚠️ **Micro-Fit 3.0 6 pos** plutôt que JST PH | Verrouillage positif exigé. 6 positions au lieu de 5 → **impossible de croiser avec un connecteur 5 broches existant** |
| `VR` / `DIR` / `EL` / `STOP` / NTC | Micro-Fit 3.0 8 pos | Nombre de broches distinct = anti-erreur |
| BNO085 déporté | JST `GH` 8 pos | Petit, verrouillage positif, 50 V suffisant |

## Y.7.5 Supports pour le module ESP32 amovible

| Type | Référence | Cycles ✅ | I / contact ✅ | Ø broches acceptées |
|---|---|---|---|---|
| Estampé | [Samtec `SSW-119-01-G-S`](https://suddendocs.samtec.com/catalog_english/ssw_th.pdf) | **100 min.** (avec 10 µ" Au) | 4,7 A | — |
| **Tourné (machined)** | [Mill-Max `310-43-119-41-001000` / `311-…`](https://www.mill-max.com/sites/default/files/external/catalog/2018-07/029-087.pdf) | 100 min. (série 315) ; **1000 min.** sur les séries à contact 4 doigts | **3 A** | **0,015–0,025"** |
| Tourné bas profil | Mill-Max `315-43-119-41-001000` | 100 min. | 3 A | ⚠️ **0,015–0,022" seulement** |

📐 **Retenu : support tourné Mill-Max `310` ou `311`.** Deux raisons :
1. Le contact BeCu à 4 doigts se déforme beaucoup moins que le contact estampé, et les séries haute fiabilité sont cotées **1000 cycles** contre 100 — c'est exactement ton exigence de remplacement sans dessoudage.
2. ⚠️ **Les broches d'une DevKitC sont des broches carrées de 0,025".** La série `315` (bas profil) n'accepte que **0,022" maximum** — elle ne les recevra pas correctement. **Prendre impérativement `310` ou `311`.** C'est une erreur silencieuse : le module « rentre » mais force et abîme les contacts.

Les 3 A par contact sont sans conséquence : la consommation d'un ESP32 passe par une ou deux broches d'alimentation, très en dessous.

## Y.7.6 Largeur de piste — confirmation IPC-2221

Formule IPC-2221, couche externe : `A[mils²] = (I / (k·ΔT^b))^(1/c)` avec **k = 0,048**, b = 0,44, c = 0,725.

| Courant | Section | **Largeur, 2 oz, couche externe, ΔT 30 °C** |
|---|---|---|
| 25 A | 708 mils² | **≈ 6,5 mm** |
| 40 A | 1356 mils² | **≈ 12,5 mm** |

✅ **Cela valide la table cuivre du README `safety_power`** (« 40 A → 12,3 mm en 2 oz »), à l'arrondi près.

⚠️ **Réserve de validité** : les abaques IPC-2221 d'origine ne couvrent que **jusqu'à 35 A**, 400 mils de largeur, ΔT 10–100 °C, cuivre 0,5–3 oz. **Le chiffre à 40 A est une extrapolation hors domaine.**

📐 **Conséquence** : pour les branches à 25 A, ne pas router une piste de 6,5 mm mais **un polygone de cuivre pleine largeur sur les deux faces, cousu de vias** — ce que le README préconisait déjà (10 mm des deux côtés, vias tous les 5 mm). Cette approche est la bonne, et elle a l'avantage de sortir du domaine où la formule IPC devient douteuse.

---

# Y.8 Récapitulatif : la BOM validée

| Fonction | Retenu | Remplace | Raison du changement |
|---|---|---|---|
| Transceiver CAN | **`TCAN1042HVDR`** | `SN65HVD230` | Tenue de défaut ±70 V au lieu de +16 V |
| ESD CAN | **`NUP2105L`** | — | Clampe à 40 V @ 5 A |
| Self mode commun CAN | **`ACT45B-510-2P-TL003`** | — | 200 mA, 1 Ω, série dédiée CAN |
| Tampon Hall | **`SN74LVC3G17`** ×2 | `SN74LVC14AD` | ⚠️ `Ioff` garanti + non inverseur |
| Clamp entrée Hall | **`BAT54S`** | — | Seule config série de la famille |
| TVS rail logique | **`PESD5V0F1BL`** | — | 0,4 pF, ±10 kV IEC |
| DAC consigne `VR` | **`MCP4728`** | — | ✅ confirmé : sortie EEPROM = 0 V au démarrage |
| Ampli `VR` | **`TLV9062`** | `MCP6002` | 20 mV aux rails, spec garantie à 2 kΩ |
| MOSFET niveau logique | **`DMG1012T`** | `2N7002` | ⚠️ Seul avec `RDS(on)` garanti à VGS = 2,5 V |
| eFuse bus 42 V | **`TPS26630`** | `TPS2595` | ⚠️ Le TPS2595 plafonne à 18 V |
| eFuse auxiliaire 42 V | **`TPS26620`** | — | Protection inverse intégrée |
| eFuse 12 V / 5 V | **`TPS259573`** | — | Broche `EN/OVLO` |
| Mesure de courant | **`INA228`** | `INA226` | ⚠️ 85 V au lieu de 36 V + accumulateur de charge |
| Diode idéale 5 V | **`LM66100`** | `SS14` | Supprime la contrainte « 5,0 V MAX » |
| Anti-inversion bus | **`LTC4359`** + FET N | — | 80 V, −40 V inverse, 30 mV de chute |
| PPTC 12 V | **`1812L050/30`** | `MF-MSMF050` | ⚠️ 30 V au lieu de 15 V |
| PPTC Hall 5 V | **`MF-MSMF010`** | — | 100 mA / 60 V |
| TVS bus | **`SMCJ43A`** | `SMCJ45A/48A` | Standoff le plus bas au-dessus de 42 V |
| Watchdog | **`TPL5010`** | — | ⚠️ `RSTn` drain ouvert : pull-up obligatoire |
| Résistance hacheur | **`WH100-22RJI`** | — | 100 W, 1 °C/W, hors PCB |
| Résistance précharge | **`RH02510R00FE02`** | — | 625 J en surcharge courte |
| Level shifter WS2812B | **`SN74AHCT125`** | — | `VIH` = 2,0 V à `VCC` = 5 V ✅ |
| Bornier 42 V / 25 A | **`PC 5/2-STF1-7,62`** | `MKDS 3` 5,08 mm | ⚠️ 24 A × 0,8 = 19 A, insuffisant |
| Porte-fusible de branche | **MIDI 498 Series** M3 in-board | clip lame ATO | ⚠️ Lame ATO = 32 V DC |
| Fusible de branche | **`BF1 58V`** | MIDI standard | ⚠️ MIDI standard = 32 V DC |
| Connecteur puissance débrochable | **Anderson `PP45`** CI | `XT90-S` | 🔴 Amass ne publie aucune spec |
| Connecteur signal et Hall | **Micro-Fit 3.0** | JST `PH` | ⚠️ Le PH n'a qu'un maintien par friction |
| Support module ESP32 | **Mill-Max `310`/`311`** | support estampé | ⚠️ Broches 0,025" — la série `315` ne les accepte pas |

## Y.8.1 Ajout du 24 août 2026 — le contacteur statique

Ce document a été écrit **avant** la décision de concevoir le contacteur sur la carte. Le bloc correspondant est validé dans [`12-contacteur-statique.md`](12-contacteur-statique.md) ; voici son résumé de nomenclature.

| Fonction | Retenu | Remplace | Raison |
|---|---|---|---|
| Contrôleur de coupure | **`TPS4810-Q1`** | contacteur DC externe 80–200 € | ✅ **Seul composant >40 V à deux sorties de grille indépendantes** — condition du diagnostic de transistor collé |
| MOSFET de coupure | **2 × `IPB017N10N5`** D²PAK-7, 100 V, 1,7 mΩ | — | Tête-bêche source commune : bloque aussi la **régénération** |
| TVS d'ouverture | **`SMCJ48A`** drain-à-drain | — | `VC` 77,4 V < 100 V `VDS` → 22 V de marge |
| Diode de roue libre | Schottky 100 V, charge → GND | — | Transitoire **négatif** du faisceau côté charge |
| FET de précharge | P-canal 100 V, ~100 mΩ | relais de précharge | Saturé, pas en linéaire |
| Résistance de précharge | **`RH02510R00FE02`** 10 Ω / 25 W | — | ⚠️ **Conservée** — voir ci-dessous |

⚠️ **Écartés, et pourquoi** — c'est le tableau à retenir :

| Composant | Pourquoi il ne convient pas ✅ |
|---|---|
| `LTC7000`, `LTC4368`, `LM5069`, `TPS2492`, `LTC4260`, `LTC4364`, `TPS4811-Q1` | **Une seule sortie de grille** → diagnostic de transistor collé impossible |
| `TPS1210-Q1` | 45 V absolus contre un bus à 42 V — aucune marge |
| `TPS2663x` / `TPS1663x` | **6 A maximum** — et aucun eFuse à FET intégré n'existe au-dessus de 6 A en 40–80 V |

⚠️ **La précharge par rampe de grille est impossible** : un trench moderne a une SOA de **0,5 A à 54 V / 10 ms** (chiffre Infineon), là où il en faudrait **4,2 A à 42 V**. Facteur 16 à 40. Le parallélisme n'aide pas — en linéaire, le die le plus chaud attire plus de courant. **La résistance de précharge reste.**

---

# Y.9 Ce qui reste à mesurer sur ton matériel

Aucune datasheet ne remplacera ces sept mesures.

| # | Mesure | Pourquoi aucune source ne peut répondre | Bloque |
|---|---|---|---|
| **P1** | **Entraxe des rangées du module ESP32**, au pied à coulisse | Le plan Espressif ne légende aucune cote (25,40 établi par géométrie) | Tout le PCB |
| **M4** | **Polarité de `EL`/`STOP`** sur **tes** cartes | Consensus fort mais pas de spec, et ≥ 2 révisions matérielles | La chaîne de sécurité |
| **M5** | **Cavalier `J1`** : ponté ou retiré ? | 🔴 Contradiction directe entre deux sources | Le mode PWM |
| **M7** | **Niveau et impédance de `SC` et des lignes Hall** | 🔴 Aucune source, aucun chiffre publié | `motor_interface` |
| — | **Les Hall sont-ils réellement à collecteur ouvert ?** Test de pull-up au multimètre | Certains clones « 41F » sont push-pull | Le dimensionnement du pull-up |
| — | **Code couleur Hall réel**, fil par fil, moteur par moteur | 🔴 Sources contradictoires, et « orange » n'apparaît nulle part | La sérigraphie |
| — | **Variante exacte des modules ESP32** (`D0WD-V3` ou `D0WDR2-V3` avec PSRAM) | ⚠️ Sur la variante PSRAM, `GPIO16` est **inutilisable** — or le dossier l'assigne à l'UART BMS | L'affectation des GPIO |

---

*À rattacher au dossier d'architecture comme section Y, après la revue de conception (section X).*
