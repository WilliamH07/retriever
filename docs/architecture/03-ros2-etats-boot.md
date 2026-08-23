# I. Architecture ROS 2 Jazzy

## I.1 Nœuds retenus — et ceux que j'écarte

Le cahier des charges propose 18 nœuds. Tous ne sont pas nécessaires, et certains sont déjà fournis par des paquets existants qu'il serait absurde de réécrire.

| Nœud proposé | Décision | Justification |
|---|---|---|
| `robot_bringup` | ✅ **Paquet, pas nœud** | Ce sont des fichiers de lancement et des paramètres |
| `hardware_interface` | ✅ **Plugin, pas nœud** | `retriever_hardware` est un plugin `SystemInterface` chargé par `controller_manager` |
| `motor_controller` | ❌ **Supprimé** | `diff_drive_controller` (packagé) + firmware ESP32 couvrent tout |
| `imu_driver` | ✅ **Absorbé** | Publié par `retriever_can_bridge` depuis les trames CAN 0x210/0x211. Un nœud dédié n'existe qu'en mode repli UART-RVC |
| `lidar_driver` | ✅ Conservé | `ydlidar_ros2_driver` — ⚠️ **pas de release binaire Jazzy**, build source obligatoire |
| `kinect_driver` | 🟡 Conservé, isolé | `kinect2_ros2` à porter de Humble. Lancé séparément, jamais dans le bringup critique |
| `gps_driver` | ✅ Conservé | `nmea_navsat_driver` (packagé Jazzy ✅) |
| `battery_monitor` | ✅ **Fusionné** | Avec `power_monitor` → un seul nœud `retriever_power_monitor` |
| `power_monitor` | ✅ fusionné ci-dessus | La séparation créerait deux consommateurs des mêmes trames CAN |
| `diagnostics` | ✅ **Configuration, pas nœud** | `diagnostic_aggregator/aggregator_node` (packagé) + un YAML |
| `safety_manager` | ✅ **Renommé `retriever_safety_bridge`** | ⚠️ Nom important : ce nœud **ne gère pas** la sécurité. Il en est le **miroir en lecture** et le relais des demandes. L'autorité est dans l'ESP32-SAFETY. Un nœud nommé « safety_manager » sur un PC non temps réel donne une fausse impression de garantie. |
| `watchdog` | ❌ **Supprimé comme nœud** | Un watchdog qui tourne dans le même processus/OS que ce qu'il surveille ne surveille rien. Les watchdogs sont : matériel (TPL5010), firmware (ESP32), et `nav2_lifecycle_manager` (bond) pour les nœuds ROS |
| `teleop` | ✅ Conservé | `teleop_twist_joy` + `twist_mux` (packagés) |
| `navigation` | ✅ Conservé | Nav2 complet |
| `localization` | ✅ Conservé | `robot_localization` double EKF + `navsat_transform_node` |
| `mapping` | 🟡 Phase 1–2 seulement | `slam_toolbox` en intérieur. ❌ Inadapté en extérieur (§N.1) |
| `mission_manager` | ✅ Conservé | Nœud custom, machine à états de mission |
| `system_monitor` | ✅ **Remplacé** | `diagnostic_common_diagnostics` fournit CPU/RAM/disque/NTP/`sensors` — précieux pour le throttling thermique au soleil |

**Nœuds ajoutés qui ne figuraient pas dans la liste initiale** :

| Nœud | Rôle | Pourquoi il est nécessaire |
|---|---|---|
| `retriever_can_bridge` | Traduit les trames CAN non-moteur en topics ROS 2 (IMU, puissance, batterie, état sécurité, thermique) | Sans lui, ces données restent dans le bus CAN |
| `retriever_selftest` | Exécute le pipeline de diagnostic au démarrage et publie le verdict | §K |
| `twist_mux` | Priorité e-stop > téléop > navigation | ⚠️ Indispensable : sans lui, Nav2 et la téléop se battent sur `/cmd_vel` |
| `twist_stamper` | Conversion `Twist` → `TwistStamped` | ⚠️ Breaking change Jazzy (§H.1) — sauf si on active `enable_stamped_cmd_vel` partout dans Nav2 |
| `nav2_collision_monitor` | Zone d'arrêt/ralentissement basée capteurs | Filet de sécurité logiciel devant `diff_drive_controller` |
| `foxglove_bridge` | Pont WebSocket vers le poste distant | §O — évite d'étendre le graphe DDS sur le Wi-Fi |

## I.2 Fiches détaillées des nœuds

### `controller_manager` + `retriever_hardware`

| | |
|---|---|
| **Responsabilité** | Boucle `read() → update() → write()`, gestion du cycle de vie des composants matériels et des contrôleurs |
| **Paquet** | `controller_manager` (4.45.2 sur Jazzy ✅) + plugin custom `retriever_hardware/RetrieverSystem` |
| **Publie** | `/dynamic_joint_states`, `/joint_states`, `/diagnostics` |
| **Souscrit** | — (les contrôleurs souscrivent) |
| **Services** | `~/list_controllers`, `~/switch_controller`, `~/set_hardware_component_state` |
| **Paramètres** | `update_rate: 100`, `thread_priority: 50`, `cpu_affinity: 2` 📐, `can_interface: can0`, `can_timeout_ms: 150` |
| **Fréquence** | **100 Hz** |
| **Dépendances** | `can0` up, ESP32-MOTION en heartbeat |
| **Criticité** | **C1** |
| **Comportement en perte** | `read()` retourne `ERROR` si aucune trame `FB_WHEELS` depuis 150 ms → le `controller_manager` **désactive tous les contrôleurs** utilisant ces interfaces (comportement documenté ✅) |

⚠️ **Point d'architecture** : `controller_manager` **n'est pas un lifecycle node** — c'est un `rclcpp::Node` classique qui gère lui-même le cycle de vie de ses composants. Il ne peut donc **pas** être piloté par `nav2_lifecycle_manager`. Il faut le séquencer via ses propres services. C'est la principale friction dans un démarrage par étapes homogène (§K).

### `diff_drive_controller`

| | |
|---|---|
| **Responsabilité** | Cinématique différentielle, odométrie, limites |
| **Publie** | `~/odom` (`nav_msgs/Odometry`) — ⚠️ **`enable_odom_tf: false`** (c'est l'EKF qui publie `odom→base_link`) |
| **Souscrit** | `~/cmd_vel` (`geometry_msgs/TwistStamped` — **obligatoire sur Jazzy** ✅) |
| **Paramètres clés** | `left_wheel_names: [av_gauche_joint, ar_gauche_joint]`, `right_wheel_names: [av_droite_joint, ar_droite_joint]`, `wheel_separation`, `wheel_radius: 0.0825`, `wheel_separation_multiplier` 📐 **à étalonner**, `cmd_vel_timeout: 0.5`, `publish_rate: 50.0`, `linear.x.max_velocity: 1.5`, `linear.x.max_acceleration: 1.0` 📐, `linear.x.max_deceleration: 1.2` 📐 |
| **Fréquence** | 100 Hz (contrôleur), 50 Hz (publication) |
| **Criticité** | **C1** |
| **Comportement en perte de commande** | `cmd_vel_timeout` → arrêt automatique. ⚠️ **Ce n'est pas un e-stop**, juste un watchdog de commande |

⚠️ **Étalonnage `wheel_separation_multiplier`** : en skid-steer, le glissement latéral fait que l'écartement *effectif* est supérieur à l'écartement géométrique. Procédure : commander une rotation de 360°, mesurer la rotation réelle, ajuster le multiplicateur. **Et malgré cela, ne pas faire confiance au yaw odométrique** — il sera fusionné avec un poids très faible, ou pas du tout (§N.2).

### `retriever_can_bridge`

| | |
|---|---|
| **Responsabilité** | Traduire les trames CAN non-moteur en topics ROS 2, et inversement pour les demandes |
| **Publie** | `/imu/data` (100 Hz) · `/retriever/power` (20 Hz) · `/battery_state` (2 Hz) · `/retriever/cells` (0,2 Hz) · `/retriever/safety_state` (100 Hz, latched) · `/retriever/thermal` (1 Hz) · `/diagnostics` |
| **Souscrit** | `/retriever/estop_request` · `/retriever/arm_request` |
| **Services** | `~/request_arm`, `~/request_disarm`, `~/request_estop` |
| **Paramètres** | `can_interface`, `time_sync_period: 1.0`, `imu_frame: imu_link`, `imu_orientation_covariance` 📐 |
| **Fréquence** | Événementiel (réception CAN) + 1 Hz pour `TIME_SYNC` |
| **Criticité** | **C2** |
| **Comportement en perte** | Le bridge meurt → `nav2_lifecycle_manager` (bond) détecte → arrêt de la pile de navigation. Côté ESP32 : perte du heartbeat X1 → `FAULT` |

⚠️ **La covariance de l'IMU doit être renseignée.** La datasheet BNO085 donne : Rotation Vector erreur dynamique **3,5°**, statique **2,0°**, « en pratique typiquement **5°** » ✅. Utiliser 5° (0,0873 rad) → variance ≈ 7,6e-3 rad². Une covariance à zéro ferait diverger l'EKF.

### `retriever_safety_bridge`

| | |
|---|---|
| **Responsabilité** | **Miroir en lecture** de la machine à états de sécurité de l'ESP32. Traduit les demandes ROS en trames CAN. Publie l'état pour l'IHM et les diagnostics. **Aucune autorité.** |
| **Publie** | `/retriever/safety_state`, `/retriever/estop_active` (Bool, latched), `/diagnostics` |
| **Souscrit** | `/retriever/safety_state_raw` (depuis le bridge CAN) |
| **Services** | `/retriever/arm` (Trigger), `/retriever/disarm`, `/retriever/clear_fault` |
| **Paramètres** | `arm_timeout: 2.0`, `require_operator_confirmation: true` |
| **Fréquence** | 20 Hz |
| **Criticité** | **C2** (informationnel — sa perte ne crée pas de danger) |
| **Comportement en perte** | L'IHM ne voit plus l'état de sécurité → `WARN`. La sécurité physique n'est pas affectée. |

### `retriever_power_monitor`

| | |
|---|---|
| **Responsabilité** | Agrège tension/courant/SOC/cellules, calcule l'énergie consommée, gère les seuils d'alerte, journalise |
| **Publie** | `/battery_state` (`sensor_msgs/BatteryState`), `/retriever/power_report`, `/diagnostics` |
| **Souscrit** | `/retriever/power`, `/retriever/cells` |
| **Paramètres** | `soc_warn: 30`, `soc_return_home: 20`, `soc_stop: 10`, `cell_delta_warn_mv: 100`, `cell_delta_crit_mv: 300`, `v_pack_min: 30.0`, `log_path` |
| **Fréquence** | 2 Hz |
| **Criticité** | **C2** |
| **Comportement en perte** | Pas d'estimation d'autonomie → `WARN`. La protection dure reste dans l'ESP32. |

⚠️ **Sur le seuil de déséquilibre** : le firmware BMS d'origine considère 30 mV comme « déséquilibre dangereux » et 800 mV comme « critique » 🟡. Je propose des seuils ROS plus conservateurs pour l'alerte (100 mV) car un pack de trottinette réutilisé a probablement déjà vieilli de façon inégale. **Un delta croissant dans le temps est le signal d'alarme le plus important d'un pack lithium.** D'où l'historique.

### `robot_localization` — deux instances + `navsat_transform_node`

Voir §N.2 pour le détail. Résumé :

| Instance | `world_frame` | Publie TF | Entrées |
|---|---|---|---|
| `ekf_odom` | `odom` | `odom → base_link` | odométrie roues (vitesses), IMU (gyro) |
| `ekf_map` | `map` | `map → odom` | odométrie roues, IMU (gyro + yaw), `/odometry/gps` |
| `navsat_transform` | — | — | `/gps/fix`, `/imu/data`, `/odometry/filtered/global` → publie `/odometry/gps` |

### `mission_manager`

| | |
|---|---|
| **Responsabilité** | Machine à états de **mission** (§J.3), séquencement des objectifs, gestion des dégradations, décisions de repli |
| **Publie** | `/retriever/mission_state`, `/retriever/mission_feedback` |
| **Souscrit** | `/retriever/safety_state`, `/diagnostics_agg`, `/battery_state` |
| **Actions** | Serveur `retriever_msgs/action/RunMission` · Client `nav2_msgs/action/NavigateToPose`, `FollowGPSWaypoints` |
| **Services** | `/retriever/mission/pause`, `/resume`, `/abort` |
| **Paramètres** | `max_mission_duration`, `return_home_on_low_battery: true`, `abort_on_degraded: false`, `network_loss_policy: continue` |
| **Fréquence** | 10 Hz |
| **Criticité** | **C3** |
| **Comportement en perte** | Nav2 termine l'objectif en cours puis reste inactif. Aucun danger. |

### Nav2 — configuration retenue

| Composant | Choix | Justification |
|---|---|---|
| Contrôleur local | **`nav2_mppi_controller`** | ✅ Doc annonce « 100+ Hz sur un i5 4ᵉ gén » — le N5105 est du même ordre. **Jazzy apporte +45 % de performance vs Iron** et surtout les contraintes d'accélération `ax_max`/`ax_min`/`ay_max` — critiques pour un 4WD sur terrain meuble (évite de commander des rampes que la mécanique ne peut pas suivre, ce qui provoque du patinage et détruit l'odométrie) |
| Devant MPPI | **`nav2_rotation_shim_controller`** | Un skid-steer tourne très bien sur place ; le shim exploite ça |
| Alternative CPU faible | `nav2_regulated_pure_pursuit_controller` | Très léger, robuste sur chemin GPS long. ⚠️ **Ne fait pas d'évitement d'obstacles** — à coupler impérativement au collision monitor |
| Contrôleur écarté | `nav2_dwb_controller` | Legacy, espace de recherche discret, mal adapté au terrain non structuré |
| Planificateur global | `nav2_navfn_planner` (intérieur) puis `nav2_smac_planner` Hybrid-A* (extérieur) | |
| **Collision monitor** | **`nav2_collision_monitor`** avec **`VelocityPolygon`** | ✅ Nouveauté Jazzy la plus utile ici : zone d'arrêt différente selon la plage de vitesse. Sur un 4WD qui va de 0,3 à 2 m/s, une zone fixe est soit bloquante à basse vitesse, soit dangereusement courte à haute vitesse. `source_timeout: 2.0` → arrêt si les données capteur cessent |
| Lisseur | `nav2_velocity_smoother` | Entre le contrôleur et le collision monitor |
| Waypoints GPS | `nav2_waypoint_follower` action **`FollowGPSWaypoints`** | ✅ Nouveauté Jazzy |
| Costmap | `always_send_full_costmap: false` | ✅ Utilise `CostmapUpdate.msg` — **économie de bande passante importante sur lien Wi-Fi** |
| ⚠️ | `visualize: false` sur MPPI en production | La doc insiste : coût CPU significatif |

**Chaîne cmd_vel complète** :

```
Nav2 controller_server ──/cmd_vel_nav──► velocity_smoother
                                              │ /cmd_vel_smoothed
                                              ▼
                                      collision_monitor
                                              │ /cmd_vel_raw
   téléop joystick ─────────────────────────► │
   e-stop logiciel (zéro) ──────────────────► │
                                              ▼
                                          twist_mux
                                       (priorités : 255 / 100 / 10)
                                              │ /cmd_vel
                                              ▼
                                        twist_stamper
                                              │ /diff_drive_controller/cmd_vel
                                              ▼
                                    diff_drive_controller
```

## I.3 Arbre TF

```
                     map
                      │  ekf_map (robot_localization)
                     odom
                      │  ekf_odom (robot_localization)
                  base_link                      ← centre géométrique, au sol
                      │
      ┌───────┬───────┼───────┬────────┬─────────┬──────────┐
      │       │       │       │        │         │          │
  base_footprint  imu_link  laser  camera_link  gps_link  wheel_*_link ×4
                     │                  │
                     │            ┌─────┴─────┐
                     │        rgb_optical  depth_optical
                     │
             ⚠️ monté LOIN des moteurs (magnétomètre)
```

**Règles** :
- Une seule source par arête TF. `diff_drive_controller` a `enable_odom_tf: false` — c'est `ekf_odom` qui publie `odom→base_link`.
- Conventions REP-103 : x avant, y gauche, z haut. Repères optiques en REP-105 (z avant, x droite, y bas).
- Toutes les transformations statiques sont dans l'URDF (`retriever_description`), pas dans des `static_transform_publisher` éparpillés dans les fichiers de lancement.

## I.4 Politique de QoS

| Topic | Fiabilité | Historique | Durabilité | Note |
|---|---|---|---|---|
| `/cmd_vel*` | RELIABLE | KEEP_LAST(1) | VOLATILE | |
| `/joint_states`, `/odom` | RELIABLE | KEEP_LAST(10) | VOLATILE | |
| `/imu/data` | **BEST_EFFORT** | KEEP_LAST(1) | VOLATILE | 100 Hz, une perte est sans conséquence |
| `/scan` | **BEST_EFFORT** | KEEP_LAST(1) | VOLATILE | QoS capteur standard |
| `/camera/**` | **BEST_EFFORT** | KEEP_LAST(1) | VOLATILE | |
| `/retriever/safety_state` | RELIABLE | KEEP_LAST(1) | **TRANSIENT_LOCAL** | Un nœud qui démarre doit connaître l'état immédiatement |
| `/battery_state` | RELIABLE | KEEP_LAST(1) | TRANSIENT_LOCAL | |
| `/diagnostics` | RELIABLE | KEEP_LAST(10) | VOLATILE | |
| `/tf` | RELIABLE | KEEP_LAST(100) | VOLATILE | |
| `/tf_static` | RELIABLE | KEEP_LAST(1) | **TRANSIENT_LOCAL** | |

⚠️ **Ne jamais mettre RELIABLE + grand historique sur un topic qui traverse le Wi-Fi** : les retransmissions saturent le canal et écroulent tout. C'est la raison principale pour laquelle on n'étend pas le graphe DDS sur le lien radio (§O).

## I.5 Schéma du graphe ROS 2

```
┌─ COUCHE MATÉRIELLE ────────────────────────────────────────────────────────┐
│                                                                            │
│  can0 ──┬──► controller_manager                                            │
│         │      └─ retriever_hardware (SystemInterface)                     │
│         │           ├─ diff_drive_controller ──► /diff_drive/odom          │
│         │           └─ joint_state_broadcaster ──► /joint_states           │
│         │                                                                  │
│         └──► retriever_can_bridge ──┬──► /imu/data          (100 Hz)       │
│                                     ├──► /retriever/power        (20 Hz)   │
│                                     ├──► /battery_state       (2 Hz)       │
│                                     ├──► /retriever/cells       (0,2 Hz)   │
│                                     ├──► /retriever/safety_state (100 Hz)  │
│                                     └──► /retriever/thermal       (1 Hz)   │
│                                                                            │
│  /dev/ttyUSB0 ──► ydlidar_ros2_driver ──► /scan            (7 Hz)          │
│  /dev/ttyACM0 ──► nmea_navsat_driver ──► /gps/fix          (1 Hz)          │
│  USB3         ──► kinect2_ros2 ──► /camera/**             (30 Hz) †        │
└────────────────────────────────────────────────────────────────────────────┘
                                    │
┌─ LOCALISATION ─────────────────────▼───────────────────────────────────┐
│  ekf_odom  ──► /odometry/filtered          ──► TF odom → base_link     │
│  navsat_transform ──► /odometry/gps                                    │
│  ekf_map   ──► /odometry/filtered/global   ──► TF map → odom           │
│  slam_toolbox (phase 1–2 intérieur) ──► /map                           │
└────────────────────────────────────────────────────────────────────────┘
                                    │
┌─ NAVIGATION ───────────────────────▼───────────────────────────────────┐
│  planner_server · controller_server (MPPI) · behavior_server           │
│  bt_navigator · waypoint_follower · smoother_server                    │
│  velocity_smoother ──► collision_monitor ──► twist_mux ──► /cmd_vel    │
│  nav2_lifecycle_manager (bond)                                         │
└────────────────────────────────────────────────────────────────────────┘
                                    │
┌─ SUPERVISION ──────────────────────▼───────────────────────────────────┐
│  mission_manager         ──► /retriever/mission_state                  │
│  retriever_safety_bridge ──► /retriever/estop_active                   │
│  retriever_power_monitor ──► /retriever/power_report                   │
│  retriever_selftest      ──► /retriever/selftest_report                │
│  diagnostic_aggregator   ──► /diagnostics_agg                          │
│  diagnostic_common_diagnostics (CPU, RAM, disque, températures)        │
│  foxglove_bridge (WebSocket 8765) · rosbag2 (enregistrement continu)   │
└────────────────────────────────────────────────────────────────────────┘

† lancé séparément, jamais dans le bringup critique
```

---

# J. Machine à états du robot

## J.1 Le principe : deux machines, une seule autorité

Il y a **deux** machines à états, et il est important de ne pas les confondre.

| | **FSM SÉCURITÉ** | **FSM MISSION** |
|---|---|---|
| Où | ESP32-SAFETY (firmware) | X1 (`mission_manager`) |
| Autorité | **Totale sur la puissance** | Aucune sur la puissance |
| Période | 1 kHz | 10 Hz |
| Persiste si le X1 plante | ✅ Oui | ❌ Non |
| Peut refuser une demande de l'autre | ✅ Oui | Non applicable |

La FSM mission **demande**. La FSM sécurité **décide**. Toute transition de la FSM mission vers un état permettant le mouvement est conditionnée à l'accord de la FSM sécurité.

## J.2 FSM SÉCURITÉ (ESP32-SAFETY) — autoritaire

```
                         ┌──────────┐
       mise sous         │   INIT   │  contacteur ouvert, /SAFE actif
       tension  ────────►│          │  auto-test matériel
                         └────┬─────┘
                              │ auto-test OK ET champignon relâché
                              ▼
                         ┌──────────┐
                    ┌───►│   SAFE   │  puissance coupée, tout est sûr
                    │    │          │  attend la demande de mise sous puissance
                    │    └────┬─────┘
                    │         │ demande CAN de mise sous puissance
                    │         │ (ou bouton physique de démarrage)
                    │         ▼
                    │    ┌──────────┐
                    │    │PRECHARGE │  relais précharge fermé
                    │    │          │  surveille V_bus, timeout 1,5 s
                    │    └────┬──┬──┘
                    │         │  └──── timeout / V_bus insuffisant
                    │         │         ──────────────────────────┐
                    │         │ V_bus ≥ 0,90 × V_pack             │
                    │         ▼                                   │
                    │    ┌──────────────┐                         │
                    │    │LIVE_DISARMED │  contacteur FERMÉ       │
                    │    │              │  /SAFE toujours ACTIF   │
                    │    │              │  moteurs interdits      │
                    │    └───┬──────▲───┘                         │
                    │        │      │ désarmement                 │
                    │        │      │ (demande ou inactivité 60 s)│
                    │        │ ARM_REQUEST reçue                  │
                    │        │ ET toutes conditions d'armement OK │
                    │        ▼      │                             │
                    │    ┌──────────┴───┐                         │
                    │    │  LIVE_ARMED  │  /SAFE RELÂCHÉ          │
                    │    │              │  moteurs autorisés      │
                    │    │              │  budget de courant actif│
                    │    └───┬──────────┘                         │
                    │        │ défaut détecté                     │
                    │        ▼                                    │
                    │    ┌──────────┐                             │
                    │    │ BRAKING  │  /SAFE actif, freinage      │
                    │    │          │  attente vitesse ≈ 0 (max 2s)│
                    │    └────┬─────┘                             │
                    │         ▼                                   │
                    │    ┌──────────┐                             │
                    └────┤  FAULT   │◄────────────────────────────┘
       acquittement      │          │  contacteur ouvert
       explicite         │          │  cause mémorisée et publiée
       + cause levée     └──────────┘
                              ▲
                              │
       ══════════════════════════════════════════════════════════
       ┌──────────────┐
       │  ESTOP       │  ATTEIGNABLE DEPUIS N'IMPORTE QUEL ÉTAT
       │              │  déclenché par : champignon (matériel, prioritaire)
       │              │                  watchdog externe
       │              │                  ESTOP_REQUEST CAN
       │              │  /SAFE actif immédiatement
       │              │  contacteur ouvert après ~1 s (RC matériel)
       │              │  SORTIE : uniquement après relâchement PHYSIQUE
       │              │           du champignon + acquittement explicite
       └──────────────┘  → retour en SAFE (jamais directement en ARMED)
```

### Conditions d'armement (toutes obligatoires)

| # | Condition | Vérifiée par |
|---|---|---|
| 1 | Champignon relâché (lu 3 fois consécutives, anti-rebond) | GPIO |
| 2 | V_pack dans [31 V, 42,5 V] | ADC |
| 3 | V_bus ≥ 0,95 × V_pack (contacteur bien fermé) | ADC |
| 4 | Courant au repos \|I\| < 2 A 📐 | ACS758 |
| 5 | Heartbeat des 2 ESP32-MOTION depuis < 300 ms | CAN |
| 6 | Heartbeat matériel du X1 présent | GPIO |
| 7 | Aucun drapeau `STALL` / `HALL_FAULT` actif | CAN |
| 8 | Températures dissipateurs < 60 °C | ADC |
| 9 | SOC batterie > 10 % (si le BMS répond) | UART |
| 10 | Watchdog externe rafraîchi | matériel |
| 11 | Aucun `FAULT` non acquitté | interne |

Le refus d'armement est **publié avec la liste des conditions non satisfaites** — c'est ce qui rend le système diagnosticable plutôt que frustrant.

### Sorties automatiques de `LIVE_ARMED`

| Déclencheur | Vers | Délai |
|---|---|---|
| Champignon enfoncé | `ESTOP` | < 10 ms |
| Perte du heartbeat X1 | `BRAKING` → `FAULT` | 300 ms |
| Perte du heartbeat d'un ESP32-MOTION | `BRAKING` → `FAULT` | 300 ms |
| Bus CAN en `bus-off` | `BRAKING` → `FAULT` | immédiat |
| Courant > 35 A | `BRAKING` → `FAULT` | < 20 ms |
| Courant > 30 A pendant > 3 s | `BRAKING` → `FAULT` | 3 s |
| V_bus < 29 V | `BRAKING` → `FAULT` | 100 ms |
| V_bus > 43 V (régénération) | Bridage du freinage, puis `FAULT` si persistant | 200 ms |
| Température dissipateur > 85 °C | `BRAKING` → `FAULT` | 1 s |
| Inclinaison > 40° (IMU) 📐 | `BRAKING` → `FAULT` | 500 ms |
| Aucune commande depuis 60 s | `LIVE_DISARMED` | 60 s |

## J.3 FSM MISSION (X1, `mission_manager`)

```
   OFF
    │  alimentation
    ▼
   BOOT ─────────► Linux démarre, services systemd
    │
    ▼
   INIT ─────────► ROS 2 démarre, nœuds en état `unconfigured`
    │              can0 monté, controller_manager lancé
    ▼
   SELFTEST ─────► pipeline de diagnostic §K
    │              ├─ résultat CRITICAL ──────────────► FAULT
    │              ├─ résultat FAIL ──────────────────► FAULT
    │              └─ résultat WARN ──────────────────► READY_DEGRADED
    ▼
   SAFE_IDLE ────► tout est démarré, puissance NON demandée
    │              L'opérateur peut inspecter, lire les diagnostics
    │  demande de mise sous puissance
    ▼
   READY ────────► FSM sécurité en LIVE_DISARMED
    │              Le robot peut accepter une mission
    │  demande d'armement (accordée par la FSM sécurité)
    ▼
   ARMED ────────► FSM sécurité en LIVE_ARMED
    │              Les moteurs peuvent bouger. Aucune mission en cours.
    │  mission acceptée
    ▼
   RUNNING ──────► Mission en cours
    │  ├─ dégradation non bloquante ──► RUNNING_DEGRADED ──► (peut revenir)
    │  ├─ mission terminée ───────────► STOPPING ──► ARMED
    │  ├─ pause demandée ─────────────► PAUSED ──► RUNNING
    │  └─ abandon / défaut ───────────► STOPPING ──► READY
    ▼
   STOPPING ─────► décélération contrôlée, attente vitesse nulle
    │
    ▼
   READY ou ARMED selon la cause

   ═══════════════════════════════════════════════════════
   DEGRADED   : superposable à READY / ARMED / RUNNING.
                Un sous-système est HS mais l'opération continue,
                avec des limites réduites.
   FAULT      : arrêt requis. Sortie par RECOVERY après acquittement.
   ESTOP      : miroir de l'état de la FSM sécurité. Aucun mouvement.
                Sortie uniquement après relâchement physique.
   RECOVERY   : tentative de remise en service automatique bornée
                (relance d'un driver, remontée de can0, reconfiguration
                d'un lifecycle node). 3 tentatives max, puis FAULT définitif.
   SHUTDOWN   : arrêt propre — mission annulée, moteurs à zéro, freinage,
                désarmement, ouverture du contacteur, flush des logs,
                puis `systemctl poweroff`.
```

## J.4 Table des transitions autorisées

| Depuis \ Vers | SAFE_IDLE | READY | ARMED | RUNNING | DEGRADED | STOPPING | FAULT | ESTOP | RECOVERY | SHUTDOWN |
|---|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| **SELFTEST** | ✅ | — | — | — | ✅ | — | ✅ | ✅ | — | ✅ |
| **SAFE_IDLE** | — | ✅¹ | — | — | ✅ | — | ✅ | ✅ | — | ✅ |
| **READY** | ✅ | — | ✅² | — | ✅ | — | ✅ | ✅ | — | ✅ |
| **ARMED** | — | ✅ | — | ✅³ | ✅ | ✅ | ✅ | ✅ | — | ✅ |
| **RUNNING** | — | — | — | — | ✅ | ✅ | ✅ | ✅ | — | ✅ |
| **DEGRADED** | ✅ | ✅ | ✅⁴ | ✅⁴ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| **STOPPING** | — | ✅ | ✅ | — | ✅ | — | ✅ | ✅ | — | ✅ |
| **FAULT** | — | — | — | — | — | — | — | ✅ | ✅⁵ | ✅ |
| **ESTOP** | ✅⁶ | — | — | — | — | — | ✅ | — | — | ✅ |
| **RECOVERY** | ✅ | — | — | — | ✅ | — | ✅⁷ | ✅ | — | ✅ |

¹ Nécessite la fermeture effective du contacteur (FSM sécurité en `LIVE_DISARMED`)
² Nécessite l'accord de la FSM sécurité (11 conditions, §J.2)
³ Nécessite une mission valide **et** `ARMED` confirmé côté sécurité
⁴ Uniquement si la dégradation est dans la liste des dégradations « tolérées en mouvement » (§M.4)
⁵ Acquittement explicite de l'opérateur, obligatoire
⁶ Uniquement après relâchement **physique** du champignon **et** acquittement
⁷ Après 3 tentatives infructueuses

**Transitions explicitement interdites, et pourquoi** :

| Interdit | Raison |
|---|---|
| `SELFTEST → ARMED` | Aucun armement sans passer par `READY` : l'opérateur doit avoir eu l'occasion de voir le rapport d'auto-test |
| `FAULT → READY` directement | Un défaut doit être **acquitté**, pas oublié. On passe par `RECOVERY`. |
| `ESTOP → ARMED` | Un arrêt d'urgence ramène toujours au repos, jamais directement à un état armé |
| `RUNNING → ARMED` sans `STOPPING` | Une mission interrompue doit décélérer proprement |
| Toute transition vers un état de mouvement si la FSM sécurité n'est pas `LIVE_ARMED` | Principe d'autorité |

## J.5 Implémentation

**Côté ESP32** : machine à états explicite en C, un `switch` sur l'énumération, évaluée à 1 kHz. Pas de RTOS-dépendance, pas d'allocation. L'état et sa cause sont émis à 100 Hz sur CAN (`0x010`).

**Côté ROS 2** : `mission_manager` implémente la FSM mission. Les nœuds de la pile utilisent les **lifecycle nodes** ; leur orchestration passe par `nav2_lifecycle_manager` (utilisable pour ses propres nœuds, pas seulement Nav2 ✅) avec son mécanisme de **bond** — qui détecte la mort d'un nœud managé et fait transiter tout le groupe. C'est exactement le comportement voulu : la perte d'un driver capteur arrête la navigation.

---

# K. Pipeline de boot et diagnostics

## K.1 Séquence complète

```
╔═══ T0 : mise sous tension (coupe-batterie fermé) ══════════════════════╗
║                                                                        ║
║  Alimenté : DC/DC 42→5 V SAFETY (amont contacteur) → ESP32-SAFETY      ║
║  NON alimenté : busbar aval contacteur, donc X1, variateurs, capteurs  ║
╚════════════════════════════════════════════════════════════════════════╝
   │
   ├─► ESP32-SAFETY : hw_init() → /SAFE ACTIF, contacteur OUVERT
   │   (état sûr établi AVANT toute autre chose)
   │
   ├─► ESP32-SAFETY : auto-test matériel  [S1]
   │     ADC lisible ? ACS758 au zéro ? V_pack plausible ? champignon lu ?
   │     watchdog externe répond ? EEPROM du DAC à zéro ?
   │
   ├─► ESP32-SAFETY : dialogue BMS  [S2]  (optionnel, non bloquant)
   │     SOC, tensions cellules, température, delta inter-cellules
   │
   ├─► ESP32-SAFETY → état SAFE. Voyant vert clignotant lent.
   │
   │   ⏸ ATTENTE : bouton physique « mise sous puissance » ou temporisation 3 s
   │
   ├─► SÉQUENCE DE PRÉCHARGE  [S3]
   │     relais précharge fermé → surveillance dV/dt de V_bus
   │     ├─ V_bus ≥ 0,90 × V_pack en < 1,5 s → OK
   │     └─ sinon → FAULT_PRECHARGE (= diagnostic de court-circuit)
   │
   ├─► Fermeture du contacteur, ouverture du relais de précharge
   │   État LIVE_DISARMED. /SAFE toujours ACTIF.
   │
╔══▼═════════════════════════════════════════════════════════════════════╗
║  T0 + ~5 s : le busbar est sous tension                                ║
║  → DC/DC 12 V COMPUTE → X1 démarre                                     ║
║  → DC/DC 12 V AUX → 5 V LOGIC → ESP32-MOTION × 2, capteurs             ║
╚════════════════════════════════════════════════════════════════════════╝
   │
   ├─► ESP32-MOTION × 2 : hw_init() → DAC à 0, EL asserté
   │     auto-test [S4] : DAC répond ? PCNT compte quand on tourne ? CAN OK ?
   │     → heartbeat CAN émis
   │
   ├─► X1 : BIOS → GRUB → noyau → systemd            (~25 s 📐)
   │
   ├─► systemd : retriever-can.service                    [S5]
   │     ip link set can0 up type can bitrate 500000 restart-ms 100
   │     ip link set can0 txqueuelen 1000
   │
   ├─► systemd : retriever-bringup.service (After=retriever-can.service)
   │
╔══▼═════════════════════════════════════════════════════════════════════╗
║  PIPELINE DE DIAGNOSTIC ROS 2 — retriever_selftest                     ║
╚════════════════════════════════════════════════════════════════════════╝
   │
   ├─ [P1] SYSTÈME       CPU, RAM, disque, températures, horloge      2 s
   ├─ [P2] ALIMENTATION  V_pack, V_bus, I_repos, SOC, cellules, T°    3 s
   ├─ [P3] COMMUNICATION can0, 3 heartbeats, erreurs CAN, latence     3 s
   ├─ [P4] CAPTEURS      IMU, lidar, GPS, (Kinect)                    8 s
   ├─ [P5] ACTIONNEURS   4 variateurs, test de rotation ⚠️            15 s
   ├─ [P6] SÉCURITÉ      champignon, /SAFE, watchdogs, contacteur     5 s
   └─ [P7] LOGICIEL      TF complète, nœuds actifs, params chargés    3 s
   │
   ▼
   VERDICT GLOBAL → READY  |  READY_DEGRADED  |  FAULT
```

⚠️ **[P5] n'est exécuté qu'avec les roues levées du sol** — le mode est choisi par un paramètre `selftest.allow_motion` **et** un interrupteur physique « BANC / TERRAIN » sur le robot. Voir §K.4.

## K.2 Les sept étapes du pipeline en détail

### [P1] Système

| Test | PASS | WARN | FAIL | CRITICAL |
|---|---|---|---|---|
| Charge CPU 1 min | < 2,0 | 2,0–3,5 | > 3,5 | — |
| RAM disponible | > 2 Go | 1–2 Go | < 1 Go | < 300 Mo |
| Espace disque `/` et `/var/log` | > 5 Go | 2–5 Go | < 2 Go | < 500 Mo |
| Température CPU | < 70 °C | 70–85 °C | > 85 °C | > 95 °C |
| Horloge système synchronisée | oui | dérive < 1 s | dérive > 1 s | — |
| Gouverneur CPU | `performance` | `ondemand` | — | — |

⚠️ Le disque plein est une cause classique et sournoise : `rosbag2` en enregistrement continu remplit un SSD en quelques heures. Le test **FAIL** à moins de 2 Go, et une rotation automatique des bags est configurée.

### [P2] Alimentation

| Test | PASS | WARN | FAIL | CRITICAL |
|---|---|---|---|---|
| V_pack | 34–42 V | 31–34 V | < 31 V ou > 42,5 V | < 30 V ou > 43 V |
| V_bus / V_pack (contacteur fermé) | > 0,98 | 0,95–0,98 | < 0,95 | < 0,90 |
| Courant au repos (moteurs désarmés) | < 1,5 A | 1,5–2,5 A | > 2,5 A | > 5 A |
| SOC (BMS) | > 40 % | 15–40 % | < 15 % | < 8 % |
| Δ inter-cellules | < 50 mV | 50–150 mV | 150–400 mV | > 400 mV |
| Température BMS | 5–40 °C | 0–5 / 40–50 °C | < 0 / > 50 °C | > 60 °C |
| Rail 12 V COMPUTE | 11,5–12,5 V | ±5 % | hors ±8 % | — |
| Rail 12 V AUX | 11,5–12,5 V | ±5 % | hors ±8 % | — |
| Rail 5 V LOGIC | 4,9–5,1 V | ±3 % | hors ±5 % | — |
| Réponse du BMS | oui | pas de réponse | — | — |

⚠️ **Le courant au repos est un test remarquablement révélateur.** Moteurs désarmés, il devrait valoir la consommation de l'électronique (~1 A). S'il monte à 3 A, c'est qu'un variateur consomme anormalement, ou qu'un court-circuit partiel se développe. C'est le test le moins cher et le plus utile de tout le pipeline.

⚠️ **Le delta inter-cellules est l'indicateur de santé n°1 d'un pack lithium réutilisé.** Le firmware BMS d'origine considère 30 mV comme « déséquilibre dangereux » 🟡 — je propose des seuils plus tolérants pour un pack usagé, mais **l'évolution dans le temps** compte plus que la valeur absolue. D'où l'historique (§N.6).

### [P3] Communication

| Test | PASS | WARN | FAIL | CRITICAL |
|---|---|---|---|---|
| `can0` UP | oui | — | non | — |
| Erreurs CAN (fenêtre 3 s) | 0 | 1–5 | > 5 | bus-off |
| Heartbeat ESP32-SAFETY | < 150 ms | 150–300 ms | absent | absent |
| Heartbeat ESP32-MOTION-AV | < 150 ms | 150–300 ms | absent | — |
| Heartbeat ESP32-MOTION-AR | < 150 ms | 150–300 ms | absent | — |
| Latence aller-retour CAN (ping applicatif) | < 5 ms | 5–15 ms | > 15 ms | — |
| Version du protocole des 3 nœuds | identique | — | **divergente** | — |
| Ports USB attendus présents | tous | 1 manquant | > 1 manquant | — |
| Interfaces réseau | eth0 ou wlan0 UP | — | aucune | — |

⚠️ **Le test de version de protocole est essentiel.** Le mode de défaillance le plus pénible d'un système embarqué distribué est le firmware d'un nœud pas à jour après un `git pull`. Chaque heartbeat porte un **hash du fichier `protocol.yaml`** ; une divergence est un **FAIL** immédiat et explicite.

### [P4] Capteurs

| Test | PASS | WARN | FAIL |
|---|---|---|---|
| IMU : fréquence de `/imu/data` | 95–105 Hz | 80–95 Hz | < 80 Hz ou absent |
| IMU : norme du quaternion | 1,00 ± 0,01 | — | hors tolérance |
| IMU : norme de l'accélération au repos | 9,81 ± 0,3 m/s² | ± 0,8 | hors tolérance |
| IMU : biais gyro au repos | < 0,02 rad/s | < 0,05 | > 0,05 |
| Lidar : fréquence de `/scan` | 6–12 Hz | — | absent |
| Lidar : taux de points valides | > 60 % | 30–60 % | < 30 % |
| GPS : port présent | oui | — | non |
| GPS : `status` de `/gps/fix` | FIX | NO_FIX < 60 s | NO_FIX > 60 s |
| GPS : satellites | ≥ 8 | 5–7 | < 5 |
| GPS : covariance non nulle | oui | — | **non** ⚠️ |
| Kinect : présent sur USB 3.0 | oui | non | — (jamais FAIL) |

⚠️ **Le biais gyro au repos** est un test simple qui détecte une IMU mal montée (vibrations résiduelles), un capteur défaillant, ou un robot qui n'est pas réellement à l'arrêt. Il conditionne directement la qualité de l'EKF.

⚠️ **Covariance GPS nulle = FAIL.** Un `NavSatFix` avec `position_covariance` à zéro fait diverger l'EKF global. Si le driver ne la renseigne pas, il faut la calculer depuis le HDOP dans un nœud intermédiaire.

### [P5] Actionneurs ⚠️ mouvement

**Préconditions strictes** — le test est **sauté** si l'une n'est pas satisfaite :
- `selftest.allow_motion: true` dans les paramètres **ET**
- interrupteur physique « BANC » activé **ET**
- P1–P4 tous en PASS ou WARN **ET**
- confirmation opérateur si `require_operator_confirmation`

| Test | Méthode | PASS | FAIL |
|---|---|---|---|
| Présence des 4 variateurs | Retour `MOT_STATUS` | 4/4 | < 4 |
| Cohérence des capteurs Hall | Rotation manuelle de chaque roue, comptage | séquence valide | erreur de séquence |
| Sens de rotation | Consigne +0,2 rad/s 1 s, vérifier le signe du comptage | positif | négatif ⚠️ **fils inversés** |
| Réactivité | Rampe 0 → 0,3 rad/s, mesurer le retard | < 200 ms | > 500 ms |
| Freinage | Consigne 0,3 → assertion `EL`, mesurer le temps d'arrêt | < 500 ms | > 1,5 s |
| Courant à vide par roue | ACS758, une roue à la fois à 0,3 rad/s | 0,5–2,5 A 📐 | > 4 A |
| Symétrie | Écart de courant entre les 4 roues à consigne égale | < 30 % | > 50 % |
| Température après test | NTC | < 45 °C | > 60 °C |

⚠️ **Le test de sens de rotation est celui qui évite le plus d'accidents.** Une paire de fils de phase inversée, ou un `DIR` mal câblé, et le robot part dans la mauvaise direction au premier `cmd_vel`. Ce test le détecte en 4 secondes, roues levées.

⚠️ **Le test de symétrie de courant** détecte un roulement grippé, un frein qui frotte, ou un moteur en début de défaillance — avant que ça ne devienne une panne.

### [P6] Sécurité

| Test | Méthode | PASS | FAIL |
|---|---|---|---|
| Champignon lu | État GPIO | relâché | actif ou indéterminé |
| **Test actif du champignon** | Demander à l'opérateur d'appuyer, vérifier `/SAFE` et l'ouverture du contacteur, puis relâcher | transition observée | **pas de transition** ⚠️ |
| Ligne `/SAFE` : assertion | L'ESP32 asserte, les MOTION doivent le voir | vu par les 2 | non vu |
| Watchdog externe | Vérifier le compteur de reset | 0 depuis le boot | > 0 |
| Contacteur : cohérence | V_bus vs état commandé | cohérent | incohérent ⚠️ **contacteur collé** |
| Retour du relais de précharge | Chronométrer la montée de V_bus | conforme | anormal |
| Timeouts firmware | Vérifier les valeurs déclarées par chaque nœud | conformes | divergentes |

⚠️ **Le test actif du champignon est le seul qui prouve que la chaîne de sécurité fonctionne réellement.** Un champignon dont le contact est oxydé se lit « relâché » en permanence — y compris quand il est enfoncé. Ce test est **obligatoire une fois par session**, et il demande une action physique de l'opérateur. C'est deux secondes, et c'est ce qui distingue une sécurité réelle d'une sécurité supposée.

⚠️ **La détection de contacteur collé** : après le test du champignon, V_bus doit chuter. Si elle reste haute, le contacteur est soudé — état extrêmement dangereux, **CRITICAL**, refus d'armement définitif.

### [P7] Logiciel

| Test | PASS | FAIL |
|---|---|---|
| Tous les nœuds attendus vivants | oui | ≥ 1 manquant |
| Tous les lifecycle nodes en `active` | oui | ≥ 1 bloqué |
| Arbre TF complet `map → odom → base_link → *` | oui | arête manquante |
| Fraîcheur des TF | < 200 ms | > 500 ms |
| Paramètres chargés (nombre attendu) | oui | divergent |
| Version des paquets = version du firmware | identique | divergente |

## K.3 Règles d'agrégation et conditions de passage en READY

| Verdict global | Condition |
|---|---|
| **READY** | Tous les tests en PASS, ou seulement des WARN sur des composants non critiques (Kinect, GPS en intérieur, réseau) |
| **READY_DEGRADED** | ≥ 1 WARN sur un composant C2/C3, aucun FAIL. **L'armement est possible**, avec des limites réduites (§M.4) |
| **FAULT** | ≥ 1 FAIL sur un composant C1 ou C2, ou ≥ 3 FAIL au total. **Armement refusé.** |
| **CRITICAL** | ≥ 1 CRITICAL. **Armement refusé, mise hors puissance immédiate**, alerte sonore et visuelle |

**Règles supplémentaires, non négociables** :

1. **Aucun FAIL sur [P6] ne permet le passage en READY**, quel que soit le reste. La sécurité n'est pas dégradable.
2. **Le test actif du champignon doit avoir été passé dans la session courante.** Un redémarrage de ROS 2 sans coupure de puissance ne le réinitialise pas ; une coupure du contacteur, si.
3. **Une divergence de version de protocole est un FAIL bloquant.** Pas de « ça marchera sûrement ».
4. Le rapport complet est publié sur `/retriever/selftest_report` et écrit dans `~/retriever_logs/selftest_<timestamp>.json`.

## K.4 Le mode BANC

Un interrupteur physique à clé, en façade, à deux positions, câblé sur un GPIO du X1 **et** un GPIO de l'ESP32-SAFETY (les deux le lisent — cohérence vérifiée) :

| Position | Effet |
|---|---|
| **BANC** (roues levées) | `selftest.allow_motion = true` · vitesse plafonnée à **0,3 rad/s** par le **firmware** (pas par ROS) · Nav2 refuse de démarrer · voyant orange clignotant |
| **TERRAIN** | Auto-test moteur sauté (jamais de rotation automatique roues au sol) · vitesses nominales · Nav2 autorisé |

⚠️ Le bridage en mode BANC est appliqué **dans le firmware ESP32**, pas dans ROS 2. C'est le principe : une limite de sécurité ne doit pas dépendre d'un paramètre qu'un fichier YAML peut changer.

⚠️ Une **incohérence entre les deux lectures** de l'interrupteur (X1 dit BANC, ESP32 dit TERRAIN) est un `FAIL` de [P6] : c'est le signe d'un fil coupé, et on ne devine pas.

## K.5 Journalisation

| Quoi | Où | Rétention |
|---|---|---|
| Rapport d'auto-test | `~/retriever_logs/selftest_<ts>.json` | 100 derniers |
| Logs ROS 2 | `~/.ros/log/` | rotation systemd, 500 Mo |
| Bag d'événements (permanent) | `~/retriever_logs/bags/events/` — `/retriever/safety_state`, `/diagnostics_agg`, `/battery_state`, `/retriever/mission_state`, `/cmd_vel` | 30 jours 📐 |
| Bag complet (à la demande) | `~/retriever_logs/bags/full/` — tout sauf caméra | 5 dernières sessions |
| **Buffer circulaire pré-défaut** | 60 s en RAM, **vidé sur disque à chaque transition vers FAULT ou ESTOP** | 20 derniers incidents |
| Historique batterie | SQLite `~/retriever_logs/battery.db` — SOC, cellules, cycles, Δ | permanent |
| Compteurs d'erreurs CAN | dans les diagnostics + base | permanent |

⚠️ **Le buffer circulaire pré-défaut est la fonctionnalité de diagnostic la plus rentable de tout le système.** Quand le robot s'arrête inopinément, la question est toujours « qu'est-ce qui s'est passé dans les 10 secondes précédentes ? ». Sans ce buffer, il faut reproduire le défaut. Avec, on l'a déjà.

---

*Suite : `04-securite-watchdogs-capteurs.md` — sections L, M, N.*
