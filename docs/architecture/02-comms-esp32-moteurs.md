# F. Architecture des communications

## F.1 Comparaison des options pour le lien X1 ↔ microcontrôleurs

C'est la décision d'architecture la plus structurante après la sécurité. Le cahier des charges demande explicitement de ne pas partir du principe que « tout doit passer par USB ». Voici la comparaison réelle.

| Critère | USB série (×3) | UART TTL direct (×3) | Wi-Fi / micro-ROS UDP | **CAN 500 kbit/s** | RS-485 multipoint |
|---|---|---|---|---|---|
| Immunité EMI près de 4 hacheurs BLDC | ❌ Faible | 🟡 Moyenne | n/a | ✅ **Excellente** (paire différentielle) | ✅ Excellente |
| Multipoint sur 2 fils | ❌ Non (1 câble/nœud) | ❌ Non | ✅ | ✅ **Oui** | ✅ Oui |
| Arbitrage / priorité de trames | ❌ | ❌ | ❌ | ✅ **Natif, non destructif** | ❌ |
| Détection d'erreur | CRC USB, mais réénumération sur défaut | ❌ à écrire | CRC | ✅ **CRC15 + ACK + confinement de défaut** | ❌ à écrire |
| Latence bornée | ❌ (polling hôte, réénumération) | ✅ | ❌ **Non bornée** | ✅ **Oui** | ✅ |
| Nœud défaillant isolé du bus | 🟡 | ✅ | ✅ | ✅ **Bus-off automatique** | 🟡 |
| Nombre de ports X1 requis | 3 USB | 3 UART (il y en a 3 ✅) | 0 | **1 USB** | 1 UART + transceiver |
| Ajout d'un 4ᵉ nœud | ❌ nouveau câble + port | ❌ plus de port libre | ✅ | ✅ **dérivation** | ✅ |
| Support Linux natif | ✅ CDC/FTDI | ✅ `/dev/ttyS*` | ✅ | ✅ **SocketCAN (in-kernel)** | ✅ |
| Support ROS 2 Jazzy | générique | générique | micro-ROS | ✅ **`ros2_socketcan` packagé** | générique |
| Coût matériel | ~0 € | ~0 € | ~0 € | **~25 €** (adaptateur) + 3 × 2 € (transceivers) | ~15 € |
| Complexité firmware | Faible | Faible | **Élevée** | Moyenne | Moyenne |

### Pourquoi je rejette Wi-Fi / micro-ROS sur le chemin de commande

micro-ROS est un projet sérieux, officiellement releasé pour Jazzy, et l'ESP32 fait partie de ses plateformes supportées avec LTS garanti. Ce n'est pas une question de qualité. Ce sont trois faits qui le disqualifient **pour la boucle de commande moteur** :

1. **Le disclaimer des mainteneurs eux-mêmes** : « *not ready for production use* », affiché sur les dépôts.
2. **La reconnexion n'est pas automatique.** Le pattern officiel est une machine à états à écrire soi-même (`WAITING_AGENT` → `AGENT_AVAILABLE` → `AGENT_CONNECTED` → `AGENT_DISCONNECTED`). Pire : `rmw_uros_ping_agent()` est **bloquant** (bug documenté) — l'appeler dans la boucle de contrôle y introduit un délai.
3. **Les pools mémoire sont statiques et figés à la compilation.** Les valeurs par défaut sont de **5 publishers / 5 subscribers / 5 services**. Un nœud MOTION doit publier 2 encodeurs + 2 états moteur + diagnostics + heartbeat et souscrire à 2 consignes + sécurité : on dépasse. Avec `micro_ros_arduino` (bibliothèque **précompilée**) on ne peut même pas les augmenter — il faudrait passer par PlatformIO ou ESP-IDF.

À quoi micro-ROS **peut** servir dans ce projet : un futur nœud de télémétrie non critique (capteur météo, bras), là où la latence n'a pas d'importance. Et le paquet peu connu **`micro_ros_diagnostic_bridge`** (packagé pour Jazzy) permettrait de remonter des diagnostics compacts. C'est noté comme évolution possible, pas comme base.

### Pourquoi CAN plutôt que UART direct

L'UART direct (3 UART natifs 3,3 V sur le X1, ✅ vérifié) est une option honnête et gratuite. Je ne la retiens pas pour trois raisons :

- **Trois liaisons point-à-point, ce sont trois protocoles à maintenir, trois watchdogs, trois modes de défaillance.** Un bus, c'est un protocole.
- **Le X1 n'a que 3 UART.** L'architecture en consomme immédiatement 3. Aucune évolution possible.
- **Un signal TTL asymétrique de 1 m qui longe des câbles de phase moteur, c'est un cas d'école de couplage capacitif.** Le CAN est différentiel : le bruit de mode commun est rejeté.

Le CAN coûte ~30 € au total et résout tout ça. **C'est le choix retenu.**

### Comment le CAN arrive sur le X1

Le X1 **n'a aucun contrôleur CAN natif** (✅ vérifié — CAN uniquement via module d'extension Youyeetoo sur UART). Trois voies :

| Voie | Verdict |
|---|---|
| Module CAN Youyeetoo sur UART | ❌ Protocole propriétaire encapsulé sur série : pas de SocketCAN, pas de `ros2_socketcan`, driver à écrire |
| Adaptateur USB-CAN type Waveshare USB-CAN-A | ❌ Protocole série custom, pas SocketCAN |
| **Adaptateur USB-CAN à firmware `candleLight` / `gs_usb`** (CANable 2.0, USB2CAN, Innomaker) | ✅ **Retenu** — reconnu **nativement par le noyau Linux** comme interface réseau `can0`. `ip link set can0 up type can bitrate 500000` et c'est fini. Aucun driver à installer. |

⚠️ Vérifier à l'achat que le firmware est bien `candleLight`/`gs_usb` (parfois vendu sous « slcan » — le mode slcan fonctionne aussi mais passe par un démon `slcand` en espace utilisateur, moins performant et moins robuste).

## F.2 Topologie physique du bus CAN

```
   X1 ─USB─► [USB-CAN gs_usb] ═══╤══════════╤══════════╤═══════╗
                    │            │          │          │       ║
                   120Ω      ESP32-SAFETY ESP32-MOT  ESP32-MOT 120Ω
                terminaison                  AVANT     ARRIÈRE  terminaison
                  (extrémité)                                  (extrémité)

   Câble : paire torsadée blindée 2 × 0,34 mm² (CANH/CANL)
           + 2 × 0,5 mm² (5V_L / GND) dans la même gaine
   Longueur totale ≤ 2 m → 500 kbit/s largement dans les marges
   Dérivations (stubs) ≤ 15 cm
   Terminaisons : 120 Ω UNIQUEMENT aux deux extrémités physiques
   Blindage : relié au busbar − D'UN SEUL CÔTÉ (côté X1)
```

Chaque ESP32 nécessite un **transceiver CAN 3,3 V** : `SN65HVD230` ou `TCAN332`. ⚠️ **Ne pas utiliser de MCP2551** (5 V, niveaux incompatibles avec l'ESP32 sans adaptation).

## F.3 Plan d'adressage et de trames CAN

Identifiants 11 bits (CAN 2.0A). Rappel : **plus l'identifiant est petit, plus la priorité est haute** (arbitrage non destructif).

| ID | Émetteur | Contenu | DLC | Fréquence | Priorité |
|---|---|---|---|---|---|
| `0x010` | ESP32-SAFETY | **SAFETY_STATE** : état FSM, cause, flags, compteur | 8 | **100 Hz** | 🔴 Max |
| `0x020` | X1 | **ESTOP_REQUEST** : demande d'arrêt logiciel | 1 | événementiel | 🔴 |
| `0x100` | X1 | **CMD_WHEELS_FRONT** : vitesse AV-G, AV-D (int16, mrad/s) + seq + CRC8 | 6 | **50 Hz** | 🟠 |
| `0x101` | X1 | **CMD_WHEELS_REAR** : idem AR | 6 | **50 Hz** | 🟠 |
| `0x180` | ESP32-MOT-AV | **FB_WHEELS_FRONT** : vitesse mesurée ×2 (int16) + position ×2 (int16 delta) | 8 | **50 Hz** | 🟠 |
| `0x181` | ESP32-MOT-AR | idem AR | 8 | **50 Hz** | 🟠 |
| `0x190` | ESP32-MOT-AV | **MOT_STATUS_FRONT** : flags par roue (stall, timeout, hall_fault), T° si dispo | 4 | 10 Hz | 🟡 |
| `0x191` | ESP32-MOT-AR | idem | 4 | 10 Hz | 🟡 |
| `0x200` | ESP32-SAFETY | **POWER** : V_bus (mV), I_bus (mA signé), V_pack | 8 | **20 Hz** | 🟡 |
| `0x201` | ESP32-SAFETY | **BATTERY** : SOC %, courant BMS, T° BMS, cycles | 8 | 2 Hz | 🟢 |
| `0x202` | ESP32-SAFETY | **CELLS_A** : tensions cellules 1–4 | 8 | 0,2 Hz | 🟢 |
| `0x203` | ESP32-SAFETY | **CELLS_B** : cellules 5–8 | 8 | 0,2 Hz | 🟢 |
| `0x204` | ESP32-SAFETY | **CELLS_C** : cellules 9–10 + min/max/delta | 8 | 0,2 Hz | 🟢 |
| `0x210` | ESP32-SAFETY | **IMU_QUAT** : quaternion w,x,y,z (int16 normalisés) | 8 | **100 Hz** | 🟠 |
| `0x211` | ESP32-SAFETY | **IMU_GYRO_ACC** : gyro xyz + accel xyz (int16) | 8+8 → 2 trames | **100 Hz** | 🟠 |
| `0x220` | ESP32-SAFETY | **THERMAL** : T° dissipateur ×2, PWM ventilos, tach ×2 | 8 | 1 Hz | 🟢 |
| `0x300` | X1 | **TIME_SYNC** : horodatage ROS (µs, uint64) | 8 | **1 Hz** | 🟢 |
| `0x310` | X1 | **ARM_REQUEST** : demande d'armement / désarmement | 2 | événementiel | 🟡 |
| `0x320` | X1 | **CONFIG** : paramètres (limites, rampes) — write-once au boot | 8 | boot | 🟢 |
| `0x700+n` | tous | **HEARTBEAT** nœud n : état, uptime, compteur d'erreurs CAN | 4 | **10 Hz** | 🟢 |

**Charge du bus** : 100 (sécurité) + 200 (commandes+retours) + 300 (IMU) + 20 (puissance) + 30 (heartbeats) + divers ≈ **675 trames/s**. À ~110 bits par trame et 500 kbit/s : **≈ 15 % de charge**. Marge très confortable pour l'évolution.

**Une seule source de vérité pour le protocole** : un fichier `retriever_can/protocol.yaml` décrit tous les identifiants, champs, échelles et unités. Un script de génération produit :
- `retriever_can/include/retriever_can/protocol.hpp` (C++, pour ROS 2),
- `firmware/common/protocol.h` (C, pour les ESP32),
- la documentation.

C'est ce qui empêche la dérive silencieuse entre firmware et logiciel — le mode de défaillance le plus pénible de ce genre de projet.

## F.4 Synchronisation temporelle

Les ESP32 horodatent leurs mesures avec leur compteur µs local. Pour que ROS 2 puisse les fusionner (EKF, TF), il faut convertir ces horodatages en temps ROS.

```
1 Hz : le X1 émet TIME_SYNC (0x300) contenant son now() en µs
       ↓
       Chaque ESP32 mémorise (t_ros_reçu, t_local_à_la_réception)
       ↓
       Le X1 maintient, pour chaque nœud, une régression linéaire
       glissante t_ros = a·t_local + b (offset + dérive d'horloge)
       ↓
       retriever_hardware convertit les horodatages des trames de retour
```

📐 Précision attendue : **1–2 ms**, largement suffisant pour un EKF à 30 Hz et une odométrie à 50 Hz. Le facteur limitant n'est pas l'algorithme mais la gigue d'interruption de réception CAN côté ESP32 (~100 µs).

## F.5 Tableau des liaisons — fiche par lien

### L1 — X1 → adaptateur USB-CAN

```
A → B          : Youyeetoo X1 (USB 2.0) → adaptateur gs_usb
Interface      : USB 2.0 Full Speed
Protocole      : gs_usb (pilote noyau Linux) → SocketCAN can0
Débit          : 500 kbit/s côté CAN
Alimentation   : bus USB (5 V, <100 mA)
Connectique    : USB-A → USB-C, câble court (≤ 30 cm) blindé + ferrite
Longueur       : 0,3 m
Criticité      : C2 — sa perte arrête la mission
Fréquence      : continue
Gestion erreur : compteurs CAN via `ip -details -statistics link show can0`
Perte          : retriever_hardware → read() retourne ERROR → controller_manager
                 désactive tous les contrôleurs. En parallèle, les ESP32 ne
                 reçoivent plus de CMD → watchdog N3 → VR=0 + frein à 150 ms.
Récupération   : `restart-ms 100` sur l'interface CAN (auto-recovery bus-off)
                 + relance du controller_manager via lifecycle
```

### L2 — Bus CAN (X1 ↔ 3 × ESP32)

```
A → B          : multipoint
Interface      : CAN 2.0A, ISO 11898-2
Protocole      : trames propriétaires retriever (§F.3)
Débit          : 500 kbit/s
Alimentation   : 5V_LOGIC véhiculé dans le même câble
Connectique    : Molex Micro-Fit 3.0, 4 positions, détrompé
Longueur       : ≤ 2 m total, stubs ≤ 15 cm
Criticité      : C1
Fréquence      : ~675 trames/s
Gestion erreur : CRC15 + ACK + retransmission automatique + confinement
                 (error-active → error-passive → bus-off)
Perte          : chaque nœud a son propre watchdog local. Perte totale du bus
                 → chaque ESP32-MOTION coupe ses moteurs sous 150 ms ;
                 ESP32-SAFETY passe en FAULT et ouvre le contacteur après freinage.
```

### L3 — ESP32-MOTION → ZS-X11H (×2 par nœud)

```
A → B          : ESP32-MOTION → variateur
Interface      : 4 signaux
                 VR    : analogique 0–5 V   (DAC MCP4728 + ampli op)
                 DIR   : logique, collecteur ouvert (MOSFET 2N7002)
                 EL    : logique, collecteur ouvert  🔴 polarité à mesurer (M4)
                 STOP  : logique, collecteur ouvert  🔴 idem
Protocole      : aucun (signaux directs)
Débit          : mise à jour de VR à 200 Hz
Alimentation   : le variateur fournit un 5 V ≤ 30 mA 🔴 — NE PAS s'en servir
                 pour alimenter l'ESP32. Utilisé seulement comme référence
                 de niveau pour l'étage de sortie.
Connectique    : bornier à vis + JST ; ⚠️ ajouter une reprise mécanique (collier)
Longueur       : ≤ 40 cm
Criticité      : C1
Fréquence      : 200 Hz
Gestion erreur : aucune (lien analogique) → plausibilité par la mesure de vitesse
Perte          : fil VR coupé → entrée flottante → ⚠️ COMPORTEMENT INDÉFINI.
                 → MITIGATION OBLIGATOIRE : résistance de rappel 10 kΩ vers GND
                 sur l'entrée VR, au plus près du variateur. Un fil coupé
                 donne alors consigne = 0.
```

⚠️ Ce dernier point est important et facile à oublier : **une entrée analogique flottante n'est pas à zéro**. Le rappel à la masse est ce qui transforme une rupture de fil en arrêt plutôt qu'en emballement.

### L4 — ZS-X11H / moteur → ESP32-MOTION (retour Hall)

```
A → B          : capteurs Hall (2 sur 3) → compteurs PCNT de l'ESP32
Interface      : logique 5 V (collecteur ouvert + pull-up sur le variateur)
                 → tampon 74LVC245 alimenté en 3,3 V (entrées 5 V tolérantes)
Protocole      : décodage de type quadrature
Débit          : à 1,5 m/s → 174 tr/min × 15 pp = 43 Hz par ligne Hall
                 (soit ~87 transitions/s, ~174 comptes/s après décodage x4)
Alimentation   : 5 V fourni par le variateur au moteur (ne pas doubler)
Connectique    : dérivation en Y sur le connecteur Hall JST 5 points
Longueur       : ≤ 40 cm, torsadé, blindé
Criticité      : C1 (odométrie + détection de calage)
Fréquence      : événementiel, jusqu'à ~300 Hz par roue
Gestion erreur : plausibilité — si commande ≠ 0 et comptage = 0 pendant 300 ms
                 → drapeau STALL
Perte          : odométrie de la roue perdue → DEGRADED, la roue est bridée
```

### L5 — ESP32-SAFETY ↔ BMS M365

```
A → B          : bidirectionnel, ESP32 maître
Interface      : UART, 115200 bauds, 8N1 présumé  🔴 niveau logique à mesurer (M3)
                 ⚠️ ISOLATION GALVANIQUE OBLIGATOIRE (ADuM1201 ou optocoupleur rapide)
                 — le connecteur BMS n'a pas de masse dédiée
Protocole      : Xiaomi/Ninebot — 55 AA | L | D | T | payload | CK0 CK1
                 D=0x22 (vers BMS), 0x25 (depuis BMS) ; T=0x01 lecture
                 Registres : 0x31 (SOC/I/V/T°), 0x40 (10 cellules), 0x1B (cycles)
Débit          : ~20 octets par échange
Alimentation   : côté BMS, aucune (l'ESP32 n'alimente pas le BMS)
Connectique    : JST PAP03-V 3 points — R / T / L (L = feu stop, non utilisé)
Longueur       : ≤ 30 cm
Criticité      : C2
Fréquence      : 2 Hz (0x31), 0,2 Hz (0x40)
Gestion erreur : checksum de trame + timeout 200 ms + 3 tentatives
Perte          : SOC indisponible → WARN. Repli sur l'estimation par tension de
                 bus (grossière) + intégration ACS758 (dérive). Pas d'arrêt.
Sécurité       : ⚠️ LECTURE SEULE. Le bus BMS n'a ni authentification ni
                 contrôle d'intégrité au-delà du checksum, et le firmware du BMS
                 n'est ni signé ni chiffré. Aucune écriture, jamais.
```

### L6 — ESP32-SAFETY ↔ BNO085

```
A → B          : ESP32 maître SPI → BNO085
Interface      : SPI mode 3 (CPOL=1, CPHA=1), 3 MHz max ✅
                 + broche H_INTN (actif bas) obligatoire
                 ⚠️ NE PAS UTILISER L'I²C : Adafruit documente que le BNO085
                 « viole le protocole I²C » et est incompatible avec l'ESP32
Protocole      : SHTP / SH-2 — Rotation Vector (quaternion 9 axes) @100 Hz
                 + gyro calibré + accélération linéaire
Alimentation   : VDD 3,3 V, VDDIO 3,3 V — ⚠️ VDD doit monter AVANT ou AVEC VDDIO
Connectique    : nappe blindée courte, connecteur JST-GH 8 points
Longueur       : ≤ 15 cm  ⚠️ le BNO085 doit être monté LOIN DES MOTEURS
                 (le magnétomètre est le maillon faible, §N.3)
Criticité      : C2
Fréquence      : 100 Hz (latence datasheet 6,6 ms @100 Hz ✅)
Gestion erreur : compteur de séquence SHTP + timeout 50 ms
Perte          : EKF sans IMU → DEGRADED. La navigation continue en odométrie
                 pure sur quelques mètres, puis STOP.
Note           : initialisation interne ≥ 90 ms après reset avant toute
                 communication ✅
```

### L7 — X1 ↔ YDLIDAR X4

```
A → B          : X1 → carte adaptatrice CP2102 → lidar
Interface      : USB 2.0 → UART TTL 3,3 V
Protocole      : propriétaire YDLIDAR, 128000 bauds, 8N1 ✅
Débit          : 5000 points/s, 6–12 Hz de rotation ✅
Alimentation   : 4,8–5,2 V, 350–500 mA, ⚠️ POINTE 1000 mA au démarrage moteur ✅
                 → LE PORT USB_PWR DOIT ÊTRE ALIMENTÉ PAR LE RAIL 5V_LOGIC,
                 pas par le port USB du X1. Le manuel avertit explicitement que
                 de nombreuses cartes ne fournissent pas assez de courant.
                 ⚠️ Ne pas utiliser de power bank (ondulation).
Connectique    : le type de port (MicroUSB vs USB-C) varie selon la révision
                 de la carte adaptatrice — à vérifier physiquement
Longueur       : ≤ 1 m
Criticité      : C3 (C2 en phase 1 intérieur)
Fréquence      : 7 Hz typique
Gestion erreur : diagnostic_updater / DiagnosedPublisher sur /scan
Perte          : costmap locale sans lidar → Nav2 s'arrête (collision_monitor
                 source_timeout = 2 s → action stop). DEGRADED.
Limite         : ❌ 0–40 °C, 0/550/2000 lux, aucun IP → INTÉRIEUR SEULEMENT ✅
```

### L8 — X1 ↔ Kinect v2

```
A → B          : X1 (USB 3.0) → Kinect v2 via hub Microsoft
Interface      : USB 3.0 — ⚠️ USB 2.0 NON SUPPORTÉ ✅
                 ⚠️ Contrôleurs Intel et NEC/Renesas OK ; ASMedia NE FONCTIONNE PAS ✅
                 → MESURE M9 : lspci | grep -i usb sur le X1
Protocole      : libfreenect2
Débit          : 5–10 % de la bande passante USB 3.0 (~250–500 Mbit/s) ✅
                 Couleur 1920×1080 JPEG, profondeur/IR 512×424
Alimentation   : ⚠️ SECTEUR OBLIGATOIRE — bloc 12 V / 2,67 A (32 W) ✅
                 → sur le robot : DC/DC 42→12 V dédié 40 W, ou renoncer
Connectique    : connecteur propriétaire Microsoft + hub
Longueur       : câble d'origine
Criticité      : C4
Fréquence      : 30 fps théorique
Gestion erreur : réénumération USB fréquente (« USB problems can happen a lot »,
                 avertissement du projet lui-même)
Perte          : perception 3D perdue. Aucun impact sur la sécurité.
Limites        : ❌ Pipeline CPU ≈ 200 ms/trame → OpenCL obligatoire (Intel UHD
                 via intel-opencl-icd + VA-API pour le JPEG)
                 ❌ Aucun driver ROS 2 Jazzy (kinect2_ros2 = Humble, et son
                 build désactive CUDA/OpenCL)
                 ❌ libfreenect2 sans release depuis août 2021
                 ❌ IR actif : inutilisable en extérieur ensoleillé
```

### L9 — X1 ↔ GPS USB

```
A → B          : X1 (USB 2.0) → récepteur GPS
Interface      : USB CDC → /dev/ttyACM0
Protocole      : NMEA 0183 présumé 🔴 (mesure M11)
Débit          : 9600–115200 bauds, 1–10 Hz
Alimentation   : bus USB
Connectique    : USB-A ; ⚠️ l'antenne doit être en HAUT du robot, dégagée
Longueur       : selon montage
Criticité      : C2 en extérieur
Fréquence      : 1–5 Hz
Gestion erreur : DiagnosedPublisher sur /gps/fix + surveillance du HDOP et
                 du nombre de satellites
Perte          : l'EKF global perd sa référence absolue. L'EKF local continue.
                 Nav2 sur waypoints GPS → abandon de mission, retour en READY.
                 ⚠️ Une covariance NavSatFix à zéro ou bidon FAIT DIVERGER
                 L'EKF : il faut publier la covariance réelle depuis le HDOP.
```

### L10 — Réseau (voir §O pour le détail)

```
A → B          : X1 ↔ poste opérateur
Interface      : Wi-Fi 5/6 (M.2) ou Ethernet Gigabit
Protocole      : WireGuard (UDP 51820) → SSH + DDS restreint + Foxglove WS
Criticité      : C3 — le robot doit être AUTONOME sans réseau
Perte          : la téléopération est perdue → mission_manager arrête la mission
                 en cours si elle est téléopérée ; une mission autonome CONTINUE
                 (choix explicite, révisable — voir §O.6)
```

## F.6 Schéma de synthèse des communications

```
                      ┌──────────────────── YOUYEETOO X1 ────────────────────┐
                      │                                                      │
   Ethernet RJ45 ─────┤ eth0                                                 │
   Wi-Fi M.2 ─────────┤ wlan0 ──► WireGuard wg0                              │
                      │                                                      │
                      │ USB 3.0 #1 ──► [Kinect v2]  ⚠️ contrôleur dédié      │
                      │ USB 3.0 #2 ──► [hub USB 3.0 alimenté]                │
                      │                    ├──► YDLIDAR X4 (CP2102)          │
                      │                    │    ⚠️ +5 V auxiliaire externe   │
                      │                    ├──► GPS USB                      │
                      │                    └──► [USB-CAN gs_usb] ──► can0    │
                      │ USB 2.0 ×2 ──► libre (maintenance, clavier)          │
                      │ UART ttyS0/S4/S5 ──► libre (console, secours BNO085) │
                      │ I²C ──► libre                                        │
                      │ GPIO ×5 ──► 1 utilisé : entrée « heartbeat X1 »      │
                      │              (créneau 10 Hz vers ESP32-SAFETY)  ★    │
                      └──────────────────────────────────────────────────────┘
                                              │ can0 @ 500 kbit/s
      ════════════════════════════════════════╪════════════════════════════
        │                        │                        │
   ┌────▼──────────┐    ┌────────▼───────┐    ┌───────────▼──────┐
   │ ESP32-SAFETY  │    │ ESP32-MOT-AV   │    │ ESP32-MOT-AR     │
   │ node id 1     │    │ node id 2      │    │ node id 3        │
   ├───────────────┤    ├────────────────┤    ├──────────────────┤
   │ SPI  → BNO085 │    │ I²C → MCP4728  │    │ I²C → MCP4728    │
   │ UART → BMS ⚡ │    │ DAC → VR ×2    │    │ DAC → VR ×2      │
   │ ADC  → ACS758 │    │ GPIO→ DIR/EL/  │    │ GPIO→ DIR/EL/    │
   │       (ADS1115)│   │      STOP ×2   │    │      STOP ×2     │
   │ ADC  → V_bus  │    │ PCNT← Hall ×4  │    │ PCNT← Hall ×4    │
   │ ADC  → V_pack │    │ ADC → T° ×1    │    │ ADC → T° ×1      │
   │ GPIO → conta- │    └────────────────┘    └──────────────────┘
   │        cteur  │
   │ GPIO → pré-   │    ⚡ = isolation galvanique obligatoire
   │        charge │
   │ GPIO → /SAFE  │    ★ Le heartbeat matériel X1→SAFETY est indépendant du
   │ GPIO ← champi-│      bus CAN : si le CAN tombe mais que Linux tourne, on
   │        gnon   │      le sait. Si Linux se fige, le créneau s'arrête même
   │ PWM  → venti- │      si l'USB-CAN reste énuméré. C'est un test différent.
   │        los ×2 │
   │ TACH ← venti- │
   │        los ×2 │
   │ WDT  ← TPL5010│
   └───────────────┘
```

---

# G. Architecture des ESP32

## G.1 Le principe de séparation, et sa remise en question

Le cahier des charges propose :

> X1 = intelligence / ROS 2 / navigation / perception / missions
> ESP32 = temps réel / I/O / sécurité locale / contrôle bas niveau

**Je valide cette séparation, mais je la précise sur un point essentiel** : ce n'est pas seulement une répartition de *tâches*, c'est une répartition d'**autorité**.

La formulation initiale laisse penser que le X1 commande et que l'ESP32 exécute. Dans l'architecture retenue, c'est plus fort que ça :

| | Le X1 | L'ESP32-SAFETY |
|---|---|---|
| Consignes de vitesse | **Produit** | Vérifie, sature, ou refuse |
| Armement | **Demande** | **Accorde ou refuse** |
| Arrêt d'urgence | Peut le demander | **Peut l'imposer** |
| Budget de courant | Ignore | **Applique** |
| Contacteur de puissance | Aucun accès | **Seul à piloter** |

Un système où le PC est le seul à pouvoir arrêter les moteurs a un point de défaillance unique. Ici, le X1 peut planter, être mis à jour, redémarrer, ou tomber en OOM : l'ESP32-SAFETY constate la perte du heartbeat et arrête le robot proprement, sans avoir besoin qu'on le lui dise.

**Où je remets la séparation en question** : faut-il vraiment que l'IMU soit sur l'ESP32 plutôt que sur le X1 ? Analyse :

| BNO085 sur… | Pour | Contre |
|---|---|---|
| **X1 en UART-RVC** (ttyS4) | Zéro matériel, fonctionne tout de suite | ❌ **Pas de quaternion** (Euler seulement) ✅, 100 Hz fixe, horodatage côté Linux (gigue de plusieurs ms), pas de covariance |
| X1 en SPI | Quaternion complet | ❌ **Nécessite une recompilation du noyau** sur le X1 ✅ |
| X1 en I²C | — | ❌ Le BNO085 étire l'horloge et « viole le protocole » ✅ ; risque avéré |
| **ESP32-SAFETY en SPI** | Quaternion 400 Hz, **horodatage matériel**, pas de clock stretching, pas de noyau à recompiler, l'IMU reste disponible même si le X1 plante | Un peu de firmware à écrire |

→ **ESP32-SAFETY en SPI** est retenu. L'argument décisif est l'**horodatage** : sur un EKF, une gigue de 5 ms sur l'IMU à 100 Hz dégrade sensiblement l'estimation. L'argument secondaire — l'IMU reste disponible pour la détection de basculement même X1 éteint — est un bonus de sécurité réel.

**Repli de phase 1** : si le firmware SPI n'est pas prêt, brancher le BNO085 en **UART-RVC sur `/dev/ttyS4`** du X1 et utiliser un nœud Python (`adafruit-circuitpython-bno08x-rvc`). Ça marche en une heure et permet d'avancer sur le reste. L'architecture ROS 2 est conçue pour que le passage de l'un à l'autre soit un changement de fichier de lancement (le topic `/imu/data` est le même).

## G.2 Rôles retenus

### ESP32-SAFETY (nœud CAN 1) — le gardien

| Responsabilité | Détail | Période |
|---|---|---|
| **Machine à états de sécurité** | Autoritaire. Voir §J.2 | 1 kHz |
| **Pilotage du contacteur** | Seul à commander la bobine | événementiel |
| **Séquence de précharge** | Avec timeout et diagnostic de court-circuit | au boot |
| **Ligne matérielle `/SAFE`** | Sortie collecteur ouvert vers les 4 variateurs | 1 kHz |
| **Lecture du champignon** | Entrée NF, anti-rebond matériel + logiciel | 1 kHz |
| **Mesure de courant** | ACS758 via ADS1115 ; **budget de courant global** | 500 Hz |
| **Mesure de tension** | V_pack (amont contacteur) et V_bus (aval) par ponts diviseurs | 100 Hz |
| **Dialogue BMS** | Lecture seule, registres 0x31 / 0x40 / 0x1B | 2 Hz / 0,2 Hz |
| **IMU BNO085** | SPI, Rotation Vector + gyro + accel, horodatés | 100 Hz |
| **Ventilateurs** | PWM 25 kHz, boucle sur température, lecture tachy | 10 Hz |
| **Surveillance des nœuds** | Heartbeat CAN des 2 MOTION + heartbeat matériel du X1 | 10 Hz |
| **Watchdog externe** | TPL5010 ou équivalent — si l'ESP32 se fige, reset + `/SAFE` | matériel |

⚠️ **Alimentation** : l'ESP32-SAFETY est alimenté **en amont du contacteur** (via un petit DC/DC 42→5 V dédié, 5 W, protégé par un fusible MIDI 2 A). Il doit vivre quand le contacteur est ouvert, sinon il ne peut ni le fermer, ni diagnostiquer, ni rapporter la cause de l'arrêt.

### ESP32-MOTION AVANT / ARRIÈRE (nœuds CAN 2 et 3) — les exécutants

| Responsabilité | Détail | Période |
|---|---|---|
| **Boucle de vitesse** | PI par roue, consigne → DAC → VR | **200 Hz** |
| **Décodage Hall** | 2 lignes par roue → PCNT, sens + comptes | interruption |
| **Estimation de vitesse** | Filtrage, conversion en rad/s | 200 Hz |
| **Limitation** | Vitesse max, accélération max, courant estimé max | 200 Hz |
| **Détection de calage** | Commande ≠ 0 ET comptage = 0 pendant 300 ms | 200 Hz |
| **Watchdog de commande** | Pas de trame `0x100/0x101` depuis 150 ms → VR=0 + frein | 200 Hz |
| **Surveillance `/SAFE`** | Entrée matérielle ; si active → VR=0 + frein immédiat | interruption |
| **Publication** | `FB_WHEELS` 50 Hz, `MOT_STATUS` 10 Hz, heartbeat 10 Hz | — |

**Pourquoi deux nœuds MOTION plutôt qu'un seul pour quatre roues ?**

| Un nœud, 4 roues | Deux nœuds, 2 roues |
|---|---|
| ❌ 8 lignes Hall (ESP32 a 8 unités PCNT — saturé, aucune marge) | ✅ 4 lignes par nœud, 4 PCNT libres |
| ❌ 4 canaux DAC nécessaires (le MCP4728 en a 4 — juste) | ✅ 2 canaux utilisés sur 4 |
| ❌ 12 GPIO de commande + 4 ADC : câblage dense, faisceaux longs vers l'arrière | ✅ Chaque nœud est à côté de ses variateurs — faisceaux courts |
| ❌ Sa panne immobilise les 4 roues | ✅ Sa panne laisse 2 roues sur 4 → repli possible en mode dégradé |
| ✅ Un firmware, un nœud | Deux instances du **même** firmware, différenciées par un strap GPIO |

Le **firmware est identique** : le rôle (avant/arrière) est lu au démarrage sur deux GPIO de configuration reliés à la masse ou à 3,3 V. Un seul binaire à maintenir.

### Ce que les ESP32 ne font PAS

Volontairement, et c'est important :

| ❌ Pas de… | Pourquoi |
|---|---|
| Wi-Fi activé | Consommation, bruit RF près des capteurs, surface d'attaque, et surtout : la pile Wi-Fi ESP32 introduit des blocages de plusieurs ms dans la boucle temps réel |
| micro-ROS sur le chemin critique | §F.1 |
| Bluetooth | Idem Wi-Fi |
| Stockage de configuration modifiable à distance | La configuration vient de `CONFIG` (0x320) au boot, en lecture ; les limites de sécurité sont **en dur dans le firmware** |
| Logique de navigation | Séparation des responsabilités |
| Écriture dans le BMS | §F.5-L5 |

## G.3 Affectation des broches (proposition — à adapter au modèle exact, question Q5)

⚠️ **Cette affectation suppose un ESP32 classique (ESP32-WROOM-32).** Si tes cartes sont des **ESP32-S3**, il faut la revoir : **le S3 n'a pas de DAC** (le MCP4728 en I²C devient alors obligatoire, ce qui est de toute façon la solution retenue) et son mapping PCNT diffère.

### ESP32-SAFETY

| Fonction | GPIO | Note |
|---|---|---|
| CAN TX / RX (TWAI) | 5 / 4 | vers SN65HVD230 |
| SPI BNO085 : SCK / MISO / MOSI / CS / INT / RST | 18 / 19 / 23 / 15 / 34 / 33 | VSPI |
| I²C ADS1115 : SDA / SCL | 21 / 22 | ACS758 + V_bus + V_pack |
| UART BMS : TX / RX | 17 / 16 | via isolateur |
| Sortie contacteur | 25 | MOSFET + diode de roue libre |
| Sortie relais précharge | 26 | idem |
| Sortie `/SAFE` | 27 | **collecteur ouvert, actif bas** ★ |
| Entrée champignon | 35 | entrée seule, pull-up externe |
| Entrée heartbeat X1 | 39 | créneau 10 Hz depuis un GPIO du X1 |
| PWM ventilateur 1 / 2 | 12 / 13 | LEDC 25 kHz |
| Tachy ventilateur 1 / 2 | 36 / 32 | PCNT |
| Watchdog externe : DONE / WAKE | 14 / 2 | TPL5010 |
| Entrée T° dissipateur | (ADS1115 ch3) | NTC 10 k |

★ **Pourquoi `/SAFE` est actif bas et en collecteur ouvert** : c'est de la **sécurité positive**. Si l'ESP32 est éteint, en reset, ou si le fil est coupé, la ligne est tirée à la masse par la résistance de pull-down du côté variateur → le robot est en sécurité. Un signal actif haut ferait exactement l'inverse.

### ESP32-MOTION (identique pour AV et AR)

| Fonction | GPIO | Note |
|---|---|---|
| CAN TX / RX | 5 / 4 | |
| I²C MCP4728 : SDA / SCL | 21 / 22 | DAC 4 canaux 12 bits |
| Roue A : Hall_A / Hall_B | 34 / 35 | PCNT unit 0, via 74LVC245 |
| Roue B : Hall_A / Hall_B | 36 / 39 | PCNT unit 1 |
| Roue A : DIR / EL / STOP | 25 / 26 / 27 | MOSFET 2N7002, collecteur ouvert |
| Roue B : DIR / EL / STOP | 32 / 33 / 14 | idem |
| Entrée `/SAFE` | 15 | interruption, pull-up |
| Strap de rôle (AV/AR) | 16 / 17 | à la masse ou à 3,3 V |
| Entrée T° dissipateur | 2 (ADC) | NTC 10 k |

⚠️ Sur ESP32 classique, les GPIO 34–39 sont **en entrée seule et sans pull-up interne** : prévoir des pull-up externes 10 kΩ. Et l'ADC2 est inutilisable quand le Wi-Fi est actif — non pertinent ici (Wi-Fi désactivé), mais à savoir.

## G.4 Structure du firmware

Le même squelette pour les trois nœuds :

```
main()
 ├─ hw_init()          GPIO en état SÛR EN PREMIER (/SAFE actif, contacteur ouvert)
 ├─ self_test()        vérifie DAC, ADC, PCNT, transceiver CAN
 ├─ can_init(500k)
 ├─ wdt_init()         watchdog externe + watchdog interne ESP32
 └─ vTaskStartScheduler()

     ┌── tâche CONTROL      prio 24, période 5 ms (200 Hz)  ← temps réel dur
     │     lecture PCNT → vitesse → PI → DAC → VR
     │     vérification watchdogs, /SAFE, limites
     │
     ├── tâche CAN_RX       prio 23, événementiel
     │     décodage, mise à jour des consignes, horodatage
     │
     ├── tâche CAN_TX       prio 20, période 20 ms (50 Hz)
     │     retours, statut, heartbeat
     │
     ├── tâche SENSORS      prio 15, période 10 ms (SAFETY : IMU, ADC)
     │
     └── tâche HOUSEKEEPING prio 5, période 100 ms
           BMS, ventilateurs, températures, diagnostics
```

**Règles de codage imposées** :

1. **Aucune allocation dynamique** après l'initialisation. Pas de `malloc`, pas de `String`, pas de `std::vector` qui grandit.
2. **Aucun appel bloquant dans la tâche CONTROL.** Les échanges I²C avec le DAC utilisent le DMA ou une file.
3. **Les limites de sécurité sont des `constexpr` en dur**, pas des paramètres. Une valeur reçue par CAN ne peut que *réduire* une limite, jamais l'augmenter.
4. **État sûr en premier** : `hw_init()` met `/SAFE` actif et le contacteur ouvert **avant** toute autre initialisation. Un crash pendant l'init laisse le robot en sécurité.
5. **Tout compteur d'erreur est publié.** Erreurs CAN, timeouts, resets watchdog : tout remonte en diagnostics.
6. **Le protocole vient du fichier généré** `protocol.h`, jamais de constantes recopiées à la main.

---

# H. Architecture des moteurs

## H.1 Chaîne de commande retenue

```
 Nav2 / téléop
      │ geometry_msgs/TwistStamped  (⚠️ stamped obligatoire sur Jazzy)
      ▼
 twist_mux ──► velocity_smoother ──► nav2_collision_monitor
      │  (priorité : e-stop > téléop > nav)
      ▼
 diff_drive_controller  (ros2_control, 4 joints : 2 gauche / 2 droite)
      │ interfaces de commande « velocity » par roue
      ▼
 controller_manager  @ 100 Hz
      │ write()
      ▼
 retriever_hardware (SystemInterface)  ──► SocketCAN can0
      │ CMD_WHEELS_FRONT (0x100) / CMD_WHEELS_REAR (0x101) @ 50 Hz
      ▼
 ESP32-MOTION  ──► boucle PI @ 200 Hz ──► MCP4728 ──► ampli op ──► VR
      │                                                     + DIR
      ▼
 ZS-X11H  ──► commutation 6 pas ──► moteur-roue BLDC
      │
      └─► Hall A/B ──► PCNT ESP32 ──► vitesse mesurée ──► FB_WHEELS (0x180/181)
```

**Pourquoi `diff_drive_controller` pour 4 roues** : il n'existe **aucun contrôleur skid-steer 4 roues dédié** dans `ros2_controllers` Jazzy (✅ vérifié). Mais ses paramètres `left_wheel_names` et `right_wheel_names` sont des **tableaux de chaînes** : on y met `["av_gauche_joint", "ar_gauche_joint"]` et `["av_droite_joint", "ar_droite_joint"]`. Le contrôleur commande les deux roues d'un côté à la même vitesse et moyenne leurs retours pour l'odométrie. C'est la solution standard, propre, et supportée.

⚠️ **Piège Jazzy à connaître** : sur Jazzy, `~/cmd_vel` de `diff_drive_controller` est **obligatoirement** de type `geometry_msgs/msg/TwistStamped` — le paramètre `use_stamped_vel` a été supprimé (✅ guide de migration officiel). Or Nav2 publie du `Twist` non stampé par défaut. Trois solutions, toutes packagées pour Jazzy : mettre `enable_stamped_cmd_vel: true` dans les nœuds Nav2, interposer `twist_stamper`, ou utiliser `twist_mux` (qu'on veut de toute façon pour la priorité téléop > nav).

## H.2 Alimentation des variateurs

Chaque ZS-X11H est alimenté directement depuis le busbar, par sa propre dérivation protégée :

```
   BUSBAR + ──[ MIDI 58 V, 25 A ]── 6 mm² ──► VCC du ZS-X11H
                                                  │
                                        [ 470 µF 63 V + 100 nF ]  ← au plus près
                                        [ TVS 45-47 V — voir §D.8 ]
                                                  │
   BUSBAR − ◄──────────── 6 mm² ──────────────── GND
```

**Pourquoi un fusible par variateur et pas un seul fusible commun** : un court-circuit dans un variateur (MOSFET claqué en conduction, cas de défaillance le plus courant) doit isoler **ce** variateur, pas couper tout le robot. Avec quatre fusibles, le robot reste en `DEGRADED` sur trois roues et peut rentrer. Avec un fusible commun, il s'arrête n'importe où.

**Calibre 25 A** : au-dessus des 20 A de pointe du variateur (pour ne pas fondre en usage légitime), en dessous de l'ampacité du câble 6 mm² (28 A après déclassement).

⚠️ **Rappel M12** : vérifier le marquage des condensateurs d'entrée avant d'appliquer 42 V.

## H.3 Génération de la consigne de vitesse

Deux options, et je recommande la seconde.

### Option A — commande PWM directe (cavalier J1)

| Pour | Contre |
|---|---|
| Zéro composant supplémentaire | ❌ Le cavalier J1 est **souvent non peuplé** : soudure au dos de la carte, **dissipateur à déposer**. Plusieurs makers rapportent avoir détruit des cartes à cette étape |
| Résolution du LEDC ESP32 excellente | ❌ Il faut aussi mettre le potentiomètre embarqué au minimum |
| | 🔴 **Fréquence contradictoire selon les sources** : 50 Hz–20 kHz / 2,5–5 V d'un côté, 1–10 kHz de l'autre |
| | ❌ Sortie ESP32 à 3,3 V < 5 V attendu → adaptation nécessaire de toute façon |

### Option B — DAC I²C + amplificateur ✅ **RETENU**

```
   ESP32 ──I²C──► MCP4728 ──► 0–3,3 V ──► AOP rail-to-rail ──► 0–5 V ──► VR
                  (4 ch,              (gain 1,515,             │
                   12 bits)            alim 5 V, ex. MCP6002)  │
                                                        [10 kΩ vers GND]  ★
```

| Pour | Contre |
|---|---|
| ✅ Aucune modification des cartes ZS-X11H | 2 composants par nœud (~4 €) |
| ✅ Fonctionne quel que soit l'état du cavalier J1 | |
| ✅ Résolution 12 bits sur 0–5 V = **1,2 mV** — bien plus fin que le seuil de démarrage mesuré (~0,07 V) | |
| ✅ Le MCP4728 a une **mémoire EEPROM** : on peut y écrire 0 V comme valeur de démarrage, donc **la sortie est à zéro dès la mise sous tension**, avant même que le firmware ne démarre | |
| ✅ 4 canaux : un seul composant pour 2 roues, avec 2 canaux libres | |
| ✅ Fonctionne aussi si tes ESP32 sont des S3 (pas de DAC interne) | |

★ **La résistance de 10 kΩ vers la masse sur l'entrée VR, au plus près du variateur, n'est pas optionnelle.** Elle transforme une rupture du fil de consigne en « consigne nulle » plutôt qu'en « entrée flottante, comportement indéfini ».

**Sur la linéarité** : le ZS-X11H régule en **tension/vitesse**, pas en couple, et il n'y a **aucune limitation de courant réglable**. La relation VR → vitesse n'est ni documentée ni garantie linéaire. C'est pourquoi l'ESP32 ferme une **boucle PI sur la vitesse mesurée** aux capteurs Hall, plutôt que d'utiliser VR en boucle ouverte. Cela compense la non-linéarité, la variation de tension du pack (30 → 42 V, soit 40 % d'écart !) et la charge.

⚠️ **Ce dernier point est souvent sous-estimé** : sans boucle fermée, la même consigne VR donne des vitesses très différentes entre un pack chargé et un pack vide. Pour Nav2, ce serait rédhibitoire.

## H.4 Retour de vitesse — le point technique le plus important de cette section

### Le problème

La sortie `SC` du ZS-X11H bascule à chaque transition Hall — soit 90 transitions par tour sur un moteur 6,5" (15 paires de pôles × 6 états). C'est une bonne résolution. **Mais c'est un signal non signé** : impossible de savoir si la roue avance ou recule.

Pour de l'odométrie Nav2, c'est disqualifiant :
- Une roue qui patine à l'envers dans une pente serait comptée comme avançant.
- Un robot poussé à la main donnerait une odométrie fausse.
- Après un blocage, le sens de reprise est inconnu.

### La solution : dériver deux lignes Hall par moteur

Les trois capteurs Hall d'un BLDC sont déphasés de 120° électriques. Deux d'entre eux, pris ensemble, se décodent exactement comme un codeur incrémental en quadrature (avec des rapports cycliques inégaux, mais la table de transition donne bien le sens).

```
   Moteur ── connecteur Hall JST 5 pts ──┬──► ZS-X11H (usage normal)
                                          │
                                          └──► dérivation en Y
                                               │
                            Ha ──[RC 1k/1nF]──►│
                            Hb ──[RC 1k/1nF]──►│ 74LVC245 (Vcc = 3,3 V,
                                               │  entrées 5 V tolérantes)
                                               │
                                               └──► GPIO 34/35 de l'ESP32
                                                    → unité PCNT en mode
                                                      quadrature x4
```

| Grandeur | Valeur |
|---|---|
| Paires de pôles | 15 🟡 (**à confirmer par M6**) |
| Transitions Hall A+B par tour, décodage ×4 | 15 × 4 = **60 comptes/tour** |
| Diamètre de roue 6,5" | 165 mm |
| Circonférence | 518 mm |
| **Résolution linéaire** | **8,6 mm par compte** |
| À 1,5 m/s | 174 comptes/s par roue |
| À 1,5 m/s, fréquence d'une ligne Hall | **43 Hz** (≈ 87 transitions/s) |

8,6 mm de résolution est **largement suffisant** pour de l'odométrie Nav2 (le patinage du skid-steer introduit une erreur bien supérieure).

⚠️ **Trois précautions de câblage** :
1. **Torsader** les lignes Hall et **blinder** le faisceau : elles passent à côté des câbles de phase, qui commutent des dizaines d'ampères à quelques kHz.
2. **Filtre RC 1 kΩ / 1 nF** sur chaque ligne (fréquence de coupure ≈ 160 kHz) : élimine les fronts parasites sans dégrader le signal utile à 43 Hz.
3. **Ne pas alimenter les capteurs Hall depuis l'ESP32.** Ils sont déjà alimentés par le variateur (5 V, ≤30 mA au total pour la sortie logique du variateur 🔴). Doubler l'alimentation créerait une boucle de masse.

🔴 **Mesure M7** avant de câbler : vérifier au minimum l'amplitude réelle des lignes Hall (5 V attendu) et la présence de pull-ups sur le variateur. Si les niveaux sont incertains, remplacer le 74LVC245 par des optocoupleurs rapides (6N137) — plus lent mais galvaniquement isolé.

## H.5 Détection des pannes moteur

| Panne | Détection | Seuil 📐 | Réaction |
|---|---|---|---|
| **Moteur bloqué** | Consigne ≠ 0 pendant N cycles ET comptes Hall = 0 | 300 ms | Consigne de cette roue à 0, drapeau `STALL`, `MOT_STATUS` → X1 → `DEGRADED`. Si 2 roues du même côté : `FAULT` |
| **Capteur Hall défaillant** | Séquence d'états Hall invalide (transition impossible) | 5 occurrences en 1 s | Drapeau `HALL_FAULT`, roue désactivée, `DEGRADED` |
| **Variateur muet** | Consigne appliquée, aucune réaction en vitesse, courant global inchangé | 500 ms | `DRIVER_FAULT`, roue désactivée |
| **Variateur en court-circuit** | Chute brutale de V_bus + pic ACS758 | > 35 A ou dV/dt anormal | `/SAFE` immédiat + ouverture contacteur. Le fusible 25 A doit avoir fondu |
| **Surintensité globale** | ACS758 > budget | 22 A → bridage progressif ; 30 A > 3 s ou 35 A instantané → `/SAFE` | §A.1-5 |
| **Sur-régénération** | ACS758 **négatif** (le sens bidirectionnel sert ici) | < −15 A 📐 | Réduction du taux de freinage électrique, passage en roue libre partielle |
| **Surchauffe variateur** | NTC sur le dissipateur | 70 °C → bridage 50 % ; 85 °C → arrêt | Ventilateurs à 100 % en parallèle |
| **Désynchronisation gauche/droite** | Écart entre vitesse commandée et mesurée > 30 % sur une roue alors que les autres suivent | 500 ms | `DEGRADED`, bridage global |

## H.6 ⚠️ Le freinage régénératif — un risque réel et spécifique

Quand les ZS-X11H freinent électriquement, l'énergie cinétique du robot repart vers le pack. Trois problèmes :

1. **Surtension.** Sur un pack à 42 V (pleine charge), le BMS coupe en surtension (seuil 4,2 V/cellule ✅). La coupure BMS = perte de toute la puissance, y compris le calculateur, en pleine manœuvre de freinage. Le pire moment possible.
2. **Courant inverse excessif.** La communauté M365 rapporte des cartes ESC détruites au-delà de ~30 A de régénération 🟡.
3. **Le raccordement `P-` vs `C-`** : le BMS doit être raccordé côté **`P-`**, faute de quoi la régénération provoque des surtensions 🟡.

**Mitigations retenues** :

| Mitigation | Détail |
|---|---|
| **Mesure du courant de régénération** | L'ACS758**-B** est bidirectionnel : il mesure le courant négatif nativement. C'est la raison pour laquelle cette version est un atout ici. |
| **Bridage du freinage à haut SOC** | Si V_bus > 41 V, le taux de décélération maximal autorisé est réduit (rampe de décélération allongée). Le robot met plus de distance à s'arrêter, mais ne casse rien. |
| **Limite de décélération dans `diff_drive_controller`** | `linear.x.max_deceleration` — paramètre disponible sur Jazzy |
| **Priorité au freinage mécanique en urgence** | Sur arrêt d'urgence à haut SOC, le compromis penche vers le freinage électrique quand même (la sécurité prime sur le matériel), mais l'événement est journalisé et remonté en `WARN` |
| **Évolution** | Un **hacheur de freinage** (MOSFET + résistance de puissance sur le bus, déclenché au-dessus de 41,5 V) est la solution propre. À prévoir en phase 4 si les freinages fréquents deviennent un problème. |

## H.7 Où se trouve le dernier niveau capable d'arrêter physiquement les moteurs

Réponse directe à la question du cahier des charges :

> **Le contacteur DC principal**, dont la bobine est alimentée à travers le contact NF du champignon d'arrêt d'urgence.

Et, en amont de lui, **le coupe-batterie manuel** puis **le fusible MRBF**.

Ce qui rend cette réponse valide :

| Élément | Peut-il être contourné par un bug ? |
|---|---|
| Nav2 / collision monitor | ✅ Oui — c'est du logiciel non temps réel |
| `cmd_vel_timeout` de ros2_control | ✅ Oui — un nœud qui publie du zéro en boucle le contourne |
| Watchdog ESP32-MOTION | 🟡 Un firmware bogué le contourne |
| Watchdog ESP32-SAFETY | 🟡 Idem, mais protégé par un watchdog **externe** (TPL5010) |
| Ligne `/SAFE` | 🟡 Elle dépend d'un microcontrôleur pour être *relâchée*, mais elle est **tirée à la masse** par le champignon, indépendamment |
| **Bobine du contacteur en série avec le champignon** | ❌ **Non.** Aucun logiciel ne peut refermer un contact mécanique ouvert. |
| **Coupe-batterie** | ❌ Non |

⚠️ **Nuance essentielle** : ouvrir le contacteur met les moteurs en **roue libre**, pas en freinage. Sur un plan incliné, un robot de 35 kg en roue libre est un danger. C'est pourquoi la boucle d'arrêt d'urgence est **à deux temps** :

```
   t = 0 ms     champignon enfoncé
                ├──► ligne /SAFE tirée à la masse DIRECTEMENT (par le contact)
                │      └──► entrées EL/STOP des 4 ZS-X11H → freinage électrique
                │
                └──► la chaîne d'alimentation de la bobine est coupée
                       └──► mais un réseau RC maintient la bobine ~1 s

   t ≈ 1000 ms  le contacteur s'ouvre — le robot est déjà à l'arrêt ou
                très ralenti, et il n'y a plus de courant à couper
                (bénéfice secondaire : pas d'arc de coupure sur le contacteur)
```

📐 La constante de temps de 1 s est à valider sur la mesure **M4** (temps d'arrêt réel du variateur en freinage) et sur un essai de freinage à pleine vitesse, roues levées puis au sol.

**Évolution recommandée en phase 4** : un **frein mécanique à manque de courant** (frein électromagnétique qui serre quand il n'est plus alimenté) sur au moins deux roues. C'est la seule solution qui tient une pente avec le robot totalement hors tension. À budgéter si le robot doit opérer sur des terrains inclinés.

---

*Suite : `03-ros2-etats-boot.md` — sections I, J, K.*
