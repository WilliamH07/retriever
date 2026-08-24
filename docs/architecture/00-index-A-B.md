# Dossier d'architecture — Robot mobile 4WD « Retriever »

**Plateforme** : châssis 4 roues motrices, extérieur, 20–60 kg
**Calculateur** : Youyeetoo X1 (Intel Celeron N5105) — Ubuntu 24.04 — ROS 2 Jazzy
**Objectif 3–6 mois** : navigation autonome Nav2
**Version du dossier** : 1.1 — 10 août 2026 *(v1.0 le 8 août ; ajout de la section W — interface opérateur)*
**Auteur** : architecture établie avec Claude (Cowork), sur la base d'une recherche documentaire sourcée

---

## Comment lire ce dossier

| Marqueur | Signification |
|---|---|
| ✅ **VÉRIFIÉ** | Valeur issue d'une datasheet constructeur ou d'une norme. Utilisable telle quelle. |
| 🟡 **CONSENSUS** | Valeur issue de rétro-ingénierie communautaire recoupée. Fiable mais non contractuelle. |
| 🔴 **À MESURER** | Valeur inconnue ou contradictoire. **Interdiction de câbler avant mesure.** |
| ⚠️ **RISQUE** | Point de conception dangereux si mal traité. |
| 📐 **HYPOTHÈSE** | Valeur que j'ai choisie faute de donnée ; à confirmer par le projet. |

### Fichiers du dossier

| Fichier | Sections |
|---|---|
| `00-index-A-B.md` | Index · **A** Résumé · **B** Hypothèses et informations manquantes |
| `01-inventaire-electrique.md` | **C** Inventaire · **D** Architecture électrique · **E** Busbars |
| `02-comms-esp32-moteurs.md` | **F** Communications · **G** ESP32 · **H** Moteurs |
| `03-ros2-etats-boot.md` | **I** Architecture ROS 2 · **J** Machine à états · **K** Boot & diagnostics |
| `04-securite-watchdogs-capteurs.md` | **L** Sécurité · **M** Watchdogs & pannes · **N** Capteurs & localisation |
| `05-reseau-code-tests-physique.md` | **O** Réseau · **P** Organisation du code · **Q** Tests · **R** Architecture physique |
| `06-bom-fmea-schema-plan.md` | **S** BOM · **T** FMEA · **U** Schéma global · **V** Plan de réalisation |
| `07-interface-operateur.md` | **W** Interface opérateur — pupitre Nintendo Switch *(ajout du 10 août 2026)* |

---

# A. Résumé de l'architecture recommandée

## A.1 Les cinq décisions structurantes

### 1. Le bus temps réel est un **bus CAN**, pas de l'USB

L'architecture initialement esquissée (« tout remonte au X1 par USB ») est rejetée. L'USB est un bus maître-esclave, à énumération dynamique, sensible aux perturbations électromagnétiques et sans priorité de trames. Sur un robot extérieur avec quatre moteurs BLDC hachant du courant à quelques dizaines d'ampères à moins de 30 cm des câbles de signal, c'est le mauvais choix pour le chemin de commande.

**Retenu** : un bus **CAN 2.0A à 500 kbit/s** relie le X1 et tous les microcontrôleurs. Le X1 n'ayant pas de contrôleur CAN natif (✅ vérifié : CAN uniquement via module d'extension), on utilise un **adaptateur USB-CAN à firmware `candleLight`/`gs_usb`**, reconnu nativement par le noyau Linux comme interface **SocketCAN** `can0`. Côté ROS 2, `ros2_socketcan` est packagé pour Jazzy.

L'USB reste utilisé, mais **uniquement pour les capteurs à gros débit ou nativement USB** : Kinect, lidar, GPS. Ce sont des flux de perception : leur perte dégrade la mission, elle ne provoque pas de mouvement dangereux.

### 2. La sécurité est **matérielle et indépendante du X1**

Cinq niveaux d'arrêt, du plus rapide au plus autoritaire. Le niveau 5 ne contient **aucune ligne de code**.

```
┌────────────────────────────────────────────────────────────────────────┐
│ N1  ROS 2 : nav2_collision_monitor + cmd_vel_timeout        ~100 ms    │
│ N2  ros2_control : SystemInterface renvoie ERROR             ~50 ms    │
│ N3  ESP32-MOTION : pas de trame CAN depuis 150 ms → VR=0     ~150 ms   │
│ N4  ESP32-SAFETY : surintensité / perte heartbeat            ~20 ms    │
│         → ligne matérielle /SAFE tirée à la masse                      │
│         → entrées STOP + EL des 4 ZS-X11H                              │
│ N5  BOUCLE MATÉRIELLE : champignon NF en série                ~10 ms   │
│         → ligne /SAFE directement                                      │
│         → puis, après temporisation RC ~1 s, bobine du contacteur DC   │
│ ────────────────────────────────────────────────────────────────────── │
│ N6  Coupe-batterie manuel + fusible principal (dernier recours)        │
└────────────────────────────────────────────────────────────────────────┘
```

**Le dernier niveau capable d'arrêter physiquement les moteurs est le contacteur DC principal**, dont la bobine est alimentée à travers le contact NF du champignon d'arrêt d'urgence. Aucun logiciel, aucun microcontrôleur, aucune alimentation logique ne peut le maintenir fermé si le champignon est enfoncé.

⚠️ **Point de conception critique** : couper la puissance à un ZS-X11H met le moteur en roue libre — **pas de freinage**. Sur une pente, cela aggrave la situation. C'est pourquoi la boucle d'arrêt d'urgence agit en deux temps : `/SAFE` déclenche d'abord le **freinage électrique** des variateurs, et le contacteur ne s'ouvre qu'après une temporisation matérielle (~1 s). Voir §L.

### 3. Le X1 ne fait **jamais** de temps réel

| X1 (Ubuntu 24.04, non temps réel) | ESP32 (bare-metal / FreeRTOS) |
|---|---|
| Perception, SLAM/localisation, Nav2 | Boucle de commande moteur 200 Hz |
| Machine à états **de mission** | Machine à états **de sécurité** (autoritaire) |
| Diagnostics, journalisation, télémétrie | Watchdogs, timeouts, limites de courant |
| Interface opérateur distante | Séquence de précharge, pilotage contacteur |
| Génère des **consignes** | Décide de l'**autorisation** de les exécuter |

Le X1 *demande* l'armement ; l'ESP32-SAFETY l'*accorde* ou le refuse. Cette inversion est le cœur de l'architecture : un bug ROS 2, un OOM-killer, un noyau bloqué ou un `colcon build` malencontreux ne peuvent pas produire de mouvement.

### 4. Le parc de capteurs actuel est un **parc d'intérieur** — et l'objectif est l'extérieur

C'est la conclusion la plus inconfortable de l'analyse, et il faut la poser franchement.

| Capteur | Verdict extérieur | Fait |
|---|---|---|
| **YDLIDAR X4** | ❌ **Inadapté** | ✅ Datasheet : environnement de test **0/550/2000 lux**, plage **0–40 °C**, **aucun indice IP**, portée spécifiée « en intérieur ». Le plein soleil c'est 10 000 à 100 000 lux : le récepteur sature. |
| **Kinect v2** | ❌ **Inadapté** | ✅ Profondeur par IR actif (time-of-flight) : inutilisable au soleil. ✅ Alimentation secteur 12 V / 2,67 A obligatoire. ✅ USB 3.0 dédié obligatoire (le X1 n'en a que 2). ✅ Pipeline CPU = ~200 ms/trame → OpenCL obligatoire. libfreenect2 sans release depuis 2021, aucun driver ROS 2 Jazzy. |
| **BNO085** | ✅ Bon choix | Fusion embarquée, quaternion 400 Hz. |
| **GPS USB** | ✅ Nécessaire | Mais insuffisant seul pour le cap. |
| **ACS758LCB-100B** | 🟡 Utilisable, mais pas pour ce qu'on croit | Voir §A.5. |

**Ce que je recommande de faire, sans jeter le matériel** :

- **Phase 1–2 (banc + intérieur)** : utiliser YDLIDAR X4 et Kinect v2 tels quels. Ils sont parfaits pour développer et valider toute la pile logicielle en intérieur, avec `slam_toolbox` et Nav2.
- **Phase 3 (extérieur)** : basculer la localisation sur **GPS + IMU + odométrie roues** (double EKF `robot_localization`), sans SLAM 2D. Le lidar X4 reste utile en **détection d'obstacles locale par temps couvert**, jamais comme source de localisation.
- **Phase 4** : remplacer par un lidar IP65+ à immunité solaire spécifiée (Hokuyo UST-10LX, RPLIDAR série S/T) ou une caméra stéréo extérieure.

L'architecture logicielle est conçue pour que ce remplacement soit **un changement de fichier de lancement**, pas une réécriture.

### 5. La batterie M365 ne peut pas alimenter quatre moteurs hoverboard à pleine puissance

C'est le second point inconfortable, et il est chiffré.

| Grandeur | Valeur | Source |
|---|---|---|
| Pack M365 | 10S3P, 7,8 Ah, **280 Wh**, 36 V nom / 42 V max | ✅ manuel Xiaomi (280 Wh, 36 V, 42 V) + 🟡 10S3P/7,8 Ah (analyse matérielle publiée) |
| Courant continu max du BMS d'origine | 🔴 **INCONNU — non publié** | Seule valeur approchante : 33 A continu / 133 A détection court-circuit sur un **BMS de remplacement tiers**, pas le BMS Xiaomi |
| 4 × ZS-X11H, nominal | 4 × 16 A = **64 A** | 🟡 fiches revendeurs concordantes |
| 4 × ZS-X11H, pointe | 4 × 20 A = **80 A** | 🟡 idem |
| 4 × moteur hoverboard 250 W | ≈ 1000 W ≈ **28 A à 36 V** | 🟡 |

Autrement dit : la demande crête de la motorisation dépasse d'un facteur 2 à 3 ce que le pack peut vraisemblablement fournir, et **on ne connaît même pas le seuil réel de coupure du BMS**. Une coupure BMS en pleine manœuvre = perte instantanée de toute la puissance et de tout le calculateur.

**Décision d'architecture** : le système impose un **budget de courant global**, appliqué par l'ESP32-SAFETY à partir de la mesure ACS758, avec limitation par saturation des consignes avant d'atteindre le seuil BMS.

| Budget | Valeur 📐 | Justification |
|---|---|---|
| Courant continu total | **20 A** (720 W à 36 V) | Sous le seuil BMS présumé avec marge de 33 % |
| Pointe autorisée | **30 A pendant ≤ 2 s** | Franchissement d'obstacle, démarrage en côte |
| Seuil de bridage progressif | 22 A | Réduction proportionnelle des consignes des 4 roues |
| Seuil de coupure | 35 A ou 30 A > 3 s | `/SAFE` |
| Autonomie estimée à 200 W moyens | ≈ **1 h 10** (238 Wh utiles) | 📐 hypothèse 85 % de profondeur de décharge |

**Ces valeurs sont provisoires** : elles doivent être recalées après la mesure n°1 du §B.2 (seuil réel du BMS). Si le BMS coupe à 25 A, le budget descend à 15 A et il faudra soit se limiter à 2 roues motrices, soit changer de pack.

⚠️ **Second risque batterie, souvent oublié** : le **freinage régénératif**. Les ZS-X11H renvoient du courant vers le pack au freinage. Sur un pack à 42 V (pleine charge), le BMS coupe en surtension — et la communauté M365 rapporte des cartes ESC détruites au-delà de ~30 A de régénération. Voir §H.6.

## A.2 Vue d'ensemble en une image

```
                     ╔══════════════════════════════════════════════╗
                     ║          POSTE OPÉRATEUR DISTANT             ║
                     ║   RViz2 · Foxglove · SSH · manette           ║
                     ╚═══════════════════┬══════════════════════════╝
                                         │  WireGuard (UDP 51820)
                                         │  jamais de DDS nu sur Internet
                     ╔═══════════════════▼══════════════════════════╗
                     ║      Wi-Fi / Ethernet — réseau du robot      ║
                     ╚═══════════════════┬══════════════════════════╝
   ┌─────────────────────────────────────▼──────────────────────────────────────┐
   │  YOUYEETOO X1  ·  Ubuntu 24.04  ·  ROS 2 Jazzy      [NON TEMPS RÉEL]       │
   │  Nav2 · robot_localization · slam_toolbox · diagnostics · mission          │
   │  ros2_control (controller_manager 100 Hz) + retriever_hardware (SocketCAN) │
   └───┬────────────────────┬───────────────────────┬───────────────────────────┘
       │ USB 3.0            │ USB 2.0               │ USB → SocketCAN can0
       │                    │                       │  (adaptateur gs_usb)
   ┌───▼─────┐    ┌─────────▼─────────┐   ══════════▼══════════════════════
   │ Kinect  │    │ YDLIDAR X4 (USB)  │    B U S   C A N   500 kbit/s
   │ v2 †    │    │ GPS USB           │   ═══╤═════════╤═════════╤═════════
   └─────────┘    └───────────────────┘      │         │         │
                                         ┌───▼───┐ ┌───▼───┐ ┌───▼────────┐
                                         │ESP32  │ │ESP32  │ │  ESP32     │
                                         │MOTION │ │MOTION │ │  SAFETY    │
                                         │ AVANT │ │ARRIÈRE│ │  + POWER   │
                                         └─┬───┬─┘ └─┬───┬─┘ └─┬────┬───┬─┘
                                           │   │     │   │     │    │   │
                                       ZS-X11H ×2  ZS-X11H ×2  │    │  BNO085
                                           │   │     │   │     │    │  (SPI)
                                        Moteur-roue hoverboard │    │
                                           ×4                  │    └─ ventilos
                                                               │       PWM 12 V
                       ┌───────────────────────────────────────┘
                       │ ACS758 · V_bus · contacteur · précharge
                       │ ligne matérielle /SAFE · UART BMS M365
                       ▼
   ══════════════════ CHAÎNE DE PUISSANCE ══════════════════════════════════
   BAT M365 ─ coupe-batterie ─ MRBF 80 A ─ shunt/ACS758 ─ contacteur DC ─┐
                                              précharge ──────────────────┤
                                                                 BUSBAR + │
        ┌──────────┬──────────┬──────────┬────────────┬──────────┬────────┘
     MIDI 25A   MIDI 25A   MIDI 25A   MIDI 25A     MIDI 10A   MIDI 5A
        │          │          │          │            │          │
     ZS-X11H    ZS-X11H    ZS-X11H    ZS-X11H     DC/DC 12 V  DC/DC 12 V
      (AV-G)     (AV-D)     (AR-G)     (AR-D)     COMPUTE      AUX
                                                     │           │
                                              X1 + Kinect   ventilos,
                                                  + hub USB  bobine,
                                                             DC/DC 5 V
   ══════════════════════════════════════════════════════════════════════════
```

## A.3 Ce qui change par rapport à l'esquisse initiale

| Esquisse initiale | Architecture retenue | Pourquoi |
|---|---|---|
| ESP32 ↔ X1 par USB | **Bus CAN 500 kbit/s** | Immunité EMI, multipoint, priorités, ajout de nœuds sans recâbler |
| Un seul niveau ESP32 | **2 rôles distincts** : MOTION (×2) et SAFETY (×1) | La sécurité ne doit pas partager un CPU avec une boucle de commande |
| Disjoncteur 100 A en tête | **Fusible MRBF 80 A / 58 V DC** + coupe-batterie DC + contacteur | Le « 100 A » de GSB est un sectionneur sans protection, sans pouvoir de coupure DC déclaré |
| X1 = cerveau qui commande tout | X1 = cerveau qui **propose** ; ESP32-SAFETY **dispose** | Suppression du single point of failure logiciel |
| Kinect + lidar comme capteurs de navigation | Capteurs de **phase 1 intérieur** ; extérieur = GPS+IMU+odométrie | Contraintes physiques (soleil, IP, température) documentées |
| ACS758 pour l'estimation du SOC | ACS758 pour la **protection surintensité** ; SOC lu **dans le BMS M365 par UART** | Le protocole BMS est documenté (registre 0x31 : mAh, %, courant, tension, T°) |

## A.4 Ce que l'architecture permet d'ajouter sans recâbler

Critère explicite du cahier des charges. Le bus CAN, le busbar à points de connexion libres et le découpage en paquets ROS 2 permettent d'ajouter, sans toucher au reste :

- un 5ᵉ, 6ᵉ nœud CAN (bras, tourelle, capteur intelligent) — 2 fils + 1 dérivation busbar ;
- un lidar de remplacement — un fichier de lancement, la TF est déjà paramétrée ;
- un second pack batterie — le busbar est dimensionné avec 2 points de connexion libres ;
- une carte de contrôle moteur différente (ODrive, VESC) — l'interface `retriever_hardware` est derrière une abstraction CAN ;
- un e-stop radio — il s'insère en série dans la boucle matérielle existante.

## A.5 Trois corrections importantes sur le matériel existant

**ACS758LCB-100B — bon capteur, mauvais calibre, et NRND.**
✅ Datasheet Allegro Rev. 20 (17 mars 2025) : le composant est **« NOT FOR NEW DESIGN »** depuis mars 2025 (remplaçants ACS772/ACS773). Surtout : en version **-100B (bidirectionnelle)**, la sensibilité est de **20 mV/A**, la sortie au repos à **V_CC/2 = 2,5 V**, le bruit de **6 mV (≈0,3 A)** et la dérive d'offset sur la plage de température de **±20 mV, soit ±1 A**. Sur un courant réel de 5–20 A, l'erreur d'offset seule représente **5 à 20 %**.

→ **Il est inutilisable pour du comptage coulométrique (estimation du SOC).** Il est en revanche parfaitement adapté à ce dont on a réellement besoin : **détection rapide de surintensité et d'anomalie** (bande passante 120 kHz, temps de réponse 4 µs, isolation galvanique 4800 V). C'est le rôle que je lui donne. Sa version bidirectionnelle est même un atout : elle mesure le **courant de freinage régénératif**, ce qui est exactement le risque identifié en §A.1-5.

→ Pour le SOC, on lit le BMS.

**Le BMS M365 expose une télémétrie complète par UART — utilisons-la.**
🟡 Protocole Xiaomi/Ninebot documenté par rétro-ingénierie : trames `55 AA | L | D | T | payload | CK0 CK1`, adresse `0x22` (maître→BMS) / `0x25` (BMS→maître), **115200 bauds**. Registres utiles : `0x31` (capacité mAh, %, courant, tension pack, température), `0x40` (**les 10 tensions de cellule**), `0x1B` (nombre de cycles).

C'est infiniment mieux qu'une estimation par ACS758 : on obtient le SOC calculé par le BMS lui-même, la température, et surtout **le déséquilibre inter-cellules**, qui est le vrai indicateur de santé et de danger d'un pack lithium.
🔴 **À mesurer avant de câbler** : niveau logique du bus (3,3 V présumé — le STM8L et le STM32 de l'ESC l'imposent de facto, mais **aucune source ne l'écrit**).

**Le ZS-X11H ne fournit pas de retour de vitesse signé — il faut lire les capteurs Hall directement.**
🟡 La sortie `SC` de la carte bascule à chaque transition d'un des trois capteurs Hall, soit **90 transitions par tour** sur un moteur 6,5" (15 paires de pôles × 6). Mais c'est un train d'impulsions **non signé** : impossible de distinguer une roue qui recule d'une roue qui avance.

Pour une odométrie exploitable par Nav2, c'est disqualifiant. **Solution retenue** : dériver **deux des trois lignes Hall** de chaque moteur vers les compteurs matériels (PCNT) de l'ESP32, à travers un tampon 74LVC245. Les signaux Hall étant déphasés de 120°, un décodage de type quadrature donne le **sens** et **60 comptes par tour** — soit ≈ **8,6 mm de résolution linéaire** sur une roue de 165 mm. Détail en §H.4.

---

# B. Hypothèses et informations manquantes

## B.1 Hypothèses de conception que j'ai posées (à confirmer par toi)

| # | Hypothèse 📐 | Impact si fausse |
|---|---|---|
| H1 | Châssis **skid-steer** (4 roues fixes, virage par différentiel de vitesse), pas de direction | Change le contrôleur ros2_control et le modèle Nav2 |
| H2 | Masse totale en ordre de marche **≈ 35 kg** | Recalcul du couple nécessaire, du budget de courant et des rampes |
| H3 | Vitesse max visée **1,5 m/s** | Cohérent avec 578 tr/min mesurés à vide sous 36 V sur roue 6,5" → ≈ 5 m/s à vide ; on est très en dessous, c'est confortable |
| H4 | Le pack M365 est un **Classic (280 Wh)**, pas un Pro (474 Wh) | Le Pro double l'autonomie et probablement le courant admissible |
| H5 | Le GPS USB est un récepteur **NMEA générique** (u-blox ou équivalent), pas un RTK | Sans RTK, précision 2–5 m : suffisant pour du waypoint, pas pour du suivi de rang |
| H6 | Longueur maximale des câbles de puissance **≤ 1,5 m** | Au-delà, revoir les sections (§D.6) |
| H7 | Température ambiante interne du coffret de puissance **≤ 60 °C** | Déclassement des câbles recalculé |
| H8 | Pas d'exigence de certification (usage privé, pas de mise sur le marché) | Une mise sur le marché déclencherait la directive Machines et EN 60204-1 en version formelle |
| H9 | Utilisation **hors présence de tiers** pendant la phase de mise au point | Sinon, périmètre de sécurité et procédure d'exploitation à formaliser |
| H10 | Capacité de fabrication : perçage, taraudage, sertissage hydraulique **accessible** | Le sertissage 25 mm² exige une pince hydraulique à matrices hexagonales (§E.6) |

## B.2 Les mesures à faire **avant tout câblage de puissance**

Ces mesures ne sont pas optionnelles. Chacune bloque une décision de dimensionnement. Toutes se font **hors robot**, sur établi, avec des charges faibles.

| # | Mesure | Méthode | Décision qu'elle débloque | Bloque |
|---|---|---|---|---|
| **M1** | **Seuil de coupure en courant du BMS M365 d'origine** | Charge résistive croissante par paliers de 5 A sur un banc, mesure du courant à l'instant de la coupure. **Sous protection : fusible 40 A en série, extincteur classe D à proximité, pack à ≤50 % de SoC, hors habitation.** | Budget de courant global, calibre du fusible principal, nombre de roues motrices | §D, §H |
| **M2** | **Tension réelle du pack** à vide et sous 10 A, et **résistance interne** (ΔU/ΔI) | Multimètre + charge connue | Courant de court-circuit prospectif → pouvoir de coupure requis du fusible principal | §D.3 |
| **M3** | **Niveau logique et brochage du connecteur BMS 3 points** | Oscilloscope sur les lignes R/T pendant un dialogue ESC↔BMS, ou en interrogeant le BMS avec un adaptateur USB-UART 3,3 V isolé | Câblage de la liaison ESP32-SAFETY ↔ BMS | §D.9, §N.6 |
| **M4** | **Polarité des entrées `EL`/`BRAKE` et `STOP` du ZS-X11H** | Banc moteur seul, alimentation de labo limitée en courant à 3 A, 24 V. Tester actif-haut et actif-bas, chronométrer le temps d'arrêt | **Toute la chaîne de sécurité N3–N5.** Sources contradictoires : une source dit actif-haut 5 V, une autre dit « relier à la masse arrête le moteur » | §L, §H |
| **M5** | **Présence et état du cavalier J1** (mode PWM) sur chaque ZS-X11H | Inspection visuelle sous le dissipateur | Choix entre commande PWM et commande analogique par DAC | §H.3 |
| **M6** | **Nombre de paires de pôles réel** de tes moteurs | Compter les transitions de la sortie `SC` sur exactement un tour de roue (à la main, moteur non alimenté, halls alimentés en 5 V). 90 transitions ⇒ 15 paires de pôles | Constante d'odométrie | §H.4 |
| **M7** | **Tension et impédance de la sortie `SC`** | Oscilloscope, moteur tourné à la main | Choix du circuit d'adaptation (74LVC245 vs optocoupleur) | §H.4 |
| **M8** | **Courant à vide d'un ensemble moteur+variateur**, puis en charge sur roue au sol | Pince ampèremétrique DC, alimentation de labo | Validation du budget, dimensionnement des dérivations | §D.6 |
| **M9** | **Contrôleur USB 3.0 du X1** (`lspci -nn \| grep -i usb`) | Une commande | Le Kinect v2 ne fonctionne **pas** sur contrôleur ASMedia (✅ libfreenect2). Si ASMedia → le Kinect est mort avant d'avoir commencé | §N.4 |
| **M10** | **Capacité totale d'entrée du bus** (somme des condensateurs des 4 variateurs + filtrage) | Datasheet des condensateurs ou capacimètre carte hors tension | Résistance et durée de précharge | §D.7 |
| **M11** | **Tension d'alimentation acceptée par le GPS et son protocole** (`lsusb`, `cat /dev/ttyACM0`) | Une commande une fois branché | Rail d'alimentation, driver ROS 2 | §N.5 |
| **M12** | **Le ZS-X11H tolère-t-il 42 V ?** Sources contradictoires : 6–60 V ou 9–60 V | Lecture du marquage des condensateurs d'entrée (63 V attendu) | Va/ne va pas. Si les condensateurs sont marqués 50 V, **le pack chargé à 42 V est déjà à 84 % de leur tenue** — marginal | §D.4 |

## B.3 Les sept informations que je te demande

Ce sont les points que la documentation ne peut pas me donner et qui changent des choix d'architecture.

| # | Question | Pourquoi c'est bloquant |
|---|---|---|
| **Q1** | **Quelle est la masse cible du robot en ordre de marche, et la pente maximale qu'il doit franchir ?** | Le couple nécessaire, donc le courant, donc tout le dimensionnement de puissance et le budget d'autonomie. Une pente de 15 % à 35 kg change complètement les pointes de courant. |
| **Q2** | **As-tu déjà un châssis, ou est-il à concevoir ?** Dimensions, empattement, voie, garde au sol. | L'empattement et la voie entrent directement dans la cinématique skid-steer, dans l'URDF et dans le calibrage `wheel_separation_multiplier`. |
| **Q3** | **Le pack M365 est-il un Classic (280 Wh) ou un Pro (474 Wh) ? Est-il d'origine ou remplacé (BMS tiers) ?** | Le courant admissible, l'autonomie et le protocole BMS en dépendent. Un BMS tiers « Repair BMS gen4 » a des seuils documentés (33 A) ; le BMS Xiaomi non. |
| **Q4** | **Quelle est la référence exacte du GPS USB ?** | Détermine le driver ROS 2 (`nmea_navsat_driver` vs `ublox`), la fréquence, et si un heading double-antenne est envisageable plus tard. |
| **Q5** | **Combien d'ESP32 possèdes-tu exactement, et quels modèles ?** (ESP32 classique, S3, C3, WROOM/WROVER, carte de dev ?) | L'architecture en réclame **3**. Le modèle détermine : présence du contrôleur TWAI (CAN), nombre de DAC, nombre d'unités PCNT, RAM. L'ESP32-S3 **n'a pas de DAC** — cela change le circuit de commande des ZS-X11H. |
| **Q6** | **Acceptes-tu de percer/tarauder du cuivre et de sertir avec une pince hydraulique** (achat ou emprunt ~60–120 €) ? | Sinon il faut partir sur des busbars préfabriqués du commerce (Victron Lynx, Blue Sea), plus chers mais sûrs. Le sertissage 25 mm² à la pince manuelle de GSB est un point chaud garanti : **1 mΩ = 10 W à 100 A**. |
| **Q7** | **Quel est ton budget d'achat complémentaire, et acceptes-tu d'acheter hors Leroy Merlin** (fournisseurs marine/industriel) ? | **Leroy Merlin ne vend pas de barre plate en cuivre** (vérifié : seulement des profilés laiton — et le laiton conduit **3,6× moins bien** que le cuivre). Les protections DC correctes (MRBF, Class T, sectionneur DC) n'existent pas non plus en GSB. Voir §S. |

## B.4 Ce que j'ai refusé de faire

Par cohérence avec ta règle n°1 :

- Je n'ai **pas** donné de courant de coupure pour le BMS M365 d'origine : il n'est publié nulle part. Les « 30 A » qui circulent sur les forums ne sont sourçables sur aucun document.
- Je n'ai **pas** donné de couple ni de courant de calage pour les moteurs hoverboard : aucune source publiée ne les fournit. J'ai fourni une **estimation dérivée** (≈100 A de calage théorique, ≈8 N·m à 16 A) en la marquant explicitement comme un calcul de ma part à partir de mesures ODrive (R_phase ≈ 0,179 Ω, L ≈ 336 µH), pas comme une spécification.
- Je n'ai **pas** tranché la polarité des entrées `EL`/`STOP` du ZS-X11H : deux sources sérieuses se contredisent. C'est la mesure **M4**, et toute la chaîne de sécurité en dépend.
- Je n'ai **pas** proposé de « brancher pour voir ». Chaque essai du §Q est encadré par une protection et une limite de courant.

---

*Suite : `01-inventaire-electrique.md` — sections C, D, E.*
