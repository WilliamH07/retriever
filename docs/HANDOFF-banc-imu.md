# HANDOFF — banc B1, la chaîne IMU

**État au 20 septembre 2026.** Branche `feat/link-layer-and-imu-bench`, 25 commits
depuis `main`. Ce document existe pour qu'on puisse reprendre ce travail dans
trois semaines sans relire l'historique : ce qui marche et avec quels chiffres,
ce qui a été cassé et pourquoi, ce qui reste et comment le faire.

---

## 1. Ce qui fonctionne, mesuré

La chaîne complète : **BNO085 → SPI → ESP32 → série 921 600 → ROS 2 Jazzy →
`/imu/data` → foxglove_bridge → Foxglove Studio**.

| Mesure | Valeur | Où la relire |
|---|---|---|
| Cadence `/imu/data` | 99,8 Hz pour 100 attendus | `ros2 topic hz /imu/data` |
| Erreurs de liaison | 0 CRC, 0 format, 0 débordement | `/retriever/link_status` |
| Erreurs SHTP du capteur | 0 sur 10 000 rapports consécutifs | journal `imu:` du firmware |
| Rapports indécodables | 0 | idem |
| Norme du quaternion | écart de 7,8 × 10⁻⁶ à 1 | `/retriever/imu_status` |
| Aller-retour de liaison | médiane 3,14 ms, max 3,88 ms | `tools/link_monitor.py` |
| Latence aller simple retenue | **1,57 ms** | `link_bench.yaml`, mesurée ✅ |
| Hash du protocole | `0xE8391C47`, 30 trames | concordant hôte/nœud |

Le nœud ROS se relance sans toucher au câble, autant de fois qu'on veut.

---

## 2. Les défauts trouvés, et leur mécanisme

C'est la partie qui a de la valeur : chacun de ces défauts était **silencieux**,
et aucun ne ressemblait à sa cause.

### 2.1 Le SPI est full-duplex — il n'existe pas de « lecture » (`2aad2a5`)

Pendant qu'on lit un paquet du BNO085, MOSI est quand même piloté, et le
composant lit ce qui y passe comme un en-tête SHTP venant de l'hôte. ESP-IDF,
quand `tx_buffer` vaut `NULL`, ne garnit pas la FIFO d'émission : MOSI rejoue la
transaction précédente. Le capteur voyait donc un paquet malformé à **chaque
lecture**, empilait une erreur sur le canal 0, ce qui maintenait `H_INTN` bas,
ce qui relançait une lecture. Symptôme : 23 000 lectures pour zéro événement
capteur. Correction : un tampon de zéros sur MOSI.

### 2.2 Le réveil PS0/WAKE avant chaque écriture (`ba0a38b`)

Après reset, le concentrateur dort : il n'asserte `H_INTN` que si l'hôte tire
`H_WAKEN` — la broche PS0, repurposée après le reset — à la masse. Ce réveil
avait été **retiré à tort**, parce que tant que le défaut 2.1 était présent le
capteur avait toujours une erreur à signaler, `H_INTN` restait bas en
permanence, et l'absence de réveil ne se voyait pas. **Les deux défauts se
masquaient l'un l'autre** — c'est pour ça que la trace vaut mieux que le
raisonnement.

### 2.3 L'étalonnage n'était jamais activé (`a2126e3`)

Le firmware n'appelait pas `sh2_setCalConfig`. Le BNO085 sortait donc ses
valeurs d'usine. Mesuré sur trois orientations franches :

```
à plat        (0,270 ; -0,230 ; -10,152)   norme 10,158
sur un chant  (0,382 ; -10,188 ; -0,304)   norme 10,200
sur l'autre   (-0,270 ; 9,460 ; -1,266)    norme  9,548
```

Les normes ne sont pas égales → ce n'est pas un facteur d'échelle. Un seul
vecteur de biais `b = (+0,27 ; −0,378 ; −0,34) m/s²` recale les deux premières à
trois millièmes de 9,81. **Norme du biais : 0,58 m/s².** La même omission
explique le cap : le capteur annonçait lui-même π radians d'incertitude.

### 2.4 Le bootloader ROM (`a27c137`, `5e9049b`)

Sur une DevKitC, `DTR` et `RTS` pilotent `IO0` et `EN` par le circuit
d'auto-reset. Ouvrir un port les fait bouger, et selon l'ordre des transitions
la carte démarre en **mode téléchargement** : elle y attend un téléversement, à
115 200, et reste muette pour un hôte qui lit à 921 600 — sans qu'aucune erreur
ne soit levée nulle part. Une carte déjà dans cet état y reste jusqu'à une
coupure d'alimentation.

⚠️ **Le silence complet et le bruit illisible sont deux états du même
problème**, pas deux pannes différentes. On les a traités séparément pendant une
demi-journée.

Correction : le transport ROS et les outils Python relâchent `DTR`/`RTS`,
enlèvent `HUPCL`, et **exécutent une remise en mode exécution à l'ouverture** —
`IO0` haut, `EN` bas 120 ms, `EN` haut, 400 ms d'attente, purge. Paramètre
`serial.reset_on_open`, à passer à `false` le jour où redémarrer le
microcontrôleur au démarrage du nœud ne sera plus acceptable. Sur CAN la
question disparaît.

### 2.5 Faux signaux, corrigés aussi

| Défaut | Correction |
|---|---|
| Le reset d'initialisation du BNO085 compté comme un incident → IMU au rouge dès le boot | `e027b46` |
| `IMU_CAL` non consommée côté ROS → `unknown_frames` alors que le hash concordait | `5ac9e5c` |
| Le moniteur mesurait son propre délai pyserial et l'appelait latence de liaison — 53 ms au lieu de 3 | `ba0a38b` |
| `imu_cal.py` affichait la même chose devant un port muet et devant un flux valide | `2625b1d` |

---

## 3. Ce qui reste

**Tous les points restants demandent la carte montée sur le robot.** Les mesurer
sur un banc n'aurait pas de sens — c'est une décision prise, pas un oubli.

### 3.1 Étalonnage — à faire en premier après le montage

L'étalonnage du magnétomètre est **spécifique à l'environnement magnétique** :
fait sur un banc, il serait à refaire une fois la carte près des moteurs.

```
python3 tools/imu_cal.py --device <port>          # observer
# six faces, 1 s chacune → accel ; immobile 3 s → gyro ; huit en l'air → mag
python3 tools/imu_cal.py --device <port> --save   # sauvegardes doit passer a 1
```

Procédure détaillée : `docs/DEMARRAGE.md` §4 bis.

**Critère** : norme de `linear_acceleration` à 9,81 ± 0,05 dans **toutes** les
orientations, et `orientation_covariance[8]` qui tombe de 9,87 (π², soit 180°)
à ~0,008 (5°). Le diagnostic ROS affiche « étalonnage non sauvegardé » jusqu'à
ce que ce soit fait — c'est volontaire.

### 3.2 Covariances mesurées — remplacer les valeurs 📐

`link_bench.yaml` contient encore deux ordres de grandeur inventés :
`angular_velocity_stddev: 0.01` et `linear_acceleration_stddev: 0.1`.

Robot immobile, 60 s d'enregistrement, écart type par axe.

⚠️ **Plancher connu** : la résolution du gyromètre du BNO085 est 1/512 rad/s,
soit 0,001953. Le bruit mesurable ne descendra pas sous σ = 0,001953/√12 ≈
**0,00056 rad/s**. La valeur actuelle de 0,01 est donc dix-huit fois trop
pessimiste — ça ne casse rien (un EKF trop prudent converge lentement, il ne
diverge pas) mais c'est exactement le genre de chiffre qu'on est censé remplacer.

### 3.3 Endurance 10 minutes

Critère de sortie du §AG : zéro erreur de liaison sur dix minutes, cadence
stable. Se lit dans `/retriever/link_status`.

### 3.4 Magnétomètre à trois distances d'un moteur de roue

C'est **la mesure qui tranche une décision d'architecture** : §AF.7 laisse le
choix du mode de fusion ouvert entre `ROTATION_VECTOR` (9 axes, cap absolu,
sensible au magnétique) et `GAME_ROTATION_VECTOR` (6 axes, immunisé, cap qui
dérive). Publier `/imu/mag` sert précisément à ça : observer de combien la norme
du champ s'écarte du champ terrestre local (25 à 65 µT) une fois le capteur à sa
place, moteurs alimentés. 🔴 **La décision ne peut pas être prise avant.**

---

## 4. Où se trouve quoi

| | |
|---|---|
| Installation, flash, vérification, dépannage | `docs/DEMARRAGE.md` |
| Architecture, recette du banc §AG, conventions de repères | `docs/architecture/13-architecture-logicielle-liaison.md` |
| Source de vérité du protocole | `firmware/protocol/protocol.yaml` |
| Le portage SPI, avec les trois pièges expliqués en tête de fichier | `firmware/components/retriever_imu/src/sh2_hal_esp32_spi.c` |
| Mise en page Foxglove | `docs/foxglove/bench_imu.json` |
| Voir la liaison sans ROS | `tools/link_monitor.py` |
| Piloter l'étalonnage | `tools/imu_cal.py` |

⚠️ **Aucune fonction de sécurité n'est implémentée ni validable sur ce banc.** La
liaison série ne donne ni arbitrage, ni acquittement, ni retransmission, ni
confinement de faute. Les niveaux d'arrêt N4 et N5 restent matériels. Voir §AB.5.

---

*Copyright (c) 2026 William Hanczyk — Apache License 2.0*
