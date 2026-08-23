# W. Interface opérateur — pupitre Nintendo Switch

*Ajout au dossier v1.0 — 10 août 2026. Cette section complète le §O (réseau) et le §L (sécurité) ; elle ne les remplace pas.*

## W.0 Le principe : la Switch ne fait tourner aucun ROS

Ton intuition est la bonne, et elle rejoint exactement l'architecture du §O.1. Je la formalise :

> **Le pupitre est un client léger. Il affiche et il envoie des gestes. Il ne participe pas au graphe ROS 2.**

Trois raisons, dans l'ordre d'importance :

1. **Sécurité.** Un participant DDS de plus sur le Wi-Fi, c'est du trafic de découverte, des retransmissions et une latence non bornée sur le lien radio. §O.1 explique pourquoi le graphe ROS 2 reste confiné à `localhost` sur le X1 : ajouter la Switch au graphe casserait ce choix.
2. **Énergie.** Tu as raison : 16 Wh de batterie, un écran 720p et une pile ROS 2 complète, c'est incompatible avec une session de terrain. Un onglet navigateur consomme une fraction de ce que consomme un `rclcpp` + DDS + RViz.
3. **Faisabilité.** Le noyau de switchroot est un **Linux 4.9.140** (branche L4T r32) ✅ — vérifié dans les sources du noyau. ROS 2 Jazzy cible Ubuntu 24.04 avec un noyau 6.8. Ça se contournerait en conteneur, mais pour quel bénéfice ?

**Conséquence architecturale forte** : le pupitre est **remplaçable et jetable**. N'importe quel appareil avec un navigateur et une manette (PC portable, tablette + manette Bluetooth, Steam Deck) affiche la même interface. La Switch est le pupitre *préféré*, pas le pupitre *requis*. C'est ce qui rend acceptable de bâtir sur une plateforme aussi bricolée.

```
┌─────────────────────────────── SWITCH (client léger) ──────────────────────────┐
│  Joy-Con railés ──► joycon-serdev ──► joycond ──► uinput ──► evdev             │
│                                                                 │              │
│  Firefox ◄──────────────────────────────── Gamepad API ─────────┘              │
│     │                                                                          │
│     │  PWA « Retriever Console » (HTML + JS, ~200 ko, cache service worker)    │
│     │  Canvas 2D lidar · Leaflet carte hors-ligne · <img> MJPEG                │
└─────┼──────────────────────────────────────────────────────────────────────────┘
      │  Wi-Fi
      │  ├── WSS  :443/ws     → rosbridge (télémétrie, commandes, actions Nav2)
      │  ├── HTTPS :443/video → web_video_server (MJPEG)
      │  └── HTTPS :443/      → PWA + tuiles de carte (nginx, statique)
      ▼
┌─────────────────────────────── YOUYEETOO X1 ───────────────────────────────────┐
│  nginx (TLS, auth, reverse-proxy, HTTP/2)                                      │
│    ├─► 127.0.0.1:9090  rosbridge_server        (jamais exposé directement)     │
│    ├─► 127.0.0.1:8080  web_video_server                                        │
│    └─► /var/www/retriever  PWA + tuiles PMTiles                                │
│                                                                                │
│  retriever_teleop_guard  ← LE nœud de sécurité de la téléop (W.5)              │
│  twist_mux · velocity_smoother · Nav2 · … (graphe ROS 2 confiné à localhost)   │
└────────────────────────────────────────────────────────────────────────────────┘
```

---

## W.1 Ce que la Switch peut réellement faire — état vérifié

### La plateforme

| Élément | Fait | Confiance |
|---|---|---|
| Distributions switchroot | Ubuntu **Noble 24.04** (Kubuntu ou Unity), Jammy 22.04, Bionic 18.04, Fedora 42. **Pas de Focal.** | ✅ wiki officiel |
| Projet vivant en 2026 | Image Noble `2026-05-13` déposée le `2026-07-30` sur `download.switchroot.org` | ✅ listing serveur |
| **Noyau** | **Linux 4.9.140** (L4T r32.7.x). ⚠️ Les « 5.1.2 / 6.0.0 » du changelog sont des **versions d'image**, pas de noyau — piège classique | ✅ vérifié dans les sources |
| SoC | Tegra X1 T210 (2017) ou T210B01 « Mariko » (2019/Lite/OLED), 4× Cortex-A57, GPU Maxwell 256 cœurs, **4 Go LPDDR4** | ✅ |
| Écran portable | **720p @ 60 Hz** | ✅ |
| Batterie | **16,0 Wh** (HAC-003) · Lite : 13,2 Wh | ✅ iFixit |
| Autonomie sous switchroot | 🔴 **NON PUBLIÉE** — aucun chiffre sourcé n'existe | — |
| Modchip | Switch 2019 / Lite / OLED **exigent un modchip** ; seule la 2017 « Erista » se lance par RCM | ✅ |
| Switch 2 | ❌ **Non supportée, aucune perspective** | 🟡 consensus |

### ⚠️ Piège n°1 — Chromium est figé et Firefox est obligatoire

Deux faits indépendants qui pointent dans la même direction :

- **Le Chromium de switchroot est bloqué à la version 126** (mi-2024) et son dépôt de build est marqué *DEPRECATED* ✅. Pour un pupitre destiné à durer, c'est un navigateur qui ne recevra **plus jamais de correctif de sécurité**.
- **Chromium ne verra probablement pas tes Joy-Con.** Son `RefreshJoydevDevice()` écarte du chemin joydev tout périphérique dont le VID:PID figure dans `IsNintendoController()` — ce qui inclut `057e:2006` et `057e:2007`, exactement les PID des Joy-Con railés ✅ (vérifié dans les sources Chromium). Il les délègue au `NintendoDataFetcher`, qui énumère via **hidraw**… or le pilote `joycon-serdev` **ne crée aucun nœud hidraw**. Résultat attendu : invisibles.

→ **Firefox, depuis le PPA `mozillateam`** (maintenu, avec accélération vidéo Tegra). Bonus : son backend evdev applique le mapping « standard » dès qu'il voit `BTN_SOUTH`, ce que le périphérique combiné de joycond expose — là où Chromium le classerait `kUnknownGamepad` avec un mapping vide.

### ⚠️ Piège n°2 — Firefox exige un contexte sécurisé pour l'API Gamepad

**Depuis Firefox 81, `navigator.getGamepads()` renvoie un tableau vide en contexte non sécurisé** ✅. Une IHM servie en `http://192.168.4.1/` ne verra **aucune manette**.

Deux solutions, et je recommande la première :

| Solution | Comment | Compromis |
|---|---|---|
| ✅ **HTTPS depuis le robot** | nginx + certificat auto-signé de longue durée (10 ans), importé **une fois** dans le magasin de confiance de Firefox sur la Switch | Une manipulation à l'installation, puis on n'y pense plus. Une seule source de vérité pour l'IHM. |
| 🟡 IHM hébergée sur la Switch | Un serveur statique local ; `http://localhost` **est** un contexte sécurisé par définition | Le code de l'IHM est dupliqué sur chaque pupitre, à resynchroniser à chaque mise à jour |

Le certificat auto-signé sert aussi au **WSS** : rosbridge n'a **aucune authentification** (voir W.4), le chiffrement et l'authentification sont entièrement à la charge de nginx.

### ⚠️ Piège n°3 — les sticks pilotent le curseur de la souris

switchroot livre par défaut `/usr/share/X11/xorg.conf.d/50-joystick.conf` (xf86-input-joystick) : **bouger un stick déplace le pointeur** et des boutons émettent des touches ✅. Pour un pupitre, c'est ingérable. → Vider ou supprimer ce fichier à l'installation.

### ⚠️ Piège n°4 — les Joy-Con railés sont **deux** périphériques

En mode portable, ils ne passent pas par `hid-nintendo` (le driver Bluetooth/USB mainline) mais par un pilote **serdev/UART** propre à switchroot, `drivers/input/joystick/joycon-serdev.c` ✅ :

| Périphérique | VID:PID | Axes | Boutons |
|---|---|---|---|
| `Nintendo Switch Left Joy-Con Serial` | `057e:2006` | `ABS_X`, `ABS_Y` | `SELECT, Z, THUMBL, DPAD_*, TL, TL2` |
| `Nintendo Switch Right Joy-Con Serial` | `057e:2007` | `ABS_RX`, `ABS_RY` | `START, MODE, THUMBR, SOUTH/EAST/NORTH/WEST, TR, TR2` |

**`joycond` est indispensable** pour les fusionner en un `Nintendo Switch Combined Joy-Cons` (`057e:2008`, bus virtuel, uinput) qui expose les 4 axes et le jeu de boutons complet. Le fork switchroot (`CTCaer/joycond`) détecte le cas rail et accepte l'appairage en permanence.

⚠️ **Mais la fusion n'est déclenchée que par un événement evdev** : il faut **appuyer sur L+R (ou ZL+ZR) après chaque démarrage** pour que la manette combinée apparaisse. Ce n'est pas un bug, c'est le fonctionnement de joycond — et ça doit figurer dans la procédure de mise en route, et être diagnostiqué par l'IHM (W.6).

⚠️ **Pas de gyroscope sur les Joy-Con railés.** Le périphérique IMU n'est instancié que pour le pad intégré de la Switch **Lite** ✅ (vérifié dans le code — le wiki affirme le contraire, mais son affirmation vaut pour le chemin Bluetooth). L'IMU de la console elle-même reste accessible en **IIO** (`/sys/bus/iio/devices/`), pas en evdev. Sans conséquence : je ne recommande pas de piloter un robot de 35 kg à l'inclinaison du pupitre.

### ⚠️ Piège n°5 — la dérive des sticks, et c'est le vrai danger

Le noyau applique une zone morte de `flat = 1500` sur une plage de `±32767`, soit **4,6 %** ✅. C'est très insuffisant pour un Joy-Con usagé.

**Un stick qui dérive, c'est une consigne de vitesse non nulle au repos.** C'est le risque n°1 de tout ce chapitre, et il est aggravé par le fait que la dérive des Joy-Con est un défaut d'usure documenté à l'échelle du produit (recours collectifs, excuses publiques de Nintendo) 🟡.

Trois parades cumulatives, détaillées en W.3 et W.5 : **zone morte applicative de 15 %**, **homme-mort obligatoire**, et **test de repos au moment de l'armement**.

---

## W.2 Choix de la pile logicielle

### Le pont : `rosbridge_server`, pas `foxglove_bridge`

| Critère | `foxglove_bridge` 3.4.1 ✅ | `rosbridge_server` 2.7.0 ✅ |
|---|---|---|
| Présent en apt Jazzy (arm64 + amd64) | ✅ | ✅ |
| Langage / coût CPU | C++, relais CDR **sans sérialisation** — le moins cher | Python, CDR→objets→JSON — plus cher |
| Publication depuis le client | ✅ `clientPublish` + `client_topic_whitelist` | ✅ `advertise`/`publish` + `topics_pub_glob` (nouveau en 2.7.0) |
| **Actions ROS 2** | ❌ services seulement | ✅ **`send_action_goal` / `cancel_action_goal` / `action_feedback` / `action_result`** |
| Client JS | Il faut désérialiser le CDR soi-même | ✅ **`roslib` 2.1.0** (2026-03, réécrit en TypeScript/ESM, maintenu) |
| Compression | Aucune dans le protocole | `cbor`, `cbor-raw` (= volume CDR), `throttle_rate` par abonnement |
| TLS natif | ✅ | ✅ (mais on passe par nginx de toute façon) |
| Authentification | ❌ | ❌ (`rosauth` n'existe pas pour Jazzy ✅) |
| Maintenance amont | Actif | ✅ **Très actif** : 2.7.0 le 2026-05-27, backports systématiques |

**Le facteur décisif, ce sont les actions.** Tu veux des missions par waypoints GPS : ça veut dire appeler `nav2_msgs/action/NavigateToPose` et `FollowGPSWaypoints`, suivre leur *feedback* et pouvoir les annuler. Seul rosbridge parle nativement les actions ROS 2. Avec foxglove_bridge, il faudrait écrire un nœud passerelle action↔service — du travail pour rien.

Le surcoût CPU de rosbridge est réel mais mesurable et maîtrisable : voir W.4.

### Le client : une PWA maison, pas Foxglove Studio

**Foxglove Studio n'est plus utilisable dans ce contexte** ✅ :
- Le viewer **n'est plus open source** (dernière release ouverte : v1.x, février 2024 ; dépôt archivé).
- **Pas de version web auto-hébergeable**, et une connexion au cloud est requise au moins une fois. L'usage totalement hors-ligne relève d'une **licence commerciale sur devis**.

Le *pont* Foxglove reste sous licence MIT et excellent — c'est le client qui pose problème.

Deux alternatives évaluées :

| Option | Verdict |
|---|---|
| **Lichtblick** (fork MPL-2.0 du dernier Studio ouvert, v1.26.0 du 2026-06-17, tourne en Docker dans le navigateur, sans login) | ✅ **À garder pour le diagnostic à l'atelier.** C'est un excellent outil de debug. Mais c'est un outil généraliste lourd, pas une console de conduite : rien n'y est pensé pour une manette et un écran 720p tenu à bout de bras. |
| **PWA maison** | ✅ **Retenu pour le pupitre.** ~200 ko de HTML/JS, exactement les fonctions voulues, mapping Joy-Con sur mesure, logique d'homme-mort maîtrisée. |

### Les briques, toutes vérifiées présentes en apt Jazzy arm64 ✅

| Paquet | Version | Rôle |
|---|---|---|
| `ros-jazzy-rosbridge-server` | 2.7.0 | Pont WebSocket |
| `ros-jazzy-rosapi` | 2.7.0 | Introspection |
| `ros-jazzy-web-video-server` | 3.1.0 | MJPEG / VP8 / H264 |
| `ros-jazzy-compressed-image-transport` | 4.0.7 | JPEG côté ROS |
| `ros-jazzy-twist-mux` | 4.5.0 | Priorités + **timeouts** |
| `ros-jazzy-nav2-velocity-smoother` | 1.3.12 | Rampe d'arrêt |
| `ros-jazzy-tf2-web-republisher` | 1.0.0 | TF pour clients web |
| `nginx` (Ubuntu noble) | 1.24.0 | TLS, auth, reverse-proxy, HTTP/2 |

Côté navigateur : `roslib` **2.1.0**, `leaflet` **1.9.4**, `pmtiles` **4.4.1**. Le rendu lidar et carte d'occupation est **fait maison en Canvas 2D** — `ros2djs` et `ros3djs` sont morts (dernière publication **2022-05-03**, et `ros3djs` épingle `three@0.89` alors que Three.js en est à 0.185) ✅.

---

## W.3 Cartographie des Joy-Con

Le périphérique combiné de joycond expose `BTN_SOUTH`, donc **Firefox annonce `mapping: "standard"`** ✅ et les index sont ceux de la spécification W3C.

```
        ┌─ Joy-Con GAUCHE ─┐                  ┌─ Joy-Con DROIT ─┐
        │                  │                  │                 │
   ZL ══╡ HOMME-MORT       │                  │      VITESSE ++ ╞══ ZR
    L ──┤ vitesse max −    │                  │  vitesse max +  ├── R
        │                  │                  │                 │
        │   ╭───╮          │                  │        (X)      │
        │   │ ↑ │ stick G  │                  │    (Y)   (A)    │
        │   ╰───╯ AVANT/   │                  │        (B)      │
        │        ARRIÈRE   │                  │                 │
        │                  │                  │   ╭───╮         │
        │  ▲               │                  │   │↔ │ stick D  │
        │ ◄ ►  croix       │                  │   ╰───╯ ROTATION│
        │  ▼               │                  │                 │
        │      (−)         │                  │      (+)        │
        └──────────────────┘                  └─────────────────┘
```

| Contrôle | Index W3C | Fonction | Sécurité |
|---|---|---|---|
| **ZL** | bouton 6 | **HOMME-MORT — maintenu** | ⚠️ Aucun `Twist` non nul n'est émis sans lui |
| **Stick gauche, axe Y** | axe 1 | `linear.x` | Zone morte 15 %, courbe quadratique |
| **Stick droit, axe X** | axe 2 | `angular.z` | idem |
| ZR | bouton 7 | Vitesse haute *tant que ZL est tenu* | Plafond relevé, jamais au-delà de la limite firmware |
| L / R | boutons 4 / 5 | Plafond de vitesse − / + (5 crans) | Persistant |
| **B** | bouton 0 | **ARRÊT** — arrêt opérationnel + désarmement | ⚠️ **Ce n'est PAS un arrêt d'urgence** (W.5.6) |
| A (appui long 1 s) | bouton 1 | Demande d'armement | Appui long volontaire |
| X | bouton 3 | Cycle d'affichage : CONDUITE → CAPTEURS → CARTE → DIAG | |
| Y | bouton 2 | Poser un waypoint / marquer un point d'intérêt | |
| Croix | boutons 12–15 | Navigation dans l'IHM, sélection de waypoint | |
| − | bouton 8 | Désarmement | |
| + | bouton 9 | Menu / paramètres | |
| Clic stick G / D | boutons 10 / 11 | Recentrer la vue / basculer la caméra | |

**Schéma « twin-stick » et non « single-stick »** : l'avance est sur un stick, la rotation sur l'autre. Sur un skid-steer, un seul stick pour les deux couple avance et rotation à chaque micro-mouvement du pouce, et une dérive diagonale devient une trajectoire en arc. Deux sticks séparent les fautes.

### Traitement du signal des sticks

```js
const DEADZONE = 0.15;        // 15 %, très au-delà des 4,6 % du noyau
const EXPO     = 2.0;         // courbe quadratique : finesse au centre

function shapeAxis(v) {
  const a = Math.abs(v);
  if (a < DEADZONE) return 0;                       // zone morte franche
  const n = (a - DEADZONE) / (1 - DEADZONE);        // renormalisation
  return Math.sign(v) * Math.pow(n, EXPO);          // expo
}
```

⚠️ **La renormalisation après zone morte n'est pas cosmétique** : sans elle, la commande saute brutalement de 0 à 15 % dès qu'on sort de la zone morte. Avec, la sortie est continue.

⚠️ **Test de repos à l'armement** (parade n°3 contre la dérive) : au moment où l'opérateur demande l'armement, l'IHM vérifie que **les quatre axes lisent moins de 5 % pendant 1 seconde continue**. Sinon : refus, et message explicite *« Stick gauche à 11 % au repos — dérive détectée, armement refusé »*. Cela transforme une panne silencieuse et dangereuse en un message clair.

---

## W.4 Protocole et budget de bande passante

### Topics exposés au client

| Sens | Topic | Type | Cadence | Encodage |
|---|---|---|---|---|
| ↑ client → robot | `/web/cmd_vel` | `geometry_msgs/Twist` | **20 Hz fixe** | JSON (22 kbit/s, négligeable) |
| ↑ | `/web/heartbeat` | `retriever_msgs/WebHeartbeat` (seq + timestamp client) | **10 Hz** | JSON |
| ↑ | `/web/estop_request` | `std_msgs/Bool` | événementiel | JSON |
| ↓ robot → client | `/web/link_status` | `retriever_msgs/LinkStatus` (dernier seq reçu, état) | 5 Hz | JSON |
| ↓ | `/retriever/safety_state` | custom | 10 Hz *(décimé depuis 100 Hz)* | JSON |
| ↓ | `/retriever/telemetry` | `std_msgs/Float32MultiArray` | 10 Hz | JSON |
| ↓ | `/battery_state` | `sensor_msgs/BatteryState` | 1 Hz | JSON |
| ↓ | `/diagnostics_agg` | `diagnostic_msgs/…` | 1 Hz | JSON |
| ↓ | `/scan` | `sensor_msgs/LaserScan` | 7 Hz | **`cbor-raw`** |
| ↓ | `/map` | `nav_msgs/OccupancyGrid` | à la demande | `cbor` |
| ↓ | `/gps/fix` | `sensor_msgs/NavSatFix` | 1 Hz | JSON |
| ↓ | `/odometry/filtered/global` | `nav_msgs/Odometry` | 5 Hz *(décimé)* | JSON |
| ↔ actions | `NavigateToPose`, `FollowGPSWaypoints` | Nav2 | événementiel | JSON |
| ↓ vidéo | hors WebSocket | MJPEG via `web_video_server` | 10 fps | HTTP |

### Budget, chiffré

Chiffres calculés, pas estimés (sérialisation réelle) :

| Flux | Débit |
|---|---|
| Lidar X4 (714 pts/scan à 7 Hz), CDR / `cbor-raw` | **163 kbit/s** |
| *— le même en JSON brut* | *788 kbit/s (**×4,8**)* |
| *— en JSON arrondi à 3 décimales* | *294 kbit/s* |
| MJPEG 640×480 @ 10 fps, **q = 70** | **1 682 kbit/s** |
| *— le même à q = 95 (défaut de `web_video_server` ⚠️)* | *~4 000 kbit/s* |
| Télémétrie en `Float32MultiArray` @ 10 Hz | **72 kbit/s** |
| *— la même en JSON à champs nommés* | *172 kbit/s (**×2,4**)* |
| `cmd_vel` @ 20 Hz | 22 kbit/s |
| **Total configuration frugale** | **≈ 1,9 Mbit/s** |
| Total configuration naïve (JSON partout, q = 95, 15 fps) | ≈ 7,8 Mbit/s |

Trois enseignements concrets :

1. ⚠️ **`web_video_server` a `quality=95` par défaut** — descendre à 70 divise le débit par 2,4 pour une différence visuelle négligeable sur un écran 720p.
2. ⚠️ **Ne publier `intensities` que si le capteur en produit.** Le YDLIDAR X4 n'en produit pas ; certains drivers remplissent quand même le tableau de zéros, ce qui **double** le débit pour rien.
3. **Une image brute ne doit jamais transiter.** Un `sensor_msgs/Image` 640×480 `rgb8` à 10 fps, c'est **73,7 Mbit/s**. Toujours `default_transport=compressed`.

Un Wi-Fi 802.11n 2,4 GHz réel donne 15–30 Mbit/s en conditions moyennes mais **s'effondre en bord de couverture**. La configuration frugale garde une marge ×8. Prévoir malgré tout une **dégradation adaptative** : au-delà d'un RTT seuil, l'IHM réduit d'abord le débit d'images, puis coupe la vidéo, puis coupe le lidar — la télémétrie et l'homme-mort passent en dernier.

### Sécurisation du transport

⚠️ **rosbridge sur ROS 2 Jazzy n'a aucune authentification** ✅ (le paquet `rosauth` n'existe pas pour Jazzy, l'opcode `auth` a disparu du protocole v2). **Quiconque atteint le port 9090 pilote le robot.**

```
rosbridge_server        → écoute sur 127.0.0.1:9090 UNIQUEMENT
web_video_server        → écoute sur 127.0.0.1:8080 UNIQUEMENT
nginx :443 (TLS, HTTP/2)
   ├── /             → /var/www/retriever            (PWA + tuiles PMTiles)
   ├── /ws           → proxy_pass 127.0.0.1:9090 (WebSocket upgrade)
   └── /video/       → proxy_pass 127.0.0.1:8080
   auth_basic + client cert optionnel ; rate limiting
```

Garde-fous à activer côté rosbridge, tous vérifiés existants :

```yaml
rosbridge_websocket:
  ros__parameters:
    port: 9090
    address: "127.0.0.1"
    topics_pub_glob: ["/web/cmd_vel", "/web/heartbeat", "/web/estop_request"]  # ← 2.7.0
    topics_sub_glob: ["/retriever/*", "/scan", "/map", "/gps/fix", "/battery_state",
                      "/diagnostics_agg", "/odometry/filtered/global", "/web/link_status"]
    services_glob:  ["/retriever/arm", "/retriever/disarm", "/retriever/clear_fault"]
    actions_glob:   ["/navigate_to_pose", "/follow_gps_waypoints"]
    websocket_ping_interval: 2.0     # ⚠️ défaut 0.0 = DÉSACTIVÉ
    websocket_ping_timeout: 6.0
    use_events_executor: true        # réduit la conso CPU au repos (2.5.0+)
    max_message_size: 1000000
```

⚠️ **`topics_pub_glob` est la ligne la plus importante de ce fichier.** Sans elle, un client web peut publier sur `/diff_drive_controller/cmd_vel` et court-circuiter `twist_mux`, le collision monitor et toute la chaîne de filtres du §L.2. Avec elle, le seul chemin de commande est celui qui passe par le superviseur.

⚠️ Le `websocket_ping_interval` est à **0.0 par défaut**, c'est-à-dire désactivé. Mais attention : le ping WebSocket est traité par la pile Tornado et **ne prouve pas que la boucle ROS tourne**. Il ne remplace pas le heartbeat applicatif de W.5.

### Réseau de terrain

Le §O prévoit WireGuard. ⚠️ **Sur le noyau 4.9 de switchroot, le module WireGuard n'est pas intégré** (il l'est depuis Linux 5.6) — il faudrait un module DKMS ou une implémentation en espace utilisateur. 🔴 **À vérifier avant de s'engager.**

Pour l'usage de terrain, ce n'est pas bloquant : le VPN du §O sert à traverser Internet, ce qui n'est pas le cas ici. Sur le terrain, la configuration est plus simple et tout aussi sûre :

| Mode | Configuration |
|---|---|
| **TERRAIN** | Le robot est **point d'accès Wi-Fi** (WPA2/WPA3, réseau isolé, pas de route vers Internet). La Switch s'y connecte. nginx en TLS + auth Basic. Pas de VPN. |
| **ATELIER** | Les deux sur le réseau local ou en Ethernet. Idem. |
| **DISTANT** (par Internet) | WireGuard **obligatoire**, conformément au §O — et depuis un client qui le supporte (PC portable), pas depuis la Switch tant que le point ci-dessus n'est pas tranché. |

⚠️ Bug connu : sur switchroot **KDE Plasma**, la connexion échoue sur les réseaux en **mode transition WPA3/WPA2** ✅. Contournement : `nmcli dev wifi connect <SSID>`, ou forcer le point d'accès du robot en WPA2 seul.

---

## W.5 Sécurité de la téléopération — la partie qui compte

### W.5.1 Le principe

> **Le pupitre est la partie la moins fiable de tout le robot.** Chromium figé, noyau 4.9, sticks qui dérivent, batterie non caractérisée, lien radio. L'architecture doit donc garantir que **sa défaillance est sans conséquence** — et non espérer qu'il ne défaille pas.

C'est déjà vrai par construction : le pupitre n'est qu'une source de `cmd_vel` parmi d'autres, filtrée par les sept étages du §L.2 et arbitrée par `twist_mux`. La présente section ajoute la couche qui manquait : **la détection de perte de lien**.

### W.5.2 Les sept couches

```
┌─ ① HOMME-MORT (client) ────────────────────────────────────────────────┐
│  ZL maintenu. Aucun Twist non nul émis sans lui.                       │
│  ⚠️ Relâchement FORCÉ sur : visibilitychange, blur, gamepad            │
│     disconnect, page masquée, onglet en arrière-plan.                  │
├─ ② PUBLICATION CADENCÉE (client) ───────────────────────────────────────┤
│  20 Hz FIXE tant que ZL est tenu — jamais « seulement quand ça change ».│
│  Un message perdu ne doit pas figer la consigne.                        │
├─ ③ HEARTBEAT BIDIRECTIONNEL ────────────────────────────────────────────┤
│  ↑ /web/heartbeat 10 Hz : compteur de séquence + horodatage client      │
│  ↓ /web/link_status 5 Hz : dernier seq reçu + état du superviseur       │
│  Le client calcule le RTT, affiche la dégradation, et RELÂCHE           │
│  L'HOMME-MORT DE LUI-MÊME au-delà du seuil.                             │
├─ ④ ANTI-REJEU ──────────────────────────────────────────────────────────┤
│  Le superviseur rejette tout seq non strictement croissant.             │
│  ⚠️ Ce n'est PAS de la sécurité cryptographique — c'est WSS qui l'assure.│
├─ ⑤ TIMEOUT twist_mux ───────────────────────────────────────────────────┤
│  timeout: 0.3 s sur l'entrée web (≈6 périodes à 20 Hz).                 │
│  Au-delà : la voie web est ignorée, vitesse 0 substituée.               │
├─ ⑥ SUPERVISEUR retriever_teleop_guard (ROS 2) ──────────────────────────┤
│  Surveille l'âge du heartbeat ET du dernier Twist.                      │
│  Dépassement → verrou twist_mux priorité 255 → arrêt EN RAMPE via       │
│  velocity_smoother (pas un zéro brutal : ça fait déraper ou basculer).  │
│  ⚠️ INHIBE LE REDÉMARRAGE AUTOMATIQUE au retour du lien.                │
├─ ⑦ CHAÎNE MATÉRIELLE (§L) ──────────────────────────────────────────────┤
│  Champignon → /SAFE → contacteur. Indépendante de tout ce qui précède.  │
└─────────────────────────────────────────────────────────────────────────┘
```

Configuration `twist_mux` correspondante :

```yaml
twist_mux:
  ros__parameters:
    topics:
      web_teleop:  {topic: /web/cmd_vel, timeout: 0.3, priority: 100}
      navigation:  {topic: /cmd_vel_nav, timeout: 0.5, priority: 10}
    locks:
      teleop_guard: {topic: /web/link_lost, timeout: 0.0, priority: 254}
      estop:        {topic: /retriever/estop_active, timeout: 0.0, priority: 255}
```

### W.5.3 `retriever_teleop_guard` — nouveau nœud, à ajouter au §I

| | |
|---|---|
| **Responsabilité** | Surveiller la vivacité du pupitre. Verrouiller `twist_mux` en cas de perte. Interdire le redémarrage automatique. |
| **Publie** | `/web/link_lost` (`std_msgs/Bool`, verrou) · `/web/link_status` (5 Hz) · `/diagnostics` |
| **Souscrit** | `/web/heartbeat` · `/web/cmd_vel` · `/retriever/safety_state` |
| **Services** | `/web/rearm` (réarmement explicite après une perte) |
| **Paramètres** | `heartbeat_timeout: 0.5` · `cmd_timeout: 0.3` · `require_explicit_rearm: true` · `ramp_down_time: 0.8` |
| **Fréquence** | 50 Hz |
| **Criticité** | **C2** |

⚠️ **`require_explicit_rearm: true` n'est pas négociable.** C'est l'esprit de la clause **ISO 3691-4 §4.1.14 « Avoidance of automatic restart »** ✅ (clause vérifiée dans la table des matières publique). Concrètement : quand le Wi-Fi revient après une coupure, le robot **ne repart pas tout seul**. L'opérateur voit un bandeau « LIEN RÉTABLI — réarmement requis » et doit appuyer sur A pendant une seconde. Un robot qui redémarre seul dès que la connexion revient, alors que l'opérateur a peut-être posé le pupitre et s'est approché du robot, est un accident qui attend son heure.

### W.5.4 Budget de latence

```
t_total = t_détection_client + t_transit_radio + t_file_WS + t_ROS + t_freinage
```

| Étape | Valeur 📐 | Origine |
|---|---|---|
| Détection côté client (5 heartbeats manqués à 10 Hz) | 500 ms | choix de conception |
| Transit radio, Wi-Fi dégradé | 20–200 ms | mesuré au banc |
| Timeout `twist_mux` | 300 ms | paramètre |
| Boucle superviseur (50 Hz) | 20 ms | |
| Rampe d'arrêt `velocity_smoother` | 800 ms | paramètre |
| **Total, cas défavorable** | **≈ 1,8 s** | |

Rapporté à **ISO 13855:2024** ✅, dont la vitesse d'approche de référence est 1600 mm/s pour des temps d'arrêt > 500 ms, et qui donne le rapport direct **150 ms de temps d'arrêt supplémentaire = 300 mm de distance de sécurité en plus** :

> À 0,5 m/s, 1,8 s de détection et d'arrêt = **90 cm parcourus**.

📐 C'est acceptable en terrain dégagé et sans tiers. Ça ne l'est pas dans un couloir ou près de quelqu'un. **Deux conséquences directes** :

- La vitesse maximale en téléopération Wi-Fi est plafonnée à **0,8 m/s** 📐, pas 1,5 m/s. Le plafond nominal ne s'applique qu'en mission autonome, où le `collision_monitor` réagit localement en ~100 ms.
- Le `collision_monitor` (§I.2) reste actif **y compris en téléopération** — il s'interpose entre `twist_mux` et le contrôleur. L'opérateur ne peut pas conduire dans un obstacle même en tenant l'homme-mort.

### W.5.5 Ce qui manque, et qu'il faut acheter

⚠️ **Avec la téléopération, l'arrêt d'urgence radio passe de « recommandé » à quasi obligatoire.**

Le raisonnement est simple : le champignon du §L est **sur le robot**. En téléopération, l'opérateur est à 10 ou 30 m, une manette dans les mains. Si le robot part de travers, il ne peut pas atteindre le champignon. Le bouton B de l'IHM traverse le navigateur, le Wi-Fi, nginx, rosbridge et Python — ce n'est pas un arrêt d'urgence.

→ **Un e-stop radio à contact NF, inséré en série dans la boucle matérielle existante** (§D.3-④). Il ne dépend d'aucun logiciel, et son récepteur relâche la bobine du contacteur exactement comme le champignon. ~90 €, déjà listé au §S.3 ; je le remonte en priorité impérative dès que la téléopération est en service.

### W.5.6 Vocabulaire — un point qui n'est pas cosmétique

| Ce qu'on appelle | Ce que c'est vraiment | Étiquette dans l'IHM |
|---|---|---|
| Champignon sur le robot | **Arrêt d'urgence** (ISO 13850, catégorie 0/1, matériel) | — |
| E-stop radio | **Arrêt d'urgence** (matériel) | — |
| Bouton B / bouton rouge de l'IHM | **Arrêt opérationnel** (catégorie 2, logiciel) | **« ARRÊT »**, jamais « arrêt d'urgence » |
| Relâchement de l'homme-mort | Arrêt de la commande | — |

**La norme directement applicable ici est IEC 62745:2017** *« Safety of machinery — Requirements for cableless control systems »* 🟡 (texte payant, non lu). Ses principes rapportés : la perte de communication est traitée comme **un défaut dangereux** exigeant une transition automatique vers un état sûr, avec heartbeat, validation de séquence, CRC et watchdogs — c'est exactement l'architecture ci-dessus. Et : *« wireless does not reduce the safety requirements »*.

Si le robot devait un jour être certifié, c'est cette norme-là qu'il faut acheter (~200 €), pas ISO 10218.

---

## W.6 Maquette d'écran

Écran 720p (1280 × 720), tenu à bout de bras, en extérieur, parfois au soleil. → **gros caractères, fort contraste, très peu d'éléments, aucune information décorative.**

### Mode CONDUITE (par défaut)

```
┌────────────────────────────────────────────────────────────────────────────┐
│ ● ARMÉ         Wi-Fi ▊▊▊▊▁  RTT 34 ms      🔋 72 %  36,8 V  −4,2 A   14:23 │ ← bandeau d'état
├────────────────────────────────────────────────────────────────────────────┤
│                                                     ┌────────────────────┐ │
│                                                     │                    │ │
│                   FLUX CAMÉRA                       │   ╱─────────╲      │ │
│                   640 × 480 · 10 fps                │  │   LIDAR   │     │ │
│                                                     │  │     ▲     │     │ │
│                                                     │   ╲─────────╱      │ │
│                                                     │  portée 5 m        │ │
│                                                     └────────────────────┘ │
│                                                     ┌────────────────────┐ │
│                                                     │ VITESSE            │ │
│                                                     │   0,42 m/s         │ │
│                                                     │   ██████░░░░  plaf.│ │
│                                                     │   0,8 m/s (cran 3) │ │
│                                                     └────────────────────┘ │
├────────────────────────────────────────────────────────────────────────────┤
│  ╔══════════════════════════════════════════════════════════════════════╗  │
│  ║   ZL MAINTENU — CONDUITE ACTIVE                                      ║  │ ← bandeau homme-mort
│  ╚══════════════════════════════════════════════════════════════════════╝  │   vert plein si tenu
│  4 roues ✔   IMU ✔   GPS 9 sat ✔   Lidar ✔   T° 41 °C   ⚠ Kinect absent   │   gris barré sinon
└────────────────────────────────────────────────────────────────────────────┘
```

Le bandeau d'homme-mort occupe toute la largeur et change de couleur de façon massive : **vert plein** quand ZL est tenu, **gris rayé** sinon, **rouge clignotant** en cas de perte de lien. C'est l'élément que l'opérateur voit du coin de l'œil sans quitter la scène des yeux.

### États du bandeau supérieur

| État | Fond | Texte |
|---|---|---|
| `SAFE` / `READY` | gris | ● PRÊT |
| `ARMED` | bleu | ● ARMÉ |
| `RUNNING` | vert | ● MISSION EN COURS |
| `DEGRADED` | orange | ▲ DÉGRADÉ — *cause* |
| `FAULT` | rouge | ■ DÉFAUT — *cause* |
| `ESTOP` | rouge clignotant | ✖ ARRÊT D'URGENCE |
| Lien perdu | rouge rayé | ✖ LIEN PERDU — réarmement requis |

### Mode CARTE (missions)

```
┌────────────────────────────────────────────────────────────────────────────┐
│ ● ARMÉ    Wi-Fi ▊▊▊▁▁  RTT 78 ms       🔋 61 %   Mission : 2/5 waypoints   │
├──────────────────────────────────────────────────┬─────────────────────────┤
│                                                  │  WAYPOINTS              │
│      ╭──── carte Leaflet, tuiles hors ligne ───╮ │  ▸ 1 ✔ atteint          │
│      │                                          │ │  ▸ 2 ✔ atteint          │
│      │        ②────────③                        │ │  ▸ 3 ◉ en cours  12 m   │
│      │       ╱          ╲                       │ │  ▸ 4   en attente       │
│      │      ①            ④                      │ │  ▸ 5   en attente       │
│      │      │      🤖    │                      │ │                         │
│      │      │      ▲     │                      │ │  [Y] poser un point     │
│      │      ╰──── ⑤ ─────╯                      │ │  [A] démarrer           │
│      ╰──────────────────────────────────────────╯ │  [B] ARRÊT              │
│         ⊕ position GPS ±2,8 m · 9 satellites      │  [croix] sélectionner   │
├──────────────────────────────────────────────────┴─────────────────────────┤
│  ╔══════════════════════════════════════════════════════════════════════╗  │
│  ║   ZL RELÂCHÉ — conduite inhibée · mission autonome autorisée         ║  │
│  ╚══════════════════════════════════════════════════════════════════════╝  │
└────────────────────────────────────────────────────────────────────────────┘
```

Carte : **Leaflet 1.9.4** + tuiles raster hors connexion. Deux façons de les servir depuis le robot, et je recommande la première :

| Solution | Détail |
|---|---|
| ✅ **PMTiles + nginx** | Une archive `.pmtiles` unique posée sur nginx, lue côté navigateur par `pmtiles@4.4.1` en requêtes HTTP Range. **Aucun serveur de tuiles**, nginx gère les Range nativement. |
| 🟡 Tuiles PNG statiques | Arborescence `/{z}/{x}/{y}.png` servie en statique. Zéro dépendance JS, mais des dizaines de milliers de fichiers. |
| ❌ MBTiles + tileserver-gl | `tileserver-gl` **n'est pas dans les dépôts Ubuntu noble** ✅ — il faudrait ajouter Node.js et un service à surveiller sur le robot. |

📐 Compter 50–200 Mo pour une zone de 10 × 10 km jusqu'au niveau z18. Extraction avec `gdal2tiles.py` (`gdal-bin`, présent dans noble ✅).

⚠️ **Pas MapLibre GL ni OpenLayers** : ils exigent WebGL, ce qui pèse lourd sur un Tegra X1. Et le besoin réel — quelques dizaines de marqueurs et une polyligne — ne justifie pas un rendu vectoriel. Leaflet en Canvas suffit très largement.

### Mode DIAG

Rapport d'auto-test [P1]–[P7] du §K, tensions des 10 cellules avec leur delta, températures, compteurs d'erreurs CAN, journal des 20 derniers événements. **Et une section « pupitre »** : identifiant et mapping de la manette détectée, valeur brute des 4 axes au repos (détection de dérive), RTT, débit, version de l'IHM.

⚠️ **Cette section « pupitre » n'est pas du confort.** C'est ce qui permet de diagnostiquer en trente secondes le cas *« j'appuie et il ne se passe rien »* — manette non fusionnée (joycond), mapping vide (mauvais navigateur), contexte non sécurisé (HTTP au lieu de HTTPS), stick qui dérive.

### Rendu du lidar et de la carte d'occupation

Fait maison, en **Canvas 2D empilés** (fond statique / scan dynamique / surcouches). Ce n'est pas un pis-aller :

- `nav_msgs/OccupancyGrid` → un `Uint8ClampedArray` RGBA construit depuis `data[]` (−1 inconnu → gris, 0 libre → blanc, 100 occupé → noir), `putImageData` sur un canvas hors-écran, affiché par transformation affine depuis `info.origin` et `info.resolution`. **~40 lignes**, et on ne redessine qu'à réception.
- `sensor_msgs/LaserScan` → 714 points par scan à 7 Hz, soit ~5 000 opérations/s. Aucun problème même sur un Cortex-A57.

`ros2djs` et `ros3djs` sont morts (dernière publication 2022-05-03, `ros3djs` épingle `three@0.89` contre 0.185 aujourd'hui) ✅ — les ressusciter coûterait plus cher que d'écrire les 40 lignes.

---

## W.7 Démarrage du pupitre

### Configuration de la Switch

```bash
# 1. Firefox depuis le dépôt Mozilla (le Chromium switchroot est figé en 126)
sudo add-apt-repository ppa:mozillateam/ppa
sudo apt install firefox

# 2. ⚠️ Neutraliser le mapping stick → curseur souris
sudo mv /usr/share/X11/xorg.conf.d/50-joystick.conf{,.disabled}

# 3. joycond doit tourner
systemctl status joycond

# 4. Certificat du robot dans le magasin de confiance de Firefox
#    (Paramètres → Vie privée → Certificats → Autorités → Importer)

# 5. Économie d'énergie
#    - profil nvpmodel « Handheld » dans la barre des tâches
#    - option de boot dvfsb=1 (Switch 2019 / Lite / OLED uniquement)
xset s off && xset s noblank && xset -dpms
```

### Procédure de mise en route, dans l'ordre

```
1. Allumer la Switch, ouvrir Firefox sur https://retriever.local/
2. ⚠️ APPUYER SUR L + R  → joycond fusionne les deux Joy-Con
   (obligatoire à CHAQUE démarrage : la fusion n'est déclenchée
    que par un événement evdev)
3. ⚠️ Appuyer sur n'importe quel bouton avec la page VISIBLE
   → Firefox n'expose la manette qu'après une interaction
4. L'IHM affiche « Manette : Nintendo Switch Combined Joy-Cons
   — mapping standard ✔ »   … sinon, voir W.6 mode DIAG
5. Test de repos des sticks (automatique, 1 s)
6. [A] appui long → demande d'armement
7. Le robot répond : accordé, ou refusé avec la liste des
   conditions non satisfaites (§J.2)
```

⚠️ Les étapes 2 et 3 sont contre-intuitives et **doivent figurer sur une étiquette collée au dos de la Switch**. Sans elles, l'interface semble simplement cassée.

### Mode kiosque (optionnel, phase ultérieure)

`~/.xinitrc` avec `matchbox-window-manager`, `unclutter-xfixes` et Firefox en `--kiosk`. Paquets présents dans noble ✅. À ne faire qu'une fois l'IHM stabilisée : en développement, un bureau normal est plus pratique.

### Énergie

🔴 **Aucune mesure d'autonomie sous switchroot n'est publiée.** Je ne l'invente pas. Ce qui est documenté :

- Batterie **16,0 Wh** (13,2 Wh sur Lite) ✅
- **Deep Sleep** supporté ✅
- Profils `nvpmodel` commutables ; le profil « Handheld » bride CPU/GPU ✅
- Option de boot **`dvfsb=1`** sur Mariko : *« reduces power draw for the same performance »* ✅ — levier gratuit
- Charge en USB-C PD pendant l'usage ✅

📐 **Recommandation pragmatique** : mesurer l'autonomie réelle dès la première session (`/sys/class/power_supply/`), et prévoir dans tous les cas **une batterie externe USB-C PD**. Un pupitre qui s'éteint en pleine manœuvre est un mode de défaillance que l'architecture gère (perte de heartbeat → arrêt en rampe), mais qu'il vaut mieux ne pas provoquer.

---

## W.8 Modes de défaillance propres au pupitre

| # | Panne | Détection | Réaction | Niveau |
|---:|---|---|---|---|
| 35 | Wi-Fi perdu en téléopération | Heartbeat > 500 ms | Verrou `twist_mux`, arrêt en rampe 0,8 s, **réarmement explicite requis** | **DEGRADED** |
| 36 | **Onglet passé en arrière-plan / écran verrouillé** | `visibilitychange`, `blur` | ⚠️ Homme-mort relâché **côté client**, immédiatement. Le throttling des timers en arrière-plan rendrait la cadence de 20 Hz caduque | — |
| 37 | Manette débranchée / joycond redémarré | `gamepaddisconnected` | Homme-mort relâché, bandeau rouge | — |
| 38 | **Stick qui dérive** | Test de repos à l'armement (< 5 % pendant 1 s) | Armement refusé, valeur affichée | **refus** |
| 39 | Dérive apparue **en cours de session** | Valeur non nulle > 30 s alors que l'homme-mort est relâché | Avertissement persistant, plafond de vitesse réduit à 0,3 m/s | **WARN** |
| 40 | Batterie du pupitre vide | Extinction brutale | = panne 35 | **DEGRADED** |
| 41 | Firefox plante | Perte du WebSocket | = panne 35 | **DEGRADED** |
| 42 | RTT dégradé (> 300 ms) | Mesuré par le heartbeat | IHM grisée, dégradation adaptative (vidéo puis lidar), plafond de vitesse abaissé | **WARN** |
| 43 | Manette non fusionnée au boot | `navigator.getGamepads()` vide | Bandeau *« Appuyez sur L + R »* | **info** |
| 44 | **Mauvais navigateur / HTTP au lieu de HTTPS** | Manette vide alors que le système la voit | Bandeau explicite *« Contexte non sécurisé : Firefox n'expose pas les manettes en HTTP »* | **info** |
| 45 | Deux pupitres connectés en même temps | Deux clients rosbridge publient | ⚠️ **Le superviseur n'accepte qu'une session de conduite** : le premier armé garde la main, le second est en lecture seule avec un bandeau explicite | **info** |

⚠️ **La panne 45 est facile à oublier et dangereuse** : un collègue qui ouvre l'IHM « juste pour voir » depuis un PC ne doit pas pouvoir prendre le contrôle en parallèle. Un jeton de session de conduite, attribué à l'armement et libéré au désarmement, résout le problème en quelques lignes.

⚠️ **La panne 36 est la plus sournoise.** Le navigateur ralentit volontairement les timers d'un onglet en arrière-plan. Si l'opérateur bascule vers une autre fenêtre en tenant ZL, la boucle à 20 Hz passe à 1 Hz et la commande devient erratique. Traiter `visibilitychange` comme un relâchement d'homme-mort est **obligatoire**, pas optionnel.

---

## W.9 Plan de réalisation

S'insère après la **Phase 5** du §V (premiers déplacements) — et une partie peut se faire bien plus tôt, contre le matériel simulé.

| Phase | Contenu | Critère de sortie | Peut démarrer |
|---|---|---|---|
| **W-A · Valider la Switch** ⚠️ | Les 5 tests ci-dessous | Manette combinée visible dans Firefox avec `mapping: "standard"` | **Tout de suite** |
| **W-B · Socle IHM** | nginx + TLS, rosbridge, PWA conduite + télémétrie, `retriever_teleop_guard`, homme-mort, heartbeat | Conduite d'un robot **simulé** (`use_mock_hardware:=true`) depuis la Switch | Après Phase 1 |
| **W-C · Sécurité de la téléop** | Tests d'injection de panne : coupure Wi-Fi, onglet masqué, manette débranchée, RTT dégradé | Arrêt en rampe < 1,8 s dans tous les cas, réarmement explicite vérifié | Après W-B |
| **W-D · Capteurs** | Vue lidar Canvas, `web_video_server`, dégradation adaptative | Lidar à 7 Hz et vidéo à 10 fps sous 2 Mbit/s mesurés | Après Phase 6 |
| **W-E · Missions** | Leaflet + PMTiles hors ligne, actions Nav2, édition de waypoints | Mission GPS 3 points lancée et suivie depuis la Switch | Après Phase 7 |
| **W-F · Durcissement** | Mode kiosque, jeton de session, mesure d'autonomie, étiquettes de procédure | Session de terrain de 30 min sans intervention | Après W-E |

### ⚠️ Les cinq tests de la phase W-A — à faire avant toute ligne de code

Ils prennent une demi-heure et peuvent invalider tout le reste du chapitre.

| # | Commande | Ce qu'on cherche |
|---:|---|---|
| 1 | `cat /proc/bus/input/devices` | Confirmer `Nintendo Switch Left/Right Joy-Con Serial`, PID `2006` / `2007`, et la présence ou non d'un périphérique IMU |
| 2 | `systemctl status joycond`, puis **appuyer L+R** | Voir apparaître `Nintendo Switch Combined Joy-Cons` (`057e:2008`) et son `/dev/input/js*` |
| 3 | Ouvrir un testeur de manette **en HTTPS ou sur `localhost`**, dans **Firefox** puis dans **Chromium** | Comparer `gamepad.id`, `gamepad.mapping`, le nombre d'axes et de boutons. ⚠️ **C'est ce test qui tranche l'hypothèse « Chromium ne voit rien »** |
| 4 | `evtest` sur le périphérique combiné, sticks relâchés, 60 s | Mesurer la dérive réelle → dimensionner la zone morte applicative |
| 5 | `cat /sys/class/power_supply/*/current_now` pendant 30 min d'IHM | Obtenir enfin un chiffre d'autonomie |

**Si le test 3 échoue dans les deux navigateurs**, il reste deux issues avant d'abandonner la Switch : un petit démon local qui lit evdev et relaie en WebSocket vers la page (contourne entièrement l'API Gamepad), ou une manette USB/Bluetooth externe branchée sur la Switch. Aucune des deux n'est bloquante pour l'architecture — le pupitre reste un client léger interchangeable.

---

## W.10 Ce que ça ajoute au BOM

| Élément | Qté | Fonction | Prix 📐 | Priorité |
|---|---:|---|---:|---|
| **E-stop radio (émetteur + récepteur, contact NF)** | 1 | ⚠️ **Passe de recommandé à impératif** dès que la téléop est en service | 90 € | ★★★ **impératif** |
| Caméra USB RGB grand angle (UVC, 640×480 suffit) | 1 | Retour visuel de conduite — le Kinect est inutilisable en extérieur (§N.4) | 35 € | ★★★ |
| Point d'accès Wi-Fi / carte M.2 en mode AP | 1 | Réseau de terrain isolé | 0–40 € | ★★ |
| Batterie externe USB-C PD 30 W | 1 | Autonomie du pupitre | 35 € | ★★ |
| Sangle / harnais de cou pour la Switch | 1 | Deux mains sur la manette, pas sur la console | 15 € | ★★ |
| Carte microSD 128 Go U3/A2 | 1 | Requise par switchroot (16 Go minimum) | 20 € | ★ |
| Film antireflet | 1 | Écran brillant + soleil = illisible | 10 € | ★ |

📐 **Sous-total : ≈ 245 €**, dont 90 € qui relèvent de la sécurité.

---

## W.11 En résumé

**Ce qui est solide** : l'architecture client léger est la bonne, elle découle directement du §O ; toutes les briques ROS 2 existent et sont maintenues (rosbridge 2.7.0 très actif, roslib 2.1.0 réécrit en 2026) ; le budget réseau tient largement (1,9 Mbit/s sur un lien qui en offre 15–30) ; la sécurité de la téléop est traitable avec des mécanismes éprouvés.

**Ce qui est fragile** : la Switch elle-même. Noyau 4.9, Chromium abandonné à la 126, chaîne d'entrée à quatre étages (serdev → joycond → uinput → evdev) à valider empiriquement, sticks qui dérivent, autonomie inconnue. **Aucun de ces points n'est bloquant pour le robot** — parce que le pupitre est interchangeable et que sa perte est un cas nominal géré par le superviseur. Mais il faut faire la phase W-A avant d'investir du temps dans l'IHM.

**Ce qui manque et qu'il faut acheter** : l'e-stop radio. Piloter à 30 m un robot de 35 kg dont le seul arrêt d'urgence est un champignon fixé sur le robot lui-même, c'est le trou de l'architecture actuelle.

**Le point à ne pas oublier** : le bouton rouge de l'écran n'est **pas** un arrêt d'urgence, et ne doit jamais être étiqueté comme tel.

---

### Sources de cette section

[Switchroot Wiki — Linux Features](https://wiki.switchroot.org/wiki/linux/linux-features.md) · [Guide Ubuntu Noble](https://wiki.switchroot.org/wiki/linux/l4t-ubuntu-noble-installation-guide.md) · [Changelog Linux switchroot](https://wiki.switchroot.org/wiki/linux/linux-changelog) · [CTCaer/switch-l4t-kernel-4.9](https://github.com/CTCaer/switch-l4t-kernel-4.9) · [CTCaer/joycond](https://github.com/CTCaer/joycond) · [DanielOgorchock/joycond](https://github.com/DanielOgorchock/joycond) · [chromium/device/gamepad](https://github.com/chromium/chromium/tree/main/device/gamepad) · [gecko-dev/dom/gamepad](https://github.com/mozilla/gecko-dev/tree/master/dom/gamepad) · [Mozilla Hacks — Securing the Gamepad API](https://hacks.mozilla.org/2020/07/securing-gamepad-api/) · [MDN — Using the Gamepad API](https://developer.mozilla.org/en-US/docs/Web/API/Gamepad_API/Using_the_Gamepad_API) · [RobotWebTools/rosbridge_suite — protocole v2](https://github.com/RobotWebTools/rosbridge_suite/blob/ros2/ROSBRIDGE_PROTOCOL.md) · [roslibjs](https://github.com/RobotWebTools/roslibjs) · [RobotWebTools/web_video_server](https://github.com/RobotWebTools/web_video_server/blob/ros2/README.md) · [foxglove/foxglove-sdk](https://github.com/foxglove/foxglove-sdk) · [Foxglove vs Foxglove Studio, two years on](https://foxglove.dev/blog/foxglove-vs-foxglove-studio-two-years-on) · [lichtblick-suite/lichtblick](https://github.com/lichtblick-suite/lichtblick) · [Leaflet](https://leafletjs.com/) · [protomaps/PMTiles](https://github.com/protomaps/PMTiles) · [IEC 62745:2017](https://webstore.iec.ch/en/publication/31998) · [Machinery Safety 101 — Wireless E-stop](https://machinerysafety101.com/2026/05/25/wireless-estop/) · [ISO 3691-4:2023 (preview ANSI)](https://webstore.ansi.org/preview-pages/ISO/preview_ISO+3691-4-2023.pdf) · [Fictionlab — WebRTC on robots](https://fictionlab.pl/blog/webrtc-on-robots-how-to-stream-live-video-from-your-rover-to-any-browser/) · [iFixit — batterie HAC-003](https://www.ifixit.com/products/nintendo-switch-console-replacement-battery)
