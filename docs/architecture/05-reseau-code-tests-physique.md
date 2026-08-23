# O. Réseau et accès distant

## O.1 Principe : le graphe ROS 2 ne traverse pas le lien radio

C'est la décision structurante de cette section, et elle mérite d'être justifiée.

Le réflexe naturel est de mettre le poste opérateur et le robot sur le même `ROS_DOMAIN_ID` et de lancer RViz. Ça marche — sur un réseau filaire. Sur du Wi-Fi, ça se dégrade de façon pénible et non déterministe :

- **La découverte DDS repose sur du multicast UDP.** Le multicast 802.11 est émis au **débit de base le plus bas** et **sans acquittement** : taux de perte élevé, pas de retransmission. Les points d'accès grand public font de l'IGMP snooping, du multicast-to-unicast, ou droppent purement. L'isolation client casse tout.
- **Le trafic de découverte croît de façon quadratique** avec le nombre de participants.
- **Un topic RELIABLE avec un historique profond sur un lien lossy sature le canal en retransmissions**, et écroule tout le reste — y compris le lien SSH avec lequel on essaie de diagnostiquer.

Symptômes classiques : `ros2 topic list` incomplet, nœuds qui apparaissent 30 s plus tard, TF qui saute, RViz qui ne s'abonne jamais.

**Architecture retenue** :

```
   ╔═══════════════════════════════════════════════════════════════════╗
   ║  ZONE 3 — POSTE OPÉRATEUR                                         ║
   ║    Foxglove Studio (WebSocket)  ·  navigateur  ·  SSH             ║
   ║    RViz2 (uniquement en mode « atelier », voir O.5)               ║
   ╚═══════════════════════════════════╤═══════════════════════════════╝
                                       │  WireGuard — UDP 51820
                                       │  clés Curve25519, aucun mot de passe
   ╔═══════════════════════════════════▼═══════════════════════════════╗
   ║  ZONE 2 — SERVICES EXPOSÉS DU ROBOT (interface wg0 uniquement)    ║
   ║    :22    SSH (clés seulement, mot de passe désactivé)            ║
   ║    :8765  foxglove_bridge (WebSocket)                             ║
   ║    :8080  page d'état HTTP en lecture seule                       ║
   ║    ── nftables : DROP par défaut sur wlan0/eth0 ──                ║
   ╚═══════════════════════════════════╤═══════════════════════════════╝
                                       │
   ╔═══════════════════════════════════▼═══════════════════════════════╗
   ║  ZONE 1 — RÉSEAU INTERNE DU ROBOT (loopback + CAN)                ║
   ║    Graphe ROS 2 COMPLET, confiné à localhost                      ║
   ║    ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST                        ║
   ║    can0 — bus temps réel                                          ║
   ╚═══════════════════════════════════════════════════════════════════╝
```

**Le graphe ROS 2 est entièrement local.** Le pont vers l'extérieur est un **WebSocket** (`foxglove_bridge`), qui se reconnecte tout seul quand le lien tombe et remonte, et qui ne dépend pas de la découverte DDS.

⚠️ **Un lien radio marginal casse un graphe DDS. Il ne casse pas un WebSocket.** C'est la raison principale de ce choix.

## O.2 Configuration DDS

```bash
# /etc/retriever/ros-env.sh — chargé par tous les services
export ROS_DOMAIN_ID=42
export ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST   # découverte confinée à localhost
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp       # défaut Jazzy, Tier 1
export ROS_LOCALHOST_ONLY=0                      # déprécié, on utilise DISCOVERY_RANGE
```

`ROS_AUTOMATIC_DISCOVERY_RANGE` (valeurs : `OFF` | `LOCALHOST` | `SUBNET` (défaut) | `SYSTEM_DEFAULT`) et `ROS_STATIC_PEERS` sont disponibles depuis Iron, donc sur Jazzy, et sont **indépendants du RMW** ✅. C'est la solution portable, sans XML vendeur.

### Si on veut vraiment étendre le graphe (mode atelier, §O.5)

```bash
# Robot 192.168.1.10 / poste 192.168.1.20 — DÉCOUVERTE 100 % UNICAST
export ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST
export ROS_STATIC_PEERS="192.168.1.20"    # (et l'inverse côté poste)
```

### `rmw_zenoh` — à évaluer, pas à déployer d'emblée

`ros-jazzy-rmw-zenoh-cpp` est **packagé et disponible en apt depuis janvier 2025** ✅, et les mainteneurs écrivent explicitement recommander son usage sur Jazzy et Rolling (« *feature complete and extensively tested* »). Son modèle — connexions unicast TCP/QUIC explicites entre routeurs, multicast désactivé par défaut — est **structurellement le bon** pour un lien radio. Un retour terrain publié rapporte un flux caméra 60 Hz passant de ~33 Hz saccadés (Fast DDS) à 59,5 Hz fluides (Zenoh) sur Jazzy.

**Mais** : il n'est **Tier 1 qu'à partir de Kilted**, pas sur Jazzy ✅. Et deux pièges documentés :
- **la configuration IPv6 par défaut fait planter le routeur sur un système IPv4-only** — piège classique en embarqué ;
- panics possibles à l'arrêt (timing de destruction du contexte).

📐 **Recommandation** : garder `rmw_fastrtps_cpp` + WebSocket comme architecture de référence, et évaluer `rmw_zenoh` en phase 3 avec un test A/B sur le lien réel, en injectant de la perte de paquets (`tc netem`). Si le résultat est bon, c'est un basculement d'une variable d'environnement.

## O.3 VPN

**WireGuard**, pour trois raisons : dans le noyau (performant), sans état (survit à un changement de réseau — 4G ↔ Wi-Fi), et configuration triviale.

```ini
# /etc/wireguard/wg0.conf (côté robot)
[Interface]
Address = 10.42.0.1/24
ListenPort = 51820
PrivateKey = <clé robot>

[Peer]                       # poste opérateur
PublicKey = <clé poste>
AllowedIPs = 10.42.0.2/32
PersistentKeepalive = 25
```

⚠️ **Le keepalive de 25 s sert aussi de détection de perte de lien** : `mission_manager` surveille l'état du peer WireGuard (`wg show`) et déclenche la politique de perte réseau (§M.2, panne 8).

## O.4 Pare-feu

```
nftables — politique par défaut : DROP en entrée

Sur wlan0 / eth0 :
  ✅ UDP 51820  (WireGuard)  — le SEUL port ouvert
  ✅ ICMP echo (diagnostic)
  ❌ TOUT le reste, y compris SSH

Sur wg0 :
  ✅ TCP 22    SSH
  ✅ TCP 8765  foxglove_bridge
  ✅ TCP 8080  page d'état
  ❌ DDS (7400-7500) — bloqué explicitement, même sur le VPN

Sortant : autorisé (NTP, mises à jour) — restreignable en exploitation
```

**SSH** : authentification par clé uniquement (`PasswordAuthentication no`, `PermitRootLogin no`), écoute sur `wg0` uniquement (`ListenAddress 10.42.0.1`).

⚠️ **Pourquoi bloquer DDS même sur le VPN** : parce que la tentation d'étendre le graphe « juste pour ce test » est forte, et que ça finit par rester. Le blocage explicite oblige à un geste conscient (§O.5).

## O.5 Les trois modes réseau

| Mode | Découverte | Pont | Usage |
|---|---|---|---|
| **TERRAIN** (défaut) | `LOCALHOST` | Foxglove WebSocket + SSH sur VPN | Exploitation normale |
| **ATELIER** | `LOCALHOST` + `ROS_STATIC_PEERS` vers le poste, DDS ouvert sur wg0 | RViz2 natif, `ros2 topic echo`, `rqt` | Développement, filaire de préférence |
| **AUTONOME** | `LOCALHOST` | Aucun | Aucun réseau. Le robot fonctionne seul. |

Le basculement se fait par une variable dans `/etc/retriever/network-mode` et un redémarrage du service. **Le mode ATELIER n'est jamais le mode par défaut.**

## O.6 Politique de perte de réseau

⚠️ **C'est une décision d'exploitation, pas une décision technique.** Je propose une politique et il faut l'assumer explicitement :

| Situation | Politique retenue 📐 | Justification |
|---|---|---|
| Mission **autonome** en cours, réseau perdu | ✅ **La mission continue** | Le robot est conçu pour être autonome. S'arrêter au milieu d'un champ hors de portée radio est pire que finir la mission. Un bag complet est activé automatiquement. |
| Mission **téléopérée**, réseau perdu | ❌ **`STOPPING` immédiat** (500 ms) | Une téléopération sans opérateur n'a aucun sens |
| Réseau perdu > 5 min en mission autonome | 🟡 **Retour au point de départ** | Limite de sécurité |
| Réseau perdu au démarrage | ✅ Le robot démarre quand même, reste en `READY` | Permet un démarrage sans infrastructure |

⚠️ **Coupure des commandes distantes** : un service `/retriever/disable_remote` désactive `foxglove_bridge` et la téléop, en gardant SSH. Utile si on soupçonne un problème côté opérateur.

## O.7 Résumé des ports

| Port | Protocole | Interface | Rôle |
|---|---|---|---|
| 51820 | UDP | wlan0/eth0 | WireGuard — **seul port exposé** |
| 22 | TCP | wg0 | SSH par clé |
| 8765 | TCP | wg0 | foxglove_bridge |
| 8080 | TCP | wg0 | Page d'état HTTP (lecture seule) |
| 7400-7500 | UDP | ❌ bloqué partout | DDS |

---

# P. Organisation du code

## P.1 Arborescence

```
retriever/
├── README.md
├── docs/                              ← ce dossier d'architecture
│   ├── 00-index-A-B.md … 06-bom-fmea-schema-plan.md
│   ├── wiring/                        schémas, plans de câblage, photos
│   └── measurements/                  résultats des mesures M1–M12
│
├── ros2_ws/src/
│   ├── retriever_msgs/                ★ SOCLE : messages, services, actions
│   │   ├── msg/ SafetyState.msg PowerStatus.msg MotorStatus.msg
│   │   │        CellVoltages.msg SelftestReport.msg MissionState.msg
│   │   ├── srv/ RequestArm.srv ClearFault.srv RunSelftest.srv
│   │   └── action/ RunMission.action
│   │
│   ├── retriever_can/                 ★ SOCLE : protocole CAN partagé
│   │   ├── protocol.yaml              ◄── SOURCE UNIQUE DE VÉRITÉ
│   │   ├── scripts/generate.py        → protocol.hpp (C++) + protocol.h (C)
│   │   ├── include/retriever_can/     (généré, versionné)
│   │   └── test/                      tests d'encodage/décodage
│   │
│   ├── retriever_description/         URDF/Xacro, meshes, TF statiques
│   │   ├── urdf/ retriever.urdf.xacro base.xacro wheels.xacro sensors.xacro
│   │   │         ros2_control.xacro   ← plugin réel OU mock, par argument
│   │   ├── config/ physical_params.yaml   ◄── UNE SEULE définition des dimensions
│   │   └── meshes/
│   │
│   ├── retriever_hardware/            Plugin ros2_control SystemInterface
│   │   ├── src/retriever_system.cpp
│   │   ├── retriever_hardware_plugin.xml
│   │   └── test/ test_can_loopback.cpp
│   │
│   ├── retriever_bringup/             ★ Point d'entrée unique
│   │   ├── launch/
│   │   │   ├── retriever.launch.py    ← LE fichier maître (arguments)
│   │   │   ├── hardware.launch.py
│   │   │   ├── sensors.launch.py
│   │   │   ├── localization.launch.py
│   │   │   ├── navigation.launch.py
│   │   │   ├── supervision.launch.py
│   │   │   └── kinect.launch.py       ← isolé, jamais critique
│   │   ├── config/
│   │   │   ├── controllers.yaml  ekf_odom.yaml  ekf_map.yaml
│   │   │   ├── navsat.yaml  nav2_indoor.yaml  nav2_outdoor.yaml
│   │   │   ├── collision_monitor.yaml  twist_mux.yaml
│   │   │   └── diagnostics_analyzers.yaml
│   │   └── systemd/ retriever-can.service  retriever-bringup.service
│   │
│   ├── retriever_can_bridge/          CAN ↔ topics ROS
│   ├── retriever_safety_bridge/       Miroir de la FSM sécurité
│   ├── retriever_power_monitor/       Batterie, énergie, historique SQLite
│   ├── retriever_selftest/            Pipeline de diagnostic §K
│   ├── retriever_mission/             mission_manager
│   ├── retriever_drivers/             Wrappers de lancement des drivers tiers
│   ├── retriever_sim/                 Gazebo, mondes, ros2_control mock
│   └── retriever_bench/               Outils de test, HIL, analyse de bags
│
├── firmware/
│   ├── common/
│   │   ├── protocol.h                 (généré depuis protocol.yaml)
│   │   ├── can_driver.c/h
│   │   ├── watchdog.c/h
│   │   └── safety_limits.h            ◄── constexpr, NON modifiables par CAN
│   ├── esp32_safety/     platformio.ini  src/main.c  src/fsm.c  src/bms.c …
│   ├── esp32_motion/     platformio.ini  src/main.c  src/pi.c  src/hall.c …
│   └── test/                          tests unitaires hôte (Unity/Ceedling)
│
├── tools/
│   ├── flash_all.sh                   Flashe les 3 ESP32, vérifie les versions
│   ├── check_protocol_sync.py         ⚠️ CI : firmware ↔ ROS cohérents ?
│   ├── calibrate_wheels.py
│   ├── analyze_bag.py
│   └── thermal_check.md               Procédure de thermographie
│
├── .github/workflows/ci.yml           build + tests + vérif. de synchronisation
└── deploy/ ansible/                   Déploiement reproductible sur le X1
```

## P.2 Les quatre principes de cette organisation

### 1. Une source de vérité par donnée

| Donnée | Source unique | Consommateurs |
|---|---|---|
| Protocole CAN | `retriever_can/protocol.yaml` | Généré vers C++ et C |
| Dimensions du robot | `retriever_description/config/physical_params.yaml` | URDF, controllers.yaml, EKF, Nav2 |
| Limites de sécurité | `firmware/common/safety_limits.h` | Firmware uniquement — **jamais** dupliquées côté ROS |
| Paramètres de navigation | `retriever_bringup/config/nav2_*.yaml` | Nav2 |

⚠️ **La duplication de la définition des dimensions est le bug le plus fréquent des projets robotiques** : le rayon de roue est dans l'URDF *et* dans `controllers.yaml`, on en modifie un, l'odométrie devient fausse de 3 % et on cherche pendant deux jours. Ici, `physical_params.yaml` est lu par les deux.

### 2. Le firmware et le logiciel sont un seul projet

Un seul dépôt Git, un seul commit pour un changement de protocole. La CI vérifie que `protocol.h` est bien régénéré (`check_protocol_sync.py`). Le hash du fichier `protocol.yaml` est compilé dans le firmware et remonté dans le heartbeat, et l'auto-test [P3] refuse de démarrer en cas de divergence.

### 3. Un point d'entrée unique et paramétré

```bash
# Terrain, extérieur, navigation autonome
ros2 launch retriever_bringup retriever.launch.py

# Banc, mock hardware, sans capteurs réels
ros2 launch retriever_bringup retriever.launch.py use_mock_hardware:=true use_sensors:=false

# Simulation Gazebo
ros2 launch retriever_bringup retriever.launch.py use_sim:=true

# Intérieur avec SLAM
ros2 launch retriever_bringup retriever.launch.py environment:=indoor slam:=true

# Rejeu de bag
ros2 launch retriever_bringup retriever.launch.py use_bag:=/path/to/bag
```

Arguments : `use_mock_hardware`, `use_sim`, `use_sensors`, `use_kinect`, `environment` (indoor/outdoor), `slam`, `nav`, `bench_mode`, `use_bag`, `log_level`.

### 4. Ce qui est critique est simple, ce qui est complexe n'est pas critique

| Critique (C1/C2) | Complexe |
|---|---|
| `firmware/` — quelques milliers de lignes de C, sans allocation dynamique | Nav2, `slam_toolbox` — paquets tiers matures |
| `retriever_hardware` — ~500 lignes | `retriever_mission` — logique métier |
| `retriever_can` — généré | `kinect2_ros2` — isolé, jamais dans le chemin critique |

## P.3 Séparation configuration / paramètres / code

| Catégorie | Où | Modifiable à chaud ? |
|---|---|---|
| **Limites de sécurité** | `firmware/common/safety_limits.h` | ❌ Jamais — recompilation |
| **Configuration matérielle** (broches, calibres) | `firmware/*/config.h` + `retriever_description/config/` | ❌ Non |
| **Paramètres d'algorithme** (gains, seuils Nav2) | YAML dans `retriever_bringup/config/` | ✅ Oui, via `ros2 param` |
| **Paramètres de mission** | YAML + arguments de lancement | ✅ Oui |
| **Réglages runtime** (vitesse max de mission) | Services ROS | ✅ Oui, avec saturation firmware |

## P.4 Journalisation

| Type | Où | Format |
|---|---|---|
| Logs ROS 2 | `~/.ros/log/`, rotation systemd 500 Mo | texte |
| Bag d'événements permanent | `~/retriever_logs/bags/events/` | MCAP |
| Bag complet à la demande | `~/retriever_logs/bags/full/` | MCAP |
| Buffer pré-défaut (60 s) | RAM → disque sur `FAULT`/`ESTOP` | MCAP |
| Auto-tests | `~/retriever_logs/selftest_*.json` | JSON |
| Historique batterie | `~/retriever_logs/battery.db` | SQLite |
| Événements firmware | 32 derniers en RAM ESP32, rejoués au retour du X1 | CAN |

**Format MCAP** plutôt que SQLite3 pour les bags : meilleure performance d'écriture, lecture partielle possible, et c'est le format natif de Foxglove.

---

# Q. Stratégie de tests

## Q.1 Les huit niveaux

Chaque niveau **ne commence que si le précédent est passé**. C'est ce qui évite de chercher un problème logiciel qui est en réalité un problème de sertissage.

### Niveau 0 — Composants isolés (établi, sans batterie)

| Test | Méthode | Critère | Automatisable |
|---|---|---|---|
| Continuité de chaque câble confectionné | Ohmmètre, aux deux extrémités | < 10 mΩ | ❌ |
| Résistance de chaque sertissage | Micro-ohmmètre, ou chute de tension à 10 A | < 100 µΩ | ❌ |
| Arrachement d'une cosse sacrificielle | Effort à la main, puis dynamomètre si possible | Ne s'arrache pas | ❌ |
| **M12** — marquage des condensateurs ZS-X11H | Inspection visuelle | ≥ 63 V | ❌ |
| **M5** — état du cavalier J1 | Inspection visuelle | documenté | ❌ |
| Chaque ESP32 : flash + blink | `pio run -t upload` | OK | ✅ |
| Chaque ESP32 : boucle CAN sur soi-même | Test firmware intégré | trames reçues | ✅ |
| MCP4728 : rampe 0→5 V | Multimètre sur la sortie de l'AOP | linéaire, 0–5,00 V | ✅ (avec ADC de contrôle) |
| BNO085 : lecture SPI | Test firmware, quaternion normé | \|q\| = 1,00 ± 0,01 | ✅ |
| ACS758 : sortie au zéro | Multimètre, sans courant | 2,50 V ± 0,05 | ❌ |
| Chaque DC/DC : à vide et à charge nominale | Charge résistive, multimètre | ±3 % | ❌ |
| **M6** — comptage Hall sur un tour | Rotation manuelle, comptage firmware | 90 transitions | ✅ |
| **M7** — niveau de la sortie SC | Oscilloscope | documenté | ❌ |

### Niveau 1 — Rails électriques (alimentation de laboratoire, PAS la batterie)

⚠️ **Tout le niveau 1 se fait avec une alimentation de laboratoire limitée en courant**, jamais avec le pack. C'est la règle qui évite de transformer une erreur de câblage en incendie.

| Test | Méthode | Critère |
|---|---|---|
| Bus non alimenté : isolement + / − | Ohmmètre | > 100 kΩ |
| Alimentation à **12 V, limitée à 0,5 A** | Vérifier qu'aucun courant ne s'établit | I < 50 mA |
| Montée progressive 12 → 24 → 36 → 42 V, limite 1 A | Surveiller le courant à chaque palier | conforme au calcul |
| Rail 12 V COMPUTE à vide, puis 50 %, puis 100 % | Multimètre + charge | 12 V ±3 %, ondulation < 100 mV |
| Rail 12 V AUX, idem | | idem |
| Rail 5 V LOGIC, idem | | 5 V ±2 % |
| **Séquence de précharge** | Chronométrer la montée de V_bus | conforme au calcul (≈0,5 s) |
| **Timeout de précharge** | Provoquer un court-circuit **contrôlé** (résistance 1 Ω) en aval | `FAULT_PRECHARGE` en < 1,5 s |
| Décharge du bus | Chronométrer après ouverture | conforme |
| Consommation au repos, tout allumé, moteurs désarmés | Pince ampèremétrique | < 1,5 A à 36 V |
| **Thermographie après 30 min à charge** | Caméra IR | ΔT < 15 K entre jonctions identiques |

⚠️ **Seulement après tout cela, on connecte le pack** — et la première connexion se fait avec le pack à ~50 % de SoC, à l'extérieur, avec un extincteur adapté à portée.

### Niveau 2 — Communications

| Test | Méthode | Critère | Auto |
|---|---|---|---|
| `can0` monte | `ip link set can0 up` | pas d'erreur | ✅ |
| Trafic CAN nominal | `candump can0` pendant 60 s | 0 erreur, débit attendu | ✅ |
| Charge du bus | `canbusload can0@500000` | < 30 % | ✅ |
| Latence aller-retour | Ping applicatif, 1000 échantillons | p99 < 15 ms | ✅ |
| **Injection de trame corrompue** | `cansend` avec mauvais CRC | rejetée, compteur incrémenté | ✅ |
| **Injection de trame rejouée** | `cansend` avec seq non incrémentale | rejetée | ✅ |
| **Débranchement du bus en marche** | Physique | tous les nœuds en état sûr < 300 ms | ❌ |
| **Terminaison retirée** | Physique | erreurs détectées et remontées | ❌ |
| Dialogue BMS | Lecture registre 0x31 | valeurs plausibles | ✅ |
| Synchronisation temporelle | Comparaison sur 10 min | dérive < 5 ms | ✅ |
| **Divergence de version** | Flasher un nœud avec un ancien protocole | refus d'armement | ✅ |

### Niveau 3 — Capteurs

| Test | Critère | Auto |
|---|---|---|
| IMU : fréquence de `/imu/data` | 100 Hz ± 5 % | ✅ |
| IMU : quaternion normé | 1,00 ± 0,01 | ✅ |
| IMU : \|accélération\| au repos | 9,81 ± 0,3 m/s² | ✅ |
| IMU : biais gyro au repos, 60 s | < 0,02 rad/s | ✅ |
| **IMU : test des 6 faces** (robot sur chaque face) | Le vecteur gravité pointe dans la bonne direction — **valide l'orientation dans l'URDF** | ❌ |
| IMU : rotation 90° mesurée au rapporteur | Erreur < 3° | ❌ |
| Lidar : fréquence, taux de points valides | 6–12 Hz, > 60 % | ✅ |
| **Lidar : distance vraie sur mire** à 1 / 3 / 5 m | ±3 % | ❌ |
| GPS : fix, satellites, covariance non nulle | ≥ 8 sat, cov > 0 | ✅ |
| **GPS : dispersion statique 5 min** | σ < 3 m | ✅ |
| Kinect : flux couleur et profondeur | 30 fps ± 10 % | ✅ |
| **Cohérence TF** : le lidar voit un mur là où l'URDF le prédit | Erreur < 5 cm | ❌ |

### Niveau 4 — Actionneurs ⚠️ roues levées obligatoire

⚠️ **Le robot est sur cales, roues ne touchant pas le sol. Interrupteur en position BANC. Champignon à portée de main immédiate.**

| Test | Critère | Auto |
|---|---|---|
| **M4** — polarité de `EL` et `STOP` | documentée | ❌ |
| Rotation individuelle de chaque roue à 0,2 rad/s | Tourne, dans le bon sens | ✅ |
| **Sens de rotation** de chaque roue | Signe du comptage = signe de la consigne | ✅ |
| Rampe 0 → 1 rad/s → 0 | Suivi de consigne, erreur < 10 % | ✅ |
| Réponse indicielle | Retard < 200 ms, dépassement < 20 % | ✅ |
| **Temps d'arrêt en freinage** depuis 1 rad/s | < 500 ms | ✅ |
| Courant à vide par roue | 0,5–2,5 A 📐 | ✅ |
| Symétrie des courants | Écart < 30 % | ✅ |
| **Détection de calage** : bloquer une roue à la main (avec précaution) | `STALL` en < 300 ms | ❌ |
| **Watchdog** : `pkill retriever_can_bridge` en rotation | Arrêt en < 300 ms | ✅ |
| **`/SAFE`** : forcer la ligne à la masse | Freinage immédiat | ✅ |
| **Champignon** en rotation | Freinage puis contacteur ouvert ~1 s après | ❌ |
| Courant de régénération au freinage | Mesuré et journalisé, \|I\| < 15 A | ✅ |
| Température des dissipateurs après 10 min | < 60 °C | ✅ |

### Niveau 5 — Intégration ROS 2 (mock hardware, sans matériel)

| Test | Méthode | Auto |
|---|---|---|
| Tests unitaires `retriever_can` (encodage/décodage) | `colcon test` | ✅ |
| Tests unitaires firmware sur hôte | Unity/Ceedling | ✅ |
| `retriever_hardware` en boucle CAN virtuelle (`vcan0`) | Test d'intégration | ✅ |
| Lancement complet en `use_mock_hardware:=true` | Tous les nœuds actifs en < 20 s | ✅ |
| Arbre TF complet et frais | `tf2_ros` API | ✅ |
| Cohérence des paramètres | Comparaison YAML ↔ `ros2 param dump` | ✅ |
| **`cmd_vel` aberrant** (100 m/s, NaN, ∞) | Saturé ou rejeté, jamais propagé | ✅ |
| `cmd_vel_timeout` | Arrêt du publieur → vitesse à 0 | ✅ |
| Transitions de la FSM mission | Toutes les transitions autorisées + rejet des interdites | ✅ |
| Mort d'un lifecycle node | Bond détecte, groupe transite | ✅ |
| Rejeu de bag → EKF | Trajectoire cohérente, pas de divergence | ✅ |

### Niveau 6 — Système complet, roues levées

| Test | Critère | Auto |
|---|---|---|
| Boot complet, mise sous tension → `READY` | < 60 s | ✅ |
| Auto-test complet [P1]–[P7] | Verdict `READY` | ✅ |
| Téléopération à la manette | Réponse correcte, priorité respectée | ❌ |
| **Priorité `twist_mux`** : téléop pendant une navigation | La téléop gagne | ✅ |
| **`systemctl stop retriever-bringup` en rotation** | Arrêt < 500 ms, `FAULT`, cause correcte | ✅ |
| **Coupure brutale de l'alimentation du X1 en rotation** | Arrêt < 500 ms, `FAULT` | ❌ |
| **Débranchement du bus CAN en rotation** | Arrêt < 300 ms | ❌ |
| **Reset d'un ESP32-MOTION en rotation** | Arrêt, puis reprise après acquittement | ❌ |
| **Champignon en rotation** | Freinage, contacteur ~1 s, `ESTOP` | ❌ |
| **Perte du réseau en téléop** | `STOPPING` < 500 ms | ✅ |
| Autonomie : décharge complète en charge simulée | Courbe SOC vs temps journalisée | ✅ |
| **Thermographie après 30 min de fonctionnement** | ΔT < 15 K entre jonctions identiques | ❌ |

### Niveau 7 — Terrain

⚠️ **Progression obligatoire** : sol plat lisse → sol plat irrégulier → pente faible → terrain réel. **Champignon à portée à chaque étape.** Aucun tiers dans le périmètre.

| Test | Critère |
|---|---|
| Marche avant / arrière 10 m, ligne droite | Écart latéral < 30 cm |
| **Étalonnage `wheel_separation_multiplier`** : rotation 360° | Ajusté jusqu'à ±5° |
| Odométrie sur un carré de 5 m, retour au départ | Erreur de fermeture < 1 m |
| Comparaison odométrie / GPS sur 50 m | Cohérente |
| Franchissement d'obstacle (marche 3 cm) | Pas de calage, courant < 30 A |
| Pente 10 % en montée | Vitesse maintenue, courant journalisé |
| **Pente 10 % en descente** | ⚠️ Courant de régénération mesuré. Si > 15 A → revoir la stratégie de freinage |
| SLAM intérieur (`slam_toolbox`) | Carte cohérente, boucle fermée |
| Nav2 vers un objectif à 20 m | Atteint à ±30 cm |
| **Collision monitor** : obstacle placé sur la trajectoire | Arrêt avant contact |
| Mission par waypoints GPS, 3 points | Tous atteints |
| Mission complète 30 min | Aucune intervention, journal propre |

## Q.2 Automatisation

| Où | Quoi |
|---|---|
| **CI GitHub Actions** | Build ROS 2, `colcon test`, tests unitaires firmware, `check_protocol_sync.py`, lint |
| **Sur le robot, à chaque boot** | Auto-test [P1]–[P7] |
| **Sur le robot, à la demande** | `ros2 run retriever_bench run_level.py --level 4` (niveaux 2 à 6) |
| **Non automatisable** | Tout ce qui exige une action physique (champignon, blocage de roue, thermographie, débranchement) — **procédures écrites dans `docs/`, exécutées à chaque jalon** |

## Q.3 Simulation et non-divergence

**Le problème** : le code de simulation et le code réel divergent, et on finit avec un robot qui marche parfaitement en Gazebo et pas du tout dehors.

**Les cinq mécanismes qui l'empêchent** :

| # | Mécanisme | Détail |
|---|---|---|
| 1 | **Un seul URDF** | `ros2_control.xacro` choisit le plugin (`retriever_hardware` / `mock_components/GenericSystem` / `gz_ros2_control`) par **un argument xacro**, pas par un fichier séparé |
| 2 | **Un seul jeu de paramètres** | `controllers.yaml`, `ekf_*.yaml`, `nav2_*.yaml` sont **identiques** en simulation et en réel. Seul `use_sim_time` change |
| 3 | **Un seul point d'entrée de lancement** | `retriever.launch.py` avec des arguments |
| 4 | **La CI exécute les mêmes tests d'intégration** contre le mock hardware | Un test qui passe en CI passe sur le robot, ou c'est un bug de l'abstraction matérielle — pas du logiciel applicatif |
| 5 | **`retriever_hardware` a un mode `loopback`** | Il parle le vrai protocole CAN sur `vcan0` avec un simulateur de firmware. **Le protocole est donc testé, pas contourné.** |

⚠️ Le point 5 est le plus important. Un `mock_components/GenericSystem` teste ros2_control mais **pas le protocole CAN ni le firmware**. Le mode loopback sur `vcan0` teste toute la chaîne sauf l'électronique.

**Les cinq modes de test disponibles** :

| Mode | Commande | Ce qui est testé |
|---|---|---|
| **Mock** | `use_mock_hardware:=true` | ros2_control, Nav2, TF, FSM — pas de CAN |
| **Loopback CAN** | `use_mock_hardware:=false can_interface:=vcan0` + simulateur firmware | + protocole CAN + logique firmware |
| **Gazebo** | `use_sim:=true` | + physique, capteurs simulés, Nav2 en boucle fermée |
| **HIL** | Vrai ESP32 sur `vcan0` via un adaptateur, moteurs déconnectés | + firmware réel |
| **Rejeu** | `use_bag:=...` | Localisation et perception sur données réelles |

**Progression de test des moteurs**, dans l'ordre, jamais sauté :

```
1. Sans moteurs (variateurs débranchés, mesure à l'oscilloscope de VR)
2. Un moteur sur établi, alimentation de labo 24 V limitée à 3 A
3. Quatre moteurs, roues LEVÉES, batterie, mode BANC
4. Roues au sol, robot RETENU par une sangle, vitesse ≤ 0,3 m/s
5. Roues au sol, libre, sol plat, vitesse ≤ 0,5 m/s
6. Terrain
```

---

# R. Architecture physique du robot

## R.1 Zonage

```
 VUE DE DESSUS                                 ↑ AVANT
 ┌───────────────────────────────────────────────────────────────────┐
 │  ┌──── ZONE CAPTEURS (mât / partie haute) ────────────────────┐   │
 │  │  GPS (antenne, le plus haut, dégagé)                       │   │
 │  │  BNO085 (mât nylon, ≥25 cm au-dessus des busbars)  ⚠️      │   │
 │  │  YDLIDAR X4 (360° dégagé)   Kinect v2 (vers l'avant)       │   │
 │  └────────────────────────────────────────────────────────────┘   │
 │                                                                   │
 │  ┌── ZONE COMPUTE ─────────┐   ┌── ZONE COMMUNICATION ────────┐   │
 │  │  Youyeetoo X1           │   │  hub USB 3.0 alimenté        │   │
 │  │  SSD M.2                │   │  adaptateur USB-CAN          │   │
 │  │  ventilateur soufflage  │   │  antennes Wi-Fi (déportées)  │   │
 │  └─────────────────────────┘   └──────────────────────────────┘   │
 │  ═════════════ CLOISON MÉTALLIQUE RELIÉE À LA MASSE ═══════════   │
 │  ┌── ZONE PUISSANCE ────────────────────────────────────────┐     │
 │  │  Busbars + / − (capotés, superposés)                     │     │
 │  │  4 × ZS-X11H sur plaque dissipatrice                     │     │
 │  │  contacteur DC · précharge · fusibles MIDI · DC/DC ×3    │     │
 │  │  ESP32-SAFETY · ESP32-MOTION AV · ESP32-MOTION AR        │     │
 │  │  ventilateur extraction                                  │     │
 │  └──────────────────────────────────────────────────────────┘     │
 │  ═════════════ CLOISON THERMIQUE ═══════════════════════════════  │
 │  ┌── ZONE BATTERIE (isolée, ventilée séparément) ──────────┐      │
 │  │  Pack M365 sanglé sur berceau + mousse EVA              │      │
 │  │  MRBF 80 A sur borne +                                  │      │
 │  │  ⚠️ AUCUN flux d'air venant de la zone puissance        │      │
 │  └─────────────────────────────────────────────────────────┘      │
 │                                                                   │
 │  ┌── FAÇADE ARRIÈRE (accessible sans outil) ───────────────┐      │
 │  │  🔴 CHAMPIGNON  🔑 COUPE-BATTERIE  🔘 MISE SOUS PUISS.  │      │
 │  │  🔑 BANC/TERRAIN   💡 LED état   💡 LED BUS SOUS TENSION│      │
 │  └─────────────────────────────────────────────────────────┘      │
 └───────────────────────────────────────────────────────────────────┘
   ZONE MOTEURS : 4 moteurs-roues aux 4 coins, hors coffret
```

## R.2 Séparation puissance / signaux — règles de cheminement

| Règle | Détail |
|---|---|
| **Deux chemins de câbles distincts** | Goulotte « puissance » et goulotte « signal », séparées d'au moins **10 cm** |
| **Croisement à 90°** | Si un croisement est inévitable, il se fait perpendiculairement — le couplage inductif est minimal |
| ❌ **Jamais de parallélisme** puissance/signal sur plus de 20 cm | C'est la règle la plus violée et la plus coûteuse |
| **Phases moteur torsadées par 3** | Le champ des trois phases s'annule en grande partie |
| **Câbles Hall torsadés et blindés**, blindage à la masse **côté variateur uniquement** | |
| **CAN en paire torsadée blindée**, blindage à la masse **côté X1 uniquement** | |
| **USB : câbles courts, avec ferrite** aux deux extrémités | |
| **Cloison métallique** entre zone puissance et zone compute, reliée à la masse | Écran électrostatique |
| **Aucun câble ne passe au-dessus du pack** | Un câble qui chauffe au-dessus d'un pack lithium |
| **Boucles minimales** : aller et retour du même circuit côte à côte | Réduit la surface de boucle donc l'émission et la susceptibilité |

## R.3 Thermique

| Zone | Stratégie |
|---|---|
| **Variateurs** | Montés sur une **plaque alu commune** (dissipateur), avec pâte thermique. Ventilateur en **extraction** au-dessus. NTC 10 kΩ collée sur la plaque. |
| **Compute** | Ventilateur en **soufflage**, air frais pris à l'extérieur. Le X1 a son propre dissipateur + ventilateur. |
| **Batterie** | ⚠️ **Ventilation séparée**, air ambiant uniquement. **Aucun apport d'air chaud.** Objectif : < 40 °C. |
| **Busbars** | Dans le flux d'extraction. 2 W/m à 25 A : peu de contrainte, mais l'air circulant aide à détecter un point chaud. |
| **DC/DC** | Montés sur la plaque alu si possible, sinon avec dégagement. |
| **Étanchéité vs refroidissement** | ⚠️ Compromis. En extérieur : **IP54 avec ventilateurs filtrés et chicanes**, drainage par le bas. Un boîtier IP67 totalement fermé exigerait un refroidissement par conduction sur les parois — envisageable en phase 4. |

## R.4 Maintenance et accessibilité

| Élément | Exigence |
|---|---|
| Coupe-batterie, champignon, interrupteur BANC | **Accessibles sans outil, depuis l'extérieur** |
| Fusibles | Accessibles en retirant **un seul capot**, sans dépose |
| Busbars | Capot transparent → inspection visuelle **sans démontage** |
| ESP32 | Ports USB accessibles pour reflasher **sans dépose** |
| Pack batterie | Extractible en < 5 min pour recharge/remplacement |
| SSD M.2 | Accessible |
| Câbles | **Longueur suffisante pour sortir chaque module de 15 cm** sans débrancher les autres |
| Étiquetage | Chaque câble et chaque connecteur étiqueté aux deux extrémités |

## R.5 Codes couleur et détrompage

### Couleurs

| Fonction | Couleur | Section typique |
|---|---|---|
| **BUS+ (30–42 V)** | **Rouge** | 25 / 6 / 2,5 mm² |
| **BUS− / masse de puissance** | **Noir** | idem |
| 12 V COMPUTE + | Orange | 1,5 mm² |
| 12 V AUX + | Jaune | 1,5 mm² |
| 5 V LOGIC + | Rose | 1,0 mm² |
| Masse logique | **Bleu** (⚠️ **pas noir** — pour distinguer visuellement de la masse de puissance) | 1,0 mm² |
| CAN H | Blanc | 0,34 mm² |
| CAN L | Vert | 0,34 mm² |
| Signaux moteur (VR/DIR/EL/STOP) | Gris | 0,25 mm² |
| Hall | Selon standard hoverboard (noir/vert/jaune/blanc/rouge) | 0,25 mm² |
| Phases moteur | Selon standard hoverboard (jaune/vert/bleu) | 6 mm² |
| Sécurité (`/SAFE`, champignon) | **Violet** | 0,5 mm² |
| ❌ **Vert/jaune** | **INTERDIT** — réservé à la terre de protection | — |

### Détrompage

| Liaison | Connecteur | Détrompage |
|---|---|---|
| Puissance batterie | Anderson SB50 **rouge** | Boîtier codé couleur |
| Puissance variateur | Cosse à œil M6 | Impossible d'inverser (busbars séparés physiquement) |
| CAN | Molex Micro-Fit 4 pos | Mécanique + couleur |
| Alim capteurs 5 V | JST-XH 2 pos **blanc** | Mécanique |
| Alim capteurs 12 V | JST-XH 2 pos **noir** | ⚠️ **Boîtier de couleur différente du 5 V** — c'est ce qui empêche de brancher un capteur 5 V sur le 12 V |
| Signaux moteur | JST-PH 6 pos | Mécanique |
| Hall | JST-PH 5 pos | Standard hoverboard |
| Sécurité | Molex Micro-Fit 2 pos **violet** | Mécanique + couleur |

⚠️ **La règle absolue : deux connecteurs de fonctions différentes ne doivent jamais pouvoir s'échanger.** Si deux liaisons utilisent le même modèle de connecteur, il faut changer le nombre de positions, le boîtier, ou la couleur. Le jour où on remonte le robot à 23 h après une réparation, c'est ce qui évite de détruire un capteur.

## R.6 Étiquetage

Format : `<ZONE>-<FONCTION>-<INDEX>`, sur étiquette thermorétractable imprimée, **aux deux extrémités** de chaque câble.

| Exemple | Signification |
|---|---|
| `PWR-BUS+-01` | Bus positif, liaison 1 |
| `PWR-MOT-AVG` | Puissance moteur avant gauche |
| `SIG-CAN-02` | Segment CAN 2 |
| `SIG-HALL-ARD` | Hall arrière droit |
| `SAF-ESTOP-01` | Boucle d'arrêt d'urgence |
| `LOG-5V-03` | Alimentation logique 5 V, dérivation 3 |

Le busbar porte une **étiquette par poste**. Un **schéma de câblage plastifié** est fixé à l'intérieur du capot — c'est ce qui rend le robot maintenable dans six mois.

## R.7 Plan de maintenance

| Périodicité | Action |
|---|---|
| **Avant chaque sortie** | Auto-test complet · test actif du champignon · inspection visuelle des câbles · SOC > 40 % |
| **Toutes les 10 h de fonctionnement** | Contrôle visuel des marques témoins de serrage · nettoyage des filtres de ventilation · vérification de la fixation du pack |
| **Toutes les 50 h** | ⚠️ **Thermographie sous charge** de toutes les jonctions de puissance · contrôle au couple des busbars · lecture de l'historique batterie (Δ cellules, R interne) |
| **À 48 h après montage, puis tous les 6 mois** | ⚠️ **Recouple de tous les boulons de busbar** (fluage du cuivre) |
| **Annuel** | Contrôle complet, remplacement des gaines abîmées, révision du pack |
| **Sur événement** | Après tout `FAULT` de surintensité : thermographie **obligatoire** avant remise en service |

⚠️ **La thermographie est le seul moyen de voir venir une jonction qui se dégrade.** Un ΔT de plus de 15 K entre deux jonctions identiques sous la même charge signale un problème, bien avant que la panne ne survienne. Un thermomètre IR à 30 € suffit pour démarrer.

---

*Suite : `06-bom-fmea-schema-plan.md` — sections S, T, U, V.*
