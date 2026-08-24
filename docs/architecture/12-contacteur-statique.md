# AA. Contacteur statique intégré à `safety_power` — étude de faisabilité

**Version** : 1.0 — 24 août 2026
**Décision** : contacteur **conçu sur la carte**, pas acheté (décision du 24 août)
**Hypothèses figées** : masse **35 kg**, pente **≤ 10 %**, vitesse 1,5 m/s
**Verdict** : ✅ **Faisable** — mais pas de la manière que j'avais décrite. Deux de mes affirmations étaient fausses.

---

# AA.0 Ce que j'avais dit de faux

| Mon affirmation | La réalité | Source |
|---|---|---|
| « Supprime la précharge — rampe de grille » | ❌ **Impossible.** Un MOSFET trench moderne a une SOA de **0,5 A à 54 V / 10 ms** ; il en faudrait **4,2 A à 42 V**. Facteur 16 à 40 selon la durée. **La résistance de précharge reste.** | [Infineon, Product Brief OptiMOS Linear FET](https://www.infineon.com/dgdl/Infineon-MOSFET_OptiMOS_LinearFET-ProductBrief-v01_00-EN.pdf?fileId=5546d4625d5945ed015d7f3ed34b00a8) |
| « Le driver qui va bien est le `LTC7000` » | ❌ **Mauvais choix.** Le LTC7000 n'a **qu'une seule sortie de grille**. Les deux MOSFET tête-bêche partagent alors grille et source, ce qui rend **impossible** le diagnostic de transistor collé — qui exige de les commander séparément. | [LTC7000](https://www.analog.com/media/en/technical-documentation/data-sheets/ltc7000-7000-1.pdf) · [TI SLUAAS8](https://www.ti.com/lit/an/sluaas8/sluaas8.pdf) |

Le reste tient : le contacteur statique est le bon choix, il est nettement moins cher, et il est **plus** fail-safe qu'une bobine. Mais la topologie correcte est celle que TI documente pour les disjoncteurs de BMS, pas celle que j'avais improvisée.

---

# AA.1 Pourquoi la rampe de grille est à rejeter — le calcul

## AA.1.1 Le besoin

Bus 42 V, capacité totale **10 mF** 📐 (à confirmer par M10).

| Grandeur | Formule | Valeur |
|---|---|---|
| Énergie dans le condensateur | ½·C·V² | **8,8 J** |
| Énergie dissipée dans l'élément série | = la même, quelle que soit sa nature | **8,8 J** |
| Courant à rampe constante sur T | I = C·V/T = 0,42/T | voir table |

| Rampe T | Courant | V_DS initial | **P crête** |
|---|---|---|---|
| 0,1 s | **4,2 A** | 42 V | **176 W** |
| 0,3 s | 1,4 A | 42 V | 59 W |
| 1 s | 0,42 A | 42 V | 18 W |

## AA.1.2 Ce que la SOA autorise réellement

⚠️ **Le chiffre qui tranche**, publié par Infineon en toutes lettres, à `RDS(on)` identique :

> *« Whilst the OptiMOS 5 100 V, 1,7 mΩ power MOSFET has a safe operating area of **0,5 A**, [the Linear FET] offers **11,5 A (@ 54 V, 10 ms)**. »*

Sa limite purement **thermique** à 10 ms serait de l'ordre de 1250 W. Sa limite réelle est **27 W**. **Facteur 46.** Ce n'est pas de la thermique, c'est l'**instabilité thermique** — sous le point à coefficient de température nul, les zones les plus chaudes du die attirent davantage de courant et s'emballent. Nexperia nomme le phénomène explicitement : [effet Spirito](https://www.nexperia.com/applications/interactive-app-notes/IAN50006_Power_MOSFETs_in_linear_mode), et chiffre la perte à *« around 15 A (75 % less) »* sur un exemple à 20 V.

| Composant | Besoin à 0,1 s | SOA disponible à 42 V | Verdict |
|---|---|---|---|
| `IPB017N10N5` (trench, 1,7 mΩ) | 4,2 A | ≈ **0,26 A** à 100 ms | ❌ **échec ×16** |
| `IPB017N10N5LF` (LinearFET) | 4,2 A | ≈ 3,9 A à 60 °C | ⚠️ marginal |
| `IXTH64N10L2` (SOA garantie) | 4,2 A | 5,4 A à 40 V, 5 s | ✅ mais **32 mΩ** → 20 W à 25 A, inutilisable en principal |

⚠️ **Et mettre des transistors en parallèle n'aide pas.** En mode linéaire, le partage de courant ne se fait pas : le die le plus chaud prend plus de courant. Aucun fabricant ne publie de facteur de dérating pour le parallélisme linéaire. Infineon recommande même, pour du linéaire continu, *« MOSFETs of previous technology generations and/or higher voltage classes »* — c'est-à-dire précisément **pas** un trench moderne à faible `RDS(on)`.

## AA.1.3 L'argument qui achève, indépendamment de la SOA

Un driver ne rampe pas le **courant**, il rampe la **tension de grille**, en boucle ouverte. Près du seuil, `I_D = gm·(V_GS − V_th)`. Sur un trench basse tension, `gm` est énorme et `V_th` dérive d'environ **−5 mV/K**.

📐 **Une rampe de grille en boucle ouverte ne fixe ni le courant crête ni la durée** : les deux varient d'un facteur plusieurs avec la température et la dispersion de lot. On ne dimensionne pas une marge de SOA de 4 A avec une commande dont l'incertitude est un facteur 3.

## AA.1.4 → La résistance de précharge reste

C'est aussi la topologie de référence de TI pour les disjoncteurs de BMS ([SLUAAS8](https://www.ti.com/lit/an/sluaas8/sluaas8.pdf)) : `Q1` + `Q2` en chemin principal, **`Q3` + résistance en parallèle** pour la précharge.

> *« To limit the high inrush current during power up pre-charge feature… Once the load capacitor charges close to battery voltage then the main FET path can be turned ON and the pre-charge path can be turned off. »*

**Dimensionnement retenu** — identique au §D.5 du dossier, donc rien à changer là-bas :

| Paramètre | Valeur |
|---|---|
| R | **10 Ω / 25 W bobinée** (`Vishay RH02510R00FE02`) |
| I crête | 4,2 A |
| τ = R·C | 0,1 s → **99 % en 0,5 s** |
| Énergie dans R | **8,8 J** contre ≈ 625 J de tenue en surcharge courte ✅ marge ×70 |
| Timeout | 1,5 s → `FAULT_PRECHARGE` (§D.5) |
| FET de précharge | **Saturé, pas en linéaire** → dissipation négligeable |

📐 **Le FET de précharge est un P-canal 100 V**, source au `BUS+`, grille tirée 10 V plus bas par un petit N-FET et une zener. À 4,2 A avec ~100 mΩ, il dissipe 1,8 W pendant 0,5 s — sans objet. Un N-canal exigerait une seconde pompe de charge pour rien.

---

# AA.2 Le contrôleur : `TPS4810-Q1`

## AA.2.1 Pourquoi lui et pas un autre

La contrainte décisive est le **diagnostic de transistor collé**. Il exige de commander `Q1` et `Q2` **indépendamment** — sinon on ne peut pas ouvrir l'un pour tester l'autre.

| Composant | Plage | Sorties de grille | Diagnostic possible ? |
|---|---|---|---|
| **`TPS4810-Q1`** | 3,5 – 95 V | ✅ **2 indépendantes** (`G1PU`/`G1PD`, `G2`) + **2 entrées** (`INP1`, `INP2`) | ✅ **Oui** |
| `TPS4811-Q1` | 3,5 – 80 V | 2 sorties mais **une seule entrée `INP`** | ❌ Non |
| `TPS1210-Q1` | 3,5 – **40 V** | 2 indépendantes | ❌ **Écarté** : 45 V abs. max contre un bus à 42 V, aucune marge |
| `LTC7000` | 3,5 – 135 V | ❌ **une seule** | ❌ Non |
| `LTC4368` | 2,5 – 60 V | ❌ **une seule** | ❌ Non |
| `LM5069`, `TPS2492`, `LTC4260`, `LTC4364` | 9 – 80 V | ❌ une seule | ❌ Non |
| `TPS2663x` / `TPS1663x` | 4,5 – 60 V | FET intégré | ❌ **6 A maximum** |

🔴 **Il n'existe aucun eFuse à FET intégré au-dessus de 6 A dans la classe 40–80 V.** Au-delà, il faut obligatoirement un contrôleur à FET externes — et le `TPS4810-Q1` est **le seul** au-dessus de 40 V à piloter deux grilles indépendamment.

## AA.2.2 Caractéristiques ✅

| Paramètre | Valeur |
|---|---|
| Plage d'entrée `VS` | 3,5 – 95 V op. (🔴 sources TI contradictoires : 80 / 95 / 100 V — sans conséquence à 42 V) |
| Protection polarité inverse | **−65 V** |
| Courant de grille | 1,62 A source / 2 A sink |
| Pompe de charge | 11 V intégrée, ≈ 345 µA |
| `Iq` | 35 µA actif, 1 µA en veille |
| Seuil de court-circuit | `R_ISCP = (I_SC × R_SNS − 19 mV) / 2 µA` |
| Temps de réponse | **≈ 4,3 µs** au plus rapide |
| Mesure | shunt haut, shunt bas, **ou `V_DS` du MOSFET** (broche `CS_SEL`) |
| Défaut | `FLT` drain ouvert. 32 tentatives auto, ou latch par 100 kΩ sur `TMR` |
| ⭐ `SCP_TEST` | **Auto-test du comparateur de court-circuit** : simule un défaut en interne et vérifie que la grille et `FLT` retombent |
| Boîtier | VSSOP `DGX` 19 broches, 5,1 × 3,0 mm |

⭐ La broche `SCP_TEST` est un vrai cadeau : elle teste **l'organe de protection lui-même**, ce que le §K du dossier réclame pour le champignon et qu'on ne peut normalement pas faire sur une protection en courant.

⚠️ **Composant AEC-Q100 uniquement** — aucune version industrielle non-Q1. Ce n'est pas un problème (qualification automobile = plage étendue et déverminage), mais le prix et l'approvisionnement suivent.

## AA.2.3 ⚠️ Le conflit d'affectation, et sa résolution

TI utilise `INP1` pour `Q1`+`Q2` ensemble et `INP2` pour le FET de précharge `Q3`. Mais la table de diagnostic exige `INP1` et `INP2` sur `Q1` et `Q2` **séparément**. **Un seul `TPS4810-Q1` ne peut pas faire les deux.**

📐 **Résolution** : `INP1`/`INP2` sont dédiés au diagnostic, et le **FET de précharge est piloté directement par l'ESP32-SAFETY** via un petit étage P-canal. Cela coûte un GPIO — que la carte a — et cela préserve la fonction de sécurité la plus importante.

---

# AA.3 Les MOSFET de puissance

📐 **Bonne nouvelle collatérale** : la précharge n'étant plus faite en linéaire, **le transistor principal n'a plus besoin d'être un LinearFET**. Un trench standard, moins cher et à `RDS(on)` plus bas, convient.

**Retenu : 2 × `IPB017N10N5`**, D²PAK-7, 100 V, **1,7 mΩ** max à `VGS` = 10 V, `RthJC` 0,4 K/W ✅ ([datasheet](https://www.infineon.com/dgdl/Infineon-IPB017N10N5-DataSheet-v02_05-EN.pdf?fileId=5546d4624a75e5f1014ac4a981111eed)), en **tête-bêche source commune**.

## AA.3.1 Thermique — le cas réel, pas celui que j'avais posé

⚠️ **Correction d'un chiffre que j'avais mal cadré.** J'avais parlé de « 80 A de pointe ». C'est faux pour cet organe : les 83 A sont le chiffre de **coordination des fusibles et des câbles** (§C.3), pas ce que traverse l'interrupteur. Le budget imposé par l'ESP32-SAFETY est de **20 A continu / 30 A pendant ≤ 2 s** (§A.1-5). L'interrupteur est en aval du fusible et ne voit que le courant réel.

| Cas | Calcul (`RDS(on)` ≈ 2,6 mΩ à chaud 📐, **2 en série**) | Dissipation |
|---|---|---|
| 20 A continu (budget) | 20² × 2,6 mΩ × 2 | **2,1 W** |
| 30 A pendant 2 s (pointe) | 30² × 2,6 mΩ × 2 | **4,7 W** |
| 25 A (marge de conception) | 25² × 2,6 mΩ × 2 | 3,3 W |

**Ce que dit le datasheet sur `RthJA`** ✅ — et il faut lire les conditions :
- **62 K/W** en « minimal footprint »
- **40 K/W** avec *« 6 cm² (one layer, 70 µm thick) copper area for drain connection. PCB is vertical in still air. »*

📐 À 25 A, il faudrait `RthJA` ≤ (125 − 60)/1,63 = **39,9 K/W** — soit **exactement** les 40 K/W du datasheet, avec **zéro marge**, et dans des conditions (carte verticale, air libre) qui ne sont pas celles d'un coffret de robot.

⚠️ **Recommandation** : **10 à 12 cm² de plan de drain par transistor**, sur les deux faces, cousues d'une matrice de vias thermiques → `RthJA` ≈ 25 K/W → `Tj` ≈ **101 °C** à 60 °C ambiant, soit 24 K de marge. À 20 A de budget réel, la marge est encore plus large.

🔴 **Non calculable depuis le datasheet** : `Zth(j-a)` à 2 s et le coefficient `RDS(on)` vs `Tj` ne sont publiés qu'en courbes. Le ×1,5 à chaud est une hypothèse. **À relever sur les courbes avant de figer le plan de cuivre.**

## AA.3.2 Pourquoi tête-bêche, et pas un seul transistor

Sur un montage côté haut classique — drain à la batterie, source à la charge — **la diode de corps conduit de la charge vers la batterie**. Ouvrir un transistor unique arrêterait la décharge mais **laisserait passer le courant de régénération**.

Sur un robot à freinage régénératif, c'est disqualifiant : l'arrêt d'urgence n'empêcherait pas les moteurs de recharger le pack. D'où le **tête-bêche source commune**, qui bloque les deux sens.

⚠️ Et cela renforce le §C-S2 : si l'interrupteur bloque la régénération, **le hacheur de freinage devient indispensable**, puisque l'énergie n'a plus aucune autre issue.

---

# AA.4 Ouverture : le `di/dt` est le vrai piège

Couper 25 A dans un faisceau de 1,5 m (≈ 1,5 µH) en 100 ns donne `L·di/dt` = **375 V**. Le transistor tient 100 V. Il faut **ralentir volontairement l'ouverture**.

| `Rg` de coupure | `di/dt` | `V_L` | `V_DS` crête | Verdict |
|---|---|---|---|---|
| 1 Ω (driver nu) | 2080 A/µs | 3100 V | destruction | ❌ |
| 47 Ω | 45 A/µs | 67 V | 109 V | ❌ trop |
| **100 Ω** | 21 A/µs | 31 V | **73 V** | ✅ |
| **220 Ω** | 9,6 A/µs | 14 V | **56 V** | ✅ confortable |

📐 **Retenu : `Rg_off` = 100 à 220 Ω**, à valider à l'oscilloscope sur maquette — le `Qgd` réel décide, et il n'est publié qu'en courbe 🔴.

⚠️ **Compromis à assumer** : ce `Rg` ralentit aussi la coupure sur défaut. Le `TPS4810-Q1` détecte en 4,3 µs ; avec 220 Ω on ajoute ~3 µs de coupure. **Le TVS est la vraie protection, pas le `Rg`.**

**Clamps** :

| Élément | Rôle | Référence |
|---|---|---|
| TVS **drain-à-drain**, en travers de l'interrupteur complet | Écrête la pointe inductive | **`SMCJ48A`** — `VRWM` 48 V > 42 V (pas de conduction en service), `VC` **77,4 V** < 100 V `VDS` → **22 V de marge** ✅. Énergie à absorber ≈ **1 mJ** contre 1500 W crête : sans objet |
| **Diode de roue libre** du nœud de charge vers `GND`, cathode au nœud | Reprend les 469 µJ du faisceau côté charge | ⚠️ TI documente explicitement le transitoire **négatif** : *« the device can see transient negative voltages… due to output cable harness inductance kickbacks when the switches are turned OFF »* |

---

# AA.5 Détecter un transistor claqué

C'est la contrepartie obligatoire du choix statique : **un MOSFET stressé claque en court-circuit**, et un contacteur mécanique qui a fermé sur charge se soude. **Aucun des deux ne défaille en sécurité.**

## AA.5.1 Le principe

Mesurer le **nœud milieu** entre les deux transistors tête-bêche (`DIAG`), en commandant les grilles séparément.

⚠️ **`DIAG` n'est pas une broche du circuit intégré** — c'est un nœud de tension de **ta** carte, à ramener sur un diviseur et une entrée de l'ADS1115. Le nom vient de la note TI, pas du brochage.

**Table de diagnostic TI** (SLUAAS8, Table 3-1) :

| `INP1` | `INP2` | `V(DIAG)` attendu | Conclusion |
|---|---|---|---|
| H | L | Haut | `Q1` conduit — normal |
| **H** | **L** | **Bas** | ⚠️ **`Q1` collé OUVERT** |
| L | H | Haut | `Q2` conduit — normal |
| **L** | **H** | **Bas** | ⚠️ **`Q2` collé OUVERT** |

🔴 **Limite importante** : cette table détecte le **collé ouvert**, pas le **collé fermé** — qui est le cas qui nous intéresse. Le principe dual est évident (commander `INP1` = `INP2` = L et vérifier que `DIAG` s'effondre), **mais TI ne le tabule pas**. À dériver et à valider soi-même, avec une **résistance de tirage vers la masse sur le nœud `DIAG`** pour garantir un état bas défini quand tout est ouvert.

⚠️ Et ce test ne peut s'exécuter qu'une fois les 10 mF déchargés — d'où la **décharge active** déjà prévue au §D.8, qui devient ici une condition du diagnostic et plus seulement une commodité de maintenance.

## AA.5.2 Ce que le statique ne remplace pas

⚠️ **Un sectionneur statique seul ne fournit pas de coupure garantie sur défaut silicium.** La pratique industrielle EV conserve un organe électromécanique en série comme moyen de dernier recours. 🔴 *Je n'ai trouvé aucune note d'application constructeur énonçant cette règle explicitement — c'est une pratique de secteur, pas une source citable.*

📐 **Dans ton architecture, cet organe existe déjà** : le **coupe-batterie manuel** et le **fusible MRBF**, niveau N6 du §A.1. Le contacteur statique remplace le niveau N5 commandé, **pas** le sectionnement manuel. C'est la condition qui rend le choix acceptable, et elle doit être écrite noir sur blanc.

---

# AA.6 Ce que la carte gagne et ce qu'elle perd

| | Contacteur mécanique | **Contacteur statique retenu** |
|---|---|---|
| Coût | 80–200 € | ≈ **20 €** de composants |
| Composants externes | Contacteur + relais de précharge + bobine | **Résistance de précharge seule** |
| Consommation permanente | 2–8 W (bobine) | **≈ 4 mW** |
| Usure | Contacts qui piquent | Aucune |
| Isolation galvanique | ✅ | ❌ fuite de quelques µA |
| Vitesse de coupure sur défaut | ~10 ms | **4,3 µs** ⭐ |
| Auto-test de la protection | ❌ | ✅ `SCP_TEST` ⭐ |
| Diagnostic de collage | Mesure de `V_bus` | ✅ Par transistor, `DIAG` ⭐ |
| Blocage de la régénération | ✅ | ✅ (grâce au tête-bêche) |
| `di/dt` à l'ouverture | Sans objet | ⚠️ À maîtriser — `Rg` + TVS |
| Défaillance | Se soude fermé | Claque en court-circuit |

📐 **Sur les trois critères qui comptent pour une chaîne de sécurité — vitesse de coupure, auto-test, diagnostic —, le statique gagne nettement.** Sur l'isolation galvanique il perd, et c'est le coupe-batterie manuel qui compense.

---

# AA.7 Nomenclature du bloc

| Fonction | Référence | Note |
|---|---|---|
| Contrôleur | **`TPS4810-Q1`** (VSSOP DGX 19 br.) | Seul >40 V à deux grilles indépendantes |
| MOSFET principaux | **2 × `IPB017N10N5`** D²PAK-7, 100 V, 1,7 mΩ | Tête-bêche source commune. ⚠️ 10–12 cm² de cuivre chacun |
| Shunt de mesure | 1 mΩ, 4 fils | Vers `CS+`/`CS−` du TPS4810 |
| TVS d'ouverture | **`SMCJ48A`** drain-à-drain | 22 V de marge sous `VDS` |
| Diode de roue libre | Schottky 100 V, charge → GND | Transitoire négatif du faisceau |
| `Rg` de coupure | 100–220 Ω | 🔴 à valider à l'oscilloscope |
| FET de précharge | P-canal 100 V, ~100 mΩ | Piloté par l'ESP32-SAFETY, **pas** par le TPS4810 |
| Résistance de précharge | **`RH02510R00FE02`** 10 Ω / 25 W | Hors carte, inchangée par rapport au §D.5 |
| Tirage `DIAG` | 100 kΩ vers GND + diviseur → ADS1115 | Indispensable au diagnostic de collage |

---

# AA.8 Ce qui reste à vérifier

| # | Point | Pourquoi |
|---|---|---|
| 1 | **Brochage exact du `TPS4810-Q1`** — le boîtier est annoncé 19 broches mais la numérotation extraite va à 20 | 🔴 À lire sur le pinout du PDF avant routage |
| 2 | `Zth(j-a)` à 2 s et coefficient `RDS(on)` vs `Tj` | 🔴 Courbes seulement — décide le plan de cuivre |
| 3 | `Qgd` réel de l'`IPB017N10N5` | 🔴 Courbe seulement — décide `Rg_off` |
| 4 | **Table de diagnostic « collé fermé »** | 🔴 Non publiée par TI — à dériver et valider sur maquette |
| 5 | **M10 — capacité réelle du bus** | Les 10 mF sont une hypothèse ; ils fixent la précharge **et** l'énergie de diagnostic |
| 6 | Disponibilité et prix réels du `TPS4810-Q1` | AEC-Q100 uniquement, à vérifier chez l'assembleur avant de figer le schéma |

---

*Section AA du dossier. Remplace les blocs « contacteur » et « précharge » de `icd.yaml` (J10, J13) et les §D.3-④/⑤ du dossier v1.1.*
