# Démarrage — du dépôt au flux IMU dans ROS 2

Ce guide part d'une machine vierge et s'arrête quand `/imu/data` sort à 100 Hz
dans Foxglove. Il couvre ce qu'il faut installer, comment flasher l'ESP32,
comment vérifier que ça marche à chaque étage, et les pièges déjà rencontrés.

**État au 19 septembre 2026** — la chaîne IMU fonctionne de bout en bout côté
ESP32 : `IMU_QUAT`, `IMU_GYRO` et `IMU_ACCEL` sortent à 100 Hz, sans erreur
SHTP ni CRC. Le nœud ROS compile mais n'a pas encore été lancé sur le
calculateur. Aucune fonction de sécurité n'existe : ce banc ne fait que
transporter de la donnée capteur.

---

## 0. Les trois rôles

Rien n'oblige à ce que ce soient trois machines, mais ce sont trois jeux
d'outils distincts, et les mélanger fait perdre du temps.

| Rôle | Machine typique | Ce qu'on y fait | Outils |
|---|---|---|---|
| Poste de flash | le portable | construire et téléverser le firmware, observer la liaison | ESP-IDF v5.5, python3 + pyserial |
| Calculateur | Ubuntu Server 24.04 | faire tourner le nœud ROS et `foxglove_bridge` | ROS 2 Jazzy, colcon |
| Tests hôte | n'importe laquelle | protocole et cadrage, sans matériel | make, gcc, python3 |

L'ESP32 ne peut être branché qu'à une machine à la fois, et **un seul programme
peut tenir le port série**. Le moniteur du poste de flash et le nœud ROS ne
cohabitent pas.

---

## 1. Câblage du BNO085

⚠️ Le piège de ce capteur, et il fait perdre une journée à qui l'ignore : en
SPI, ce n'est **pas** `SDA` qui porte les données vers le capteur. C'est la
broche de sélection d'adresse I²C, sérigraphiée `ADDR`, `ADR`, `SA0`, `DI` ou
`SI` selon les cartes.

| Fonction | DevKitC | Sérigraphies rencontrées |
|---|---|---|
| alimentation | 3V3 | `3V3` · `VIN` |
| masse | GND | `GND` |
| horloge | GPIO18 | `SCL` · `SCK` |
| capteur → ESP32 | GPIO19 | `SDA` · `SO` · `MISO` |
| ESP32 → capteur | GPIO23 | `ADDR` · `ADR` · `SA0` · `DI` · `SI` |
| sélection | GPIO22 | `CS` |
| interruption | GPIO25 | `INT` |
| reset | GPIO33 | `RST` |
| PS0 / WAKE | GPIO26 | `PS0` · `P0` · `WAK` |
| PS1 | 3V3 | `PS1` · `P1` |

⚠️ **PS1 et PS0 doivent être hauts avant le reset**, sinon le composant démarre
en I²C et le SPI ne répondra jamais — sans message d'erreur, parce qu'il n'y a
personne pour en émettre un. Sur les cartes où ce sont des cavaliers à souder,
il faut les fermer.

⚠️ **PS0 est repris comme WAKE après le reset.** Il doit donc aller à un GPIO et
jamais être câblé en dur au 3V3 : le firmware le tire à la masse avant chaque
écriture pour réveiller le capteur.

⚠️ **`RST` doit être câblé**, pas laissé en l'air. Le firmware fait sa propre
séquence de reset après avoir positionné PS0.

Le tableau qui fait foi est en tête de `firmware/esp32_safety/main/board_config.h`.

---

## 2. Récupérer le dépôt

```bash
git clone https://github.com/WilliamH07/retriever.git
cd retriever
git submodule update --init --recursive
```

Le seul sous-module est la pile SH-2 de CEVA (Apache 2.0), épinglée sur la
version 1.4.0. Si le `git submodule update` ne ramène rien :

```bash
tools/fetch_firmware_deps.sh
```

⚠️ `git submodule update --init` ne fait rien, **et ne dit rien**, quand le lien
vers le sous-module a disparu de l'index. L'erreur ne se manifeste alors qu'au
premier `idf.py build`, plusieurs minutes plus tard et plusieurs couches plus
bas. Le script de repli existe exactement pour ça : il vérifie son propre
résultat.

---

## 3. Poste de flash

### Installer ESP-IDF v5.5

Pas Arduino IDE, pas PlatformIO. Le projet utilise l'API ESP-IDF directement,
FreeRTOS à 1 kHz et le pilote TWAI.

**macOS**

```bash
brew install cmake ninja dfu-util python3
mkdir -p ~/esp && cd ~/esp
git clone -b v5.5 --recursive https://github.com/espressif/esp-idf.git esp-idf-v5.5
cd esp-idf-v5.5 && ./install.sh esp32
```

**Linux (Debian / Ubuntu)**

```bash
sudo apt install git wget flex bison gperf python3 python3-pip python3-venv \
                 cmake ninja-build ccache libffi-dev libssl-dev dfu-util \
                 libusb-1.0-0
mkdir -p ~/esp && cd ~/esp
git clone -b v5.5 --recursive https://github.com/espressif/esp-idf.git esp-idf-v5.5
cd esp-idf-v5.5 && ./install.sh esp32
```

**À chaque nouveau terminal**, il faut charger l'environnement — c'est la
première cause de `idf.py: command not found` :

```bash
. ~/esp/esp-idf-v5.5/export.sh
```

⚠️ Le dossier s'appelle `esp-idf-v5.5`, pas `esp-idf` : la version est dans le
nom pour que deux versions puissent coexister, ce qui arrive dès qu'un projet
tiers en exige une autre. Espressif déconseille de mettre cette ligne dans le
`.zshrc` — elle ralentit l'ouverture de chaque terminal. Un alias plutôt :

```bash
echo "alias get_idf='. \$HOME/esp/esp-idf-v5.5/export.sh'" >> ~/.zshrc
```

### Installer le reste

```bash
pip3 install pyserial            # pour tools/link_monitor.py
```

Sur macOS, la DevKitC utilise une puce CP2102 ou CH340. Si aucun
`/dev/cu.usbserial-*` n'apparaît au branchement, installer le pilote VCP du
fabricant. Sur Linux, le noyau les gère tous les deux ; il faut en revanche
être dans le groupe `dialout` :

```bash
sudo usermod -aG dialout $USER   # puis se déconnecter et se reconnecter
```

### Construire et flasher

```bash
. ~/esp/esp-idf-v5.5/export.sh
cd retriever/firmware/esp32_safety
idf.py set-target esp32          # une seule fois, à la première construction
idf.py build
idf.py -p <port> flash
```

Trouver `<port>` :

```bash
ls /dev/cu.usbserial-*           # macOS   → /dev/cu.usbserial-0001
ls -l /dev/serial/by-id/         # Linux   → nom stable, préférable à /dev/ttyUSB0
```

### Vérifier, sans ROS

C'est le premier outil à lancer sur un banc neuf, et le seul dont on ait besoin
pour prononcer les quatre premiers points de la recette de banc. Il ne dépend
que de pyserial : si quelque chose ne va pas, la réponse ne dépend pas d'une
deuxième pile logicielle.

```bash
cd retriever
python3 tools/link_monitor.py --device <port>
```

Ce qu'il faut voir, dans cet ordre :

```
erreurs sur les 10 dernières secondes : 0      ← c'est ce chiffre qui compte
hash du protocole annoncé par le nœud : 0xC1214F10
aller-retour  médiane ~3 ms

IMU_QUAT    0x210    99.9 Hz   attendu 100
IMU_GYRO    0x211    99.9 Hz   attendu 100
IMU_ACCEL   0x212    99.9 Hz   attendu 100
spi lectures=… ecritures=… reveils=0 err_shtp=0
```

`reveils` et `err_shtp` à zéro, c'est le capteur qui va bien. Pour voir les
valeurs elles-mêmes plutôt que les débits :

```bash
python3 tools/link_monitor.py --device <port> --watch IMU_QUAT
```

Penche la carte : `w x y z` doivent suivre, et la norme rester à 1.

⚠️ La ligne `IMU  orientation nulle` n'est pas une panne. C'est l'indice de
confiance que le capteur joint à chaque rapport. Le rotation vector 9 axes
s'appuie sur le magnétomètre : tant que celui-ci n'a jamais été calibré, le
BNO085 annonce honnêtement une précision nulle. Pour la faire monter : une
rotation lente complète autour de chacun des trois axes, puis un huit en l'air,
sur une quinzaine de secondes, loin de tout métal.

---

## 4. Calculateur ROS 2

### Installer ROS 2 Jazzy

Suivre la [page d'installation officielle](https://docs.ros.org/en/jazzy/Installation/Ubuntu-Install-Debs.html)
pour ajouter le dépôt apt. ⚠️ Ne pas recopier la procédure d'un vieux tutoriel :
la façon d'ajouter la clé et la source a changé en 2025.

Ensuite :

```bash
sudo apt install ros-jazzy-ros-base ros-jazzy-foxglove-bridge \
                 python3-colcon-common-extensions python3-rosdep build-essential
sudo rosdep init && rosdep update
sudo usermod -aG dialout $USER   # puis se déconnecter et se reconnecter
```

### Construire l'espace de travail

```bash
git clone https://github.com/WilliamH07/retriever.git
cd retriever
git submodule update --init --recursive
cd ros2_ws
source /opt/ros/jazzy/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
```

⚠️ Le paquet `retriever_link` compile trois fichiers C pris **dans
`firmware/`** : le cadrage COBS et le CRC sont littéralement le même code des
deux côtés de la liaison, et c'est voulu. L'espace de travail ne se construit
donc pas séparé du dépôt. Pour le faire quand même, passer
`-DRETRIEVER_FIRMWARE_DIR=<chemin>/firmware`.

### Lancer

Débrancher l'ESP32 du poste de flash, le brancher ici, puis :

```bash
ls -l /dev/serial/by-id/
ros2 launch retriever_bringup bench_imu.launch.py \
     device:=/dev/serial/by-id/usb-Silicon_Labs_CP2102_...
```

Le lancement démarre deux nœuds : `retriever_link_bridge`, qui décode le série
et publie `/imu/data`, et `foxglove_bridge` sur le port 8765, en écoute sur
toutes les interfaces.

### Vérifier

```bash
ros2 topic hz /imu/data              # ~100 Hz
ros2 topic echo /imu/data --once     # quaternion de norme 1, accel z ≈ 9,81 au repos
ros2 topic echo /retriever/link_status --once
ros2 topic echo /retriever/imu_status --once
```

Puis Foxglove Studio → *Open connection* → `ws://<adresse-du-calculateur>:8765`.

⚠️ Le banc publie une transformation `imu_world → imu_link` pour qu'on voie le
capteur bouger dans le panneau 3D. C'est une **aide de banc**, à désactiver le
jour où l'EKF entre en service : deux sources sur la même arête de l'arbre TF
est une faute. Le paramètre est `bench.publish_tf` dans
`ros2_ws/src/retriever_bringup/config/link_bench.yaml`.

---

## 4 bis. Étalonner l'IMU

⚠️ **À faire une fois, avant toute mesure.** Sans ça le BNO085 sort ses valeurs
d'usine : biais accéléromètre de 0,58 m/s² mesuré au banc le 20 septembre 2026,
et un cap dont le capteur annonce lui-même 180° d'incertitude.

Deux choses distinctes doivent être vraies, et les confondre coûte une séance :

| | |
|---|---|
| **actif** | le capteur corrige ses biais en ce moment |
| **sauvegardé** | la correction survivra à la mise hors tension |

Le firmware active les trois capteurs au démarrage et demande la sauvegarde
automatique du DCD. Il le refait après chaque reset du capteur — un reset efface
cette configuration comme il efface les rapports.

Coupe le nœud ROS (un seul programme peut tenir le port), puis :

```bash
python3 tools/imu_cal.py --device <port>
```

La procédure est physique et tient en une minute :

1. **accéléromètre** — poser la carte sur ses six faces, immobile environ une
   seconde à chaque fois. `accel` monte à `haute` ;
2. **gyromètre** — laisser la carte parfaitement immobile trois secondes.
   `gyro` monte à `haute` ;
3. **magnétomètre** — un huit lent en l'air, loin de tout métal et de toute
   alimentation, une quinzaine de secondes. `mag` puis `orientation` montent ;
4. quand les quatre sont à `haute` :

```bash
python3 tools/imu_cal.py --device <port> --save
```

`sauvegardes en flash` doit passer à 1. Tant qu'il vaut 0, tout le travail est
en RAM et disparaît à l'extinction.

Pour repartir de zéro si un étalonnage a mal tourné — un huit fait près d'un
moteur, par exemple :

```bash
python3 tools/imu_cal.py --device <port> --clear
```

C'est irréversible, l'outil demande confirmation.

**Vérification après étalonnage**, côté ROS :

- la norme de `linear_acceleration` doit valoir 9,81 ± 0,05 m/s² dans **toutes**
  les orientations. Si elle reste constante mais fausse, c'est l'échelle ; si
  elle varie avec l'orientation, c'est qu'il reste un biais ;
- `orientation_covariance[8]` doit tomber de 9,87 (π², soit 180° d'incertitude)
  à quelque chose de l'ordre de 0,008 (5°).

---

## 5. Tests hôte

Aucun matériel, aucun ESP-IDF, aucun ROS. C'est ce qui tourne le plus vite et
ce qu'il faut lancer avant de se demander si le problème vient de la carte.

```bash
make -C firmware/test            # protocole et cadrage
python3 tools/check_protocol_sync.py
```

`protocol.yaml` est la source de vérité : l'en-tête C du firmware, l'en-tête C++
de ROS et la table de la documentation en sont **générés**. Après toute
modification :

```bash
python3 firmware/protocol/generate.py
python3 firmware/protocol/generate.py --check   # échoue si un fichier a divergé
```

Le hash sémantique du protocole voyage dans chaque `HEARTBEAT`. Si le moniteur
annonce autre chose que `0xC1214F10`, le firmware et le PC ne parlent pas la
même version.

---

## 6. Dépannage

| Symptôme | Cause la plus fréquente |
|---|---|
| `idf.py: command not found` | `export.sh` pas chargé dans ce terminal |
| `La bibliotheque SH-2 est absente` à la configuration | sous-module non récupéré → `tools/fetch_firmware_deps.sh` |
| Port occupé, `Resource busy` | le moniteur ou un autre `idf.py monitor` tient déjà le port |
| `Permission denied` sur `/dev/ttyUSB0` | pas dans le groupe `dialout`, ou session pas rouverte depuis |
| Rien ne vient dans le moniteur | mauvais port, ou débit — le firmware est à 921600 |
| Beaucoup d'erreurs « format » les premières secondes | normal : le bootloader ESP32 écrit son journal en clair à 115200 avant que la liaison ne prenne la main. Regarder « erreurs sur les 10 dernières secondes », pas le total |
| `pas de H_INTN apres reset` | PS0/PS1 pas hauts au reset, ou `INT` / `RST` mal câblés |
| `reveil sans reponse` | PS0 pas relié à GPIO26, ou câblé en dur au 3V3 |
| `err_shtp` qui grimpe | le composant refuse ce qu'on lui envoie — voir les en-têtes de `sh2_hal_esp32_spi.c`, les trois pièges connus y sont décrits |
| Orientation « nulle » | magnétomètre non calibré, pas une panne — voir §3 |
| Foxglove ne se connecte pas | pare-feu sur le port 8765, ou mauvaise adresse |

---

## 7. Pour aller plus loin

- L'architecture logicielle complète, la couche liaison et la recette de banc B1 :
  [`docs/architecture/13-architecture-logicielle-liaison.md`](architecture/13-architecture-logicielle-liaison.md)
- Le portage SPI du BNO085, avec les trois pièges résolus expliqués en détail :
  `firmware/components/retriever_imu/src/sh2_hal_esp32_spi.c`
- La définition du protocole : `firmware/protocol/protocol.yaml`

---

*Copyright (c) 2026 William Hanczyk — Apache License 2.0*
