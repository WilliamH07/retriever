# Architecture logicielle — couche de liaison et premier capteur

**Version** : 1.0 — 11 septembre 2026
**Portée** : sections **AA** à **AH**. Complète le dossier v1.2 ; ne le remplace pas.
**Prérequis de lecture** : `02-comms-esp32-moteurs.md` (§F, §G), `03-ros2-etats-boot.md` (§I), `11-corrections-v1.2.md` (C14, C15).

| Marqueur | Signification |
|---|---|
| ✅ **VÉRIFIÉ** | Datasheet constructeur, norme, ou REP officiel. |
| 🟡 **CONSENSUS** | Sources indépendantes concordantes. |
| 🔴 **À MESURER** | Inconnu ou contradictoire. **Interdiction de figer avant mesure.** |
| ⚠️ **RISQUE** | Point de conception dangereux si mal traité. |
| 📐 **HYPOTHÈSE** | Valeur posée faute de donnée ; à confirmer. |

---

# AA. Le problème, et la décision qui en découle

## AA.1 La situation, posée franchement

Le dossier v1.1 tranche : **le bus temps réel est un bus CAN** (§A.1-1, §F.1). Cette décision est bonne et n'est pas rediscutée ici. Mais elle a une conséquence de calendrier que le dossier n'adresse pas :

> La carte `safety_bus_distribution` n'est pas fabriquée. Aucun transceiver n'est câblé. Le §11 Partie 6 place cette carte à l'**étape 1** de l'ordre de fabrication, et le dossier place la navigation autonome à l'**étape 5** du plan de build.

Autrement dit : le logiciel ne peut pas commencer tant que le matériel de communication n'existe pas. C'est faux, et c'est coûteux — parce que la pile ROS 2, l'intégration des capteurs, les conventions de repères et les covariances représentent plusieurs semaines de travail qui ne dépendent en rien du support physique des trames.

## AA.2 La décision

**On sépare le protocole de son transport, et on développe sur un transport série tout en écrivant le code comme si c'était déjà du CAN.**

Concrètement :

| | Aujourd'hui | Quand la carte existera |
|---|---|---|
| Trame applicative | identifiant 11 bits, ≤ 8 octets | **identique** |
| Plan d'adressage | celui du §F.3 | **identique** |
| Code applicatif firmware | ne voit que `rt_frame_t` | **inchangé** |
| Code applicatif ROS 2 | ne voit que `protocol::Frame` | **inchangé** |
| Transport firmware | `link_uart.c` | `link_twai.c` — un fichier |
| Transport ROS 2 | `serial_transport.cpp` | `socketcan_transport.cpp` — un fichier |
| Bascule | — | une option Kconfig, un paramètre de lancement |

Les deux fichiers CAN sont **déjà écrits**. Ils ne sont pas de la prévoyance : ce sont la preuve que l'abstraction est de la bonne taille. S'il avait fallu plus qu'un fichier de chaque côté, c'est que la séparation aurait été mal placée, et il aurait mieux valu s'en apercevoir maintenant que dans trois mois.

## AA.3 Ce que la contrainte des 8 octets apporte, gratuitement

La charge utile est plafonnée à 8 octets **même en série**, où rien ne l'impose. C'est délibéré, et ce n'est pas de la discipline pour la discipline :

- un protocole conçu à l'aise en série ne rentrerait jamais dans CAN 2.0A, et le portage serait une réécriture ;
- la contrainte force à réfléchir à ce qui mérite d'être transmis à 100 Hz — exercice sain ;
- le calcul de charge de bus reste valable d'un transport à l'autre, donc on ne le refait pas.

⚠️ Le seul coût est là où il se voit : **il n'y a pas de place pour un horodatage dans les trames IMU**. Le §AF.4 dit ce qu'on fait à la place, et ce que ça coûte réellement.

---

# AB. La couche de liaison

## AB.1 La trame canonique

```c
typedef struct {
    uint16_t id;        /* 11 bits, plan d'adressage du §F.3 */
    uint8_t  dlc;       /* 0..8 */
    uint8_t  data[8];
} rt_frame_t;
```

C'est le **seul** type que voient les couches supérieures, des deux côtés de la liaison. En CAN, il correspond octet pour octet à une trame de données standard. En série, il est encadré (§AC).

## AB.2 L'interface d'un transport

Trois opérations côté firmware, trois côté calculateur. C'est la mesure exacte de ce qu'il faut réécrire pour changer de support.

```
firmware                          calculateur (C++)
  init(config)                      open()
  send(frame)                       send(frame)
  poll(timeout) → deliver(frame)    receive(frame, timeout)
  stats(out)                        stats()
```

## AB.3 L'architecture d'émission côté firmware

```
   tâche IMU ─────────► file normale  (64 trames) ──┐
   entretien ─────────►                             ├──► tâche TX ──► transport
   crochet RX ────────► file urgente  ( 8 trames) ──┘      prio 20
                                                            (urgente d'abord)
```

⚠️ **La file urgente n'est pas l'arbitrage CAN, et il ne faut pas la prendre pour tel.** Elle ordonne les trames *à l'intérieur d'un nœud*. L'arbitrage CAN, lui, ordonne les trames **entre nœuds**, sur le fil, sans qu'aucun logiciel n'intervienne. Aucune file logicielle ne reproduit cela.

## AB.4 Le dimensionnement des files

| File | Taille | Raison |
|---|---|---|
| TX normale | 64 trames | 3 trames par échantillon IMU à 100 Hz = 300/s. 64 trames couvrent ~200 ms d'à-coup sans perte. |
| TX urgente | 8 trames | Elle ne doit jamais se remplir. Si elle se remplit, le lien est mort et il faut le savoir, pas l'absorber. |
| RX | 32 trames | Le trafic descendant est événementiel (TIME_SYNC à 1 Hz, PING à 1 Hz). |

**Règle absolue** : une file pleine fait **perdre** la trame et incrémenter un compteur. Elle ne bloque jamais l'appelant. Une boucle de contrôle qui attend une liaison saturée est une boucle de contrôle qui a cessé de contrôler.

## AB.5 ⚠️ Ce que le transport série ne fournit pas

C'est la section la plus importante de ce document. Elle existe pour qu'aucune décision future ne s'appuie par inadvertance sur une garantie qui n'existe pas.

| Propriété | CAN 2.0A | Série + COBS |
|---|---|---|
| Arbitrage par priorité, non destructif | ✅ matériel | ❌ approximé par une file logicielle, dans un seul nœud |
| Acquittement par les récepteurs | ✅ | ❌ |
| Retransmission automatique | ✅ | ❌ |
| Confinement de défaut (error-passive, bus-off) | ✅ | ❌ |
| Détection d'erreur | CRC-15 + bourrage + ACK | CRC-16 seul |
| Multipoint sur deux fils | ✅ | ❌ un câble par nœud |
| Immunité EMI près des hacheurs | ✅ paire différentielle | ❌ asymétrique |
| Latence bornée | ✅ | 🟡 bornée en pratique, pas par construction |

**Conséquences de conception, non négociables :**

1. **Aucune fonction de sécurité n'est validée sur le transport série.** Les niveaux N4 et N5 de la chaîne d'arrêt sont matériels et le restent. Le banc série ne sert jamais à prononcer une recette de sécurité.
2. **Le nœud SAFETY, sur le banc, n'a aucune autorité.** Le mot « safety » dans son nom décrit sa destination, pas son état.
3. **Aucun moteur ne tourne sur liaison série.** La chaîne `CMD_WHEELS` → variateur attend le CAN. Elle est décrite dans `protocol.yaml`, générée, et marquée `planned` : rien ne l'émet.
4. **Le passage au CAN ne se fait pas « quand ça marchera bien en série ».** Il se fait quand la carte existe, et il est suivi de la recette complète du §Q niveau 2 — débranchement du bus en marche, terminaison retirée, trame corrompue injectée. Aucun de ces tests n'a d'équivalent en série.

## AB.6 Ce que coûte réellement le passage au CAN

| Étape | Ce qu'il faut faire |
|---|---|
| Firmware | `idf.py menuconfig` → Retriever → Liaison → cocher « Utiliser le bus CAN ». Recompiler. |
| ROS 2 | `transport: "socketcan"` dans `link_bench.yaml`. |
| Linux | `ip link set can0 up type can bitrate 500000 restart-ms 100` |
| Matériel | Transceiver `TCAN1042HV` avec `STB` à la masse (§C5), terminaisons 120 Ω **aux deux extrémités physiques uniquement**, `GPIO5` haut au boot. |
| Vérification | §Q niveau 2 en entier. Pas un sous-ensemble. |

⚠️ Aucune ligne de `imu_bno085.c`, de `bridge_node.cpp`, de `imu_conversion.cpp` ni de `protocol.yaml` ne change. Si l'une d'elles doit changer, c'est un défaut d'architecture et il faut le traiter comme tel.

## AB.7 Écarts assumés par rapport au §F.3

| Point | §F.3 | Ici | Pourquoi |
|---|---|---|---|
| `HEARTBEAT` | DLC 4 | **DLC 8** | Le §K.2 [P3] exige que le heartbeat porte le hash du protocole. Quatre octets ne suffisent pas à porter état + uptime + erreurs + hash. |
| `IMU_GYRO_ACC` (0x211) | « 8+8 → 2 trames » | **0x211 gyro, 0x212 accel** | Deux identifiants explicites valent mieux qu'une trame de 16 octets qui n'existe pas en CAN 2.0A. |
| — | — | **0x213 `IMU_STATUS`** | La qualité d'orientation doit remonter : c'est elle qui alimente la covariance (§AF.5). Les 8 octets de `IMU_QUAT` sont pleins. |
| — | — | **0x214 `IMU_MAG`** | Diagnostic magnétique. Le §N.3 désigne le magnétomètre comme le maillon faible ; encore faut-il pouvoir le mesurer. |
| — | — | **0x330/0x331 `LINK_PING`/`PONG`** | Le §Q niveau 2 demande une latence aller-retour applicative. Il faut une trame pour ça. |
| — | — | **0x7F0 `LOG`** | Tunnel de journalisation (§AC.5). |
| Renommage | `retriever_can`, `retriever_can_bridge` | **`retriever_protocol`**, **`retriever_link`** | Appeler « can » une couche qui abstrait le transport serait contredire l'architecture dans le nom des paquets. |

---

# AC. Le cadrage série

## AC.1 Le format sur le fil

```
   paquet, avant encodage :

   ┌────────┬────────┬─────────────────────────┬────────┬────────┐
   │ id_bas │ id_h+dlc│         données         │ CRC bas│ CRC ht │
   └────────┴────────┴─────────────────────────┴────────┴────────┘
       0        1              2 .. 2+dlc-1

   octet 1 = ((id >> 8) & 0x07) | (dlc << 4)
   CRC-16/CCITT-FALSE des octets précédents, petit-boutiste

   puis : encodage COBS de l'ensemble, puis un octet 0x00

   trame pleine (8 octets utiles)  →  14 octets sur le fil
   2 en-tête + 8 données + 2 CRC + 1 COBS + 1 délimiteur
```

## AC.2 Pourquoi COBS et pas SLIP

Les deux résolvent le même problème : réserver un octet comme délimiteur et le faire disparaître des données.

| | SLIP | **COBS** |
|---|---|---|
| Surcoût typique | 0 | 1 octet |
| Surcoût **pire cas** | ❌ **×2** (paquet entier d'octets d'échappement) | ✅ 1 + ⌈n/254⌉ |
| Resynchronisation | au délimiteur | au délimiteur |
| Coût CPU | comparable | comparable |

Le pire cas décide. Sur une liaison budgétée au plus juste, **un format dont le pire cas est imprévisible est un format qu'on ne peut pas dimensionner** — et un budget qu'on ne peut pas garantir est un budget qu'on découvre dépassé le jour où les données contiennent beaucoup de zéros, c'est-à-dire exactement quand le robot est à l'arrêt.

Référence : Cheshire & Baker, *Consistent Overhead Byte Stuffing*, IEEE/ACM Transactions on Networking, 1999.

## AC.3 Le débit, et pourquoi 921 600 et pas 115 200

Les chiffres viennent directement de `protocol.yaml` — ils sont recalculés à chaque génération, dans `docs/architecture/generated/protocol-tables.md`.

| Charge | Trames/s | Octets/s | @ 115 200 | @ 921 600 |
|---|---:|---:|---:|---:|
| **Banc B1** (IMU seule) | 330 | 4 620 | **40 %** | **5,0 %** |
| **Robot complet** (§F.3 en entier) | 695 | 9 400 | **82 %** | **10,2 %** |

Le banc tiendrait à 115 200. Le robot complet, non — 82 % d'occupation sur une liaison sans contrôle de flux, c'est une liaison qui perd des trames dès le premier à-coup d'ordonnancement.

**Donc on prend 921 600 tout de suite.** Ce n'est pas de l'excès de zèle : c'est l'économie d'une séance de débogage sur un symptôme (« l'IMU saute par moments ») dont la cause (« la liaison est saturée ») ne se voit pas depuis le symptôme. ✅ Le pont CP2102N de la DevKitC supporte 921 600.

## AC.4 La détection d'erreur, honnêtement

**CRC-16/CCITT-FALSE**, polynôme `0x1021`, initialisation `0xFFFF`, sans réflexion ni XOR final. Vecteur de référence : `CRC("123456789") = 0x29B1`, vérifié par `test_framing.c`.

🟡 Distance de Hamming 4 sur des données bien plus longues que nos paquets de 12 octets : toute erreur de 1, 2 ou 3 bits est détectée, ainsi que toute rafale de 16 bits ou moins (tables de Koopman).

⚠️ **Ce n'est pas l'équivalent du CAN.** Le CAN ajoute au CRC-15 un acquittement et une retransmission automatique : une trame corrompue est **réémise**. Ici, elle est **jetée**, et c'est tout. `test_framing.c` vérifie exhaustivement qu'un bit retourné n'importe où dans une trame ne produit jamais une trame acceptée — mais produire zéro trame n'est pas la même chose que produire la bonne.

Le comportement en cas de rafale de bruit est également testé : **on perd au plus une trame**, celle qui se colle au bruit, et le décodeur se recale au délimiteur suivant.

## AC.5 Le tunnel de journalisation

Sur une DevKitC, la console ESP-IDF et la liaison partagent l'UART0 et le même pont USB. Trois options, et une seule qui ne coûte rien :

| Option | Coût |
|---|---|
| Perdre les journaux du firmware | ❌ inacceptable pour une mise au point |
| Liaison sur UART2, console sur UART0 | un second adaptateur USB-série, un second câble |
| **Journaux tunnellisés en trames `LOG`** | ✅ un seul câble — **retenu** |

L'effet secondaire est ce qui emporte la décision : **les journaux du firmware arrivent dans `/rosout`**, horodatés par ROS, enregistrés dans les bags, visibles dans Foxglove au même endroit que tout le reste. Une ligne de firmware et une ligne de Nav2 deviennent comparables dans le temps.

Trois règles dans l'implémentation, dans cet ordre :

1. **Ne jamais bloquer** — un fragment qui ne rentre pas en file est perdu et compté.
2. **Ne jamais récurser** — si le transport journalise, le tunnel ne rejournalise pas.
3. **Ne jamais passer devant** — file normale, jamais la file urgente.

⚠️ **Ce que le tunnel ne couvre pas.** Il détourne les `ESP_LOGx`, et rien d'autre. Continuent d'écrire en clair au milieu des paquets : les messages du **bootloader ROM** (à 115 200, au démarrage), le **gestionnaire de panique** et la trace d'exception, l'abandon sur `ESP_ERROR_CHECK`, les débordements de pile signalés par FreeRTOS, et tout `printf()`.

Le décodeur les rejette et se recale, donc la liaison survit. Mais c'est précisément la trace de plantage — celle qu'on veut le plus — qui arrive hachée. Pour la lire proprement : un adaptateur sur UART2, ou l'option à `n`. C'est un compromis assumé, pas un oubli.

---

# AD. Le protocole comme source unique

## AD.1 Le principe, et le mode de défaillance qu'il évite

Le §P.2-1 pose la règle : une source de vérité par donnée. Pour le protocole, la source est `firmware/protocol/protocol.yaml`, et **rien n'est recopié à la main** :

```
                       firmware/protocol/protocol.yaml
                                    │
                         generate.py│
            ┌───────────────────────┼───────────────────────┐
            ▼                       ▼                       ▼
   retriever_protocol.h      protocol.hpp       protocol-tables.md
      (C99, firmware)      (C++17, ROS 2)         (documentation)
            │                       │
            └── même fichier ───────┘
                compilé des deux côtés

   tools/retriever_wire.py lit le YAML À L'EXÉCUTION
   → troisième implémentation, non générée, donc impossible à désynchroniser
```

Le mode de défaillance visé est précis : **un nœud flashé avec une version du protocole, un calculateur compilé avec une autre**. Il ne produit pas d'erreur franche — il produit des valeurs plausibles et fausses. Un quaternion qui se dégrade lentement parce qu'un champ a bougé d'un octet est un bug qui coûte des jours.

## AD.2 Le hash, et pourquoi il porte sur le sens et non sur le texte

Le générateur calcule un hash SHA-256 tronqué à 32 bits **sur le modèle sémantique** — identifiants, champs, types, offsets, échelles, énumérations — et non sur le texte du fichier.

- Corriger une faute dans un commentaire **ne change pas le hash**, donc ne force pas à reflasher trois nœuds.
- Changer une échelle, un identifiant ou l'ordre d'un champ **change le hash**, donc l'impose.

Ce hash est compilé dans le firmware et remonté dans chaque `HEARTBEAT`. Le nœud ROS le compare au sien. ⚠️ Une divergence est un **refus d'armement**, pas un avertissement (§K.2 [P3]) — sur le banc, c'est un diagnostic `ERROR` et un message qui dit quoi faire.

## AD.3 Ce que la CI vérifie

`tools/check_protocol_sync.py`, trois étapes :

1. **Les fichiers générés correspondent-ils au YAML ?** Sinon, quelqu'un a édité le YAML sans régénérer, ou édité un fichier généré à la main.
2. **Le C et le Python produisent-ils les mêmes octets ?** Vérifié sur les 28 trames, à travers le cadrage complet — COBS et CRC compris.
3. **Les vecteurs d'or tiennent-ils ?** Le format du fil est figé : un enregistrement d'aujourd'hui doit rester lisible dans six mois.

⚠️ Les vecteurs d'or ne sont pas décoratifs. Sans eux, une modification du générateur peut changer silencieusement le format du fil : le firmware et ROS resteraient d'accord entre eux, et tous les deux d'accord sur le mauvais format. Rien n'échouerait, et les bags passés deviendraient illisibles.

---

# AE. Le socle firmware

## AE.1 Choix de la chaîne d'outils

**ESP-IDF v5.5, pas PlatformIO, pas Arduino.** Le dossier §P.1 mentionnait `platformio.ini` ; c'est corrigé ici.

| | Raison |
|---|---|
| ESP-IDF | FreeRTOS exposé directement, Kconfig, contrôle des piles et des priorités, TWAI natif, pas de couche Arduino qui masque les latences |
| v5.5 et non v6.1 | ✅ v6.1 est la version stable actuelle (août 2026), mais v6.0 a supprimé les pilotes hérités et la documentation communautaire — BNO08x, TWAI, PCNT — est encore massivement sur 5.x. 📐 v5.5 est supportée 30 mois à compter de sa sortie mi-2025, donc bien au-delà de l'horizon du projet. La migration vers 6.x est un élément de plan, pas une urgence. |

## AE.2 Arborescence

```
firmware/
├── protocol/
│   ├── protocol.yaml            ◄── SOURCE UNIQUE
│   ├── generate.py
│   └── generated/retriever_protocol.h
├── components/                  ◄── partagés SAFETY et MOTION
│   ├── retriever_protocol/      en-tête généré
│   ├── retriever_link/
│   │   ├── portable/            C99 pur — compilé AUSSI par ROS 2
│   │   │   ├── rt_cobs.c/.h
│   │   │   ├── rt_crc16.c/.h
│   │   │   └── rt_framing.c/.h
│   │   └── src/  link.c  link_uart.c  link_twai.c  link_log.c
│   └── retriever_imu/
│       ├── src/imu_bno085.c            interface minimale
│       ├── src/sh2_hal_esp32_spi.c     le portage, c'est NOTRE code
│       └── vendor/sh2/                 sous-module CEVA, Apache 2.0
├── esp32_safety/                projet ESP-IDF
└── test/                        tests hôte, sans matériel
```

Le répertoire `portable/` mérite qu'on s'y arrête : **ces trois fichiers C sont compilés à l'identique par le firmware ESP32, par le nœud ROS 2 et par les tests hôte.** Les recopier dans le paquet ROS serait plus commode à empaqueter, et garantirait qu'ils divergent un jour. Le dépôt est un seul projet (§P.2-2) : le chemin relatif est légitime, et `-DRETRIEVER_FIRMWARE_DIR=` permet de construire hors du dépôt si le besoin se présente.

## AE.3 La pile SH-2 n'est pas réécrite

Le BNO085 parle SHTP : une pile à états, avec des numéros de séquence par canal, un contrôle de flux et une gestion de reset. CEVA la publie sous **Apache 2.0** ✅ — la licence du code de ce projet.

**On l'attache en sous-module, épinglée sur `v1.4.0`, et on n'y touche pas.** Ce qui appartient au projet, c'est le portage : `sh2_hal_esp32_spi.c`, cinq fonctions. Réimplémenter SHTP serait recopier un travail déjà fait, déjà débogué, sous une licence compatible — et hériter de ses bugs sans hériter de ses corrections.

```
git submodule update --init --recursive       # ou tools/fetch_firmware_deps.sh
```

## AE.4 Tâches et priorités

| Tâche | Prio | Période | Rôle |
|---|:-:|---|---|
| `link_rx` | 23 | événementiel | lecture, décodage, crochet, mise en file |
| `link_tx` | 20 | ≤ 20 ms | file urgente d'abord, puis file normale |
| `imu` | 18 | sur `H_INTN` | `sh2_service()`, assemblage du triplet |
| `imu_pub` | 17 | sur échantillon | emballage et émission des trames |
| `house` | 5 | 100 ms | heartbeat, compteurs, journal périodique |

Réservé pour la suite : la tâche `CONTROL` du §G.4, priorité **24**, période 5 ms — au-dessus de tout ce qui précède. C'est pour elle que la place est laissée.

⚠️ **`CONFIG_FREERTOS_HZ=1000`.** Par défaut, FreeRTOS bat à 100 Hz : `pdMS_TO_TICKS(1)` vaut alors un tick de 10 ms, et toute tâche plus rapide que 100 Hz devient impossible à cadencer. C'est la valeur la plus importante de `sdkconfig.defaults`, et c'est celle qu'on oublie.

## AE.5 Les règles du §G.4, et où elles sont appliquées

| Règle | Où |
|---|---|
| Aucune allocation dynamique après l'initialisation | files FreeRTOS créées une fois, tampons statiques partout |
| Aucun appel bloquant dans le chemin temps réel | `rt_link_send(f, 0)` ou `(f, 1 tick)`, jamais `portMAX_DELAY` |
| Limites de sécurité en dur | à venir avec la FSM ; aucune limite n'est configurable par CAN |
| **État sûr en premier** | `hw_init_safe_state()` est le premier appel d'`app_main()` |
| Tout compteur d'erreur est publié | `rt_link_stats_t` → `HEARTBEAT` → `/retriever/link_status` → `/diagnostics` |
| Le protocole vient du fichier généré | aucune constante de protocole recopiée à la main |

`hw_init_safe_state()` est aujourd'hui **vide** : sur le banc, il n'y a ni contacteur, ni ligne `/SAFE`, ni variateur à mettre en sécurité. La fonction existe et est appelée en premier pour que, le jour où elle aura un contenu, l'ordre des opérations n'ait pas à être rediscuté.

---

# AF. La chaîne ROS 2

## AF.1 Le graphe, aujourd'hui

```
   ESP32-SAFETY ──série 921600──► retriever_link_bridge ──┬──► /imu/data            (sensor_msgs/Imu)     100 Hz
                                                          ├──► /imu/mag             (MagneticField)        10 Hz
                                                          ├──► /retriever/imu_status                       10 Hz
                                                          ├──► /retriever/link_status                       2 Hz
                                                          ├──► /retriever/node_status                      10 Hz
                                                          ├──► /diagnostics                                 2 Hz
                                                          ├──► /rosout    ← journaux du firmware
                                                          └──► /tf        ⚠️ banc uniquement
                                                                    │
                                                          foxglove_bridge ──ws://…:8765──► Foxglove Studio
```

## AF.2 Les paquets

| Paquet | Contenu |
|---|---|
| `retriever_protocol` | en-têtes générés, aucune dépendance ROS |
| `retriever_msgs` | `ImuStatus`, `LinkStatus`, `NodeStatus` |
| `retriever_link` | les deux transports, la conversion, le nœud `link_bridge` |
| `retriever_bringup` | `bench_imu.launch.py`, `config/link_bench.yaml` |

**Il n'y a pas de message maison pour l'IMU.** C'est `sensor_msgs/Imu`, et c'est non négociable : tout ce qui suit dans la pile — `robot_localization`, `nav2_collision_monitor`, `imu_filter_madgwick` si un jour il sert, Foxglove, `rosbag2` — attend ce type. Un message maison ferait de chaque consommateur un adaptateur à écrire.

## AF.3 Les conventions de repères, résolues une fois pour toutes

C'est le point qui fait perdre le plus de temps, alors il est tranché ici.

**Le repère monde.** ✅ Le Rotation Vector du BNO085 est référencé au nord magnétique et à la gravité, dans la convention Android : **ENU** — x est, y nord, z haut. ✅ REP-103 impose exactement la même convention aux repères monde de ROS. **Il n'y a donc aucune conversion de repère monde à faire, et en faire une serait une erreur.**

**Le repère capteur.** ✅ Le BNO085 utilise le repère Android : X vers la droite du boîtier, Y vers le haut dans le plan de la face, Z sortant de la face. C'est un repère **direct**, donc l'inversion d'axe que prévoit REP-145 pour les capteurs indirects ne s'applique pas.

**Le montage.** ⚠️ **Le firmware n'applique AUCUNE rotation de montage.** Il publie ce que le capteur mesure, dans les axes du capteur. Le passage du repère capteur au repère du robot est décrit **une seule fois, dans l'URDF**, par la position de `imu_link` par rapport à `base_link`.

C'est la manière ROS de faire, et surtout c'est la seule qui reste vraie quand on démonte le capteur pour le remonter autrement : on corrige un fichier, pas un firmware, et `robot_localization` lit la correction dans TF sans qu'on lui dise rien.

⚠️ Corollaire pour le banc : tant que `imu_link` n'est pas placé dans l'URDF, l'orientation affichée est celle du **boîtier**, pas celle du robot. C'est normal. Le test des six faces du §Q niveau 3 est ce qui validera le placement URDF, et il ne peut pas être fait avant.

## AF.4 L'horodatage, et ce qu'il vaut

Les huit octets d'une trame IMU sont pleins : il n'y a pas de place pour un horodatage. Le §F.4 décrit une régression linéaire `t_ros = a·t_local + b` alimentée par `TIME_SYNC` — elle suppose un horodatage par échantillon qui n'existe pas dans le plan d'adressage du §F.3.

**Ce qui est fait à la place :**

```
   t_message = now() − latency_offset
```

où `latency_offset` est une **constante mesurée**, pas devinée. `LINK_PING` / `LINK_PONG` donnent l'aller-retour applicatif ; la moitié de sa médiane est l'aller simple. `link_monitor.py` affiche directement la valeur à reporter dans `link_bench.yaml`.

| | |
|---|---|
| Gigue attendue | 📐 1 à 2 ms — transmission série (152 µs pour 14 octets à 921 600) plus la granularité de scrutation du pont USB |
| Suffisant pour | un EKF à 30 Hz, une odométrie à 50 Hz — c'est la précision que le §F.4 annonçait déjà |
| Insuffisant pour | de la datation fine d'événements, un couplage serré avec un lidar |
| Si ça devient insuffisant | ajouter une trame d'horodatage dédiée — le plan d'adressage a de la place — et alors seulement mettre en œuvre la régression du §F.4 |

Les trois trames d'un même échantillon portent le **même compteur `seq`**, et le pont refuse d'assembler un quaternion et une accélération qui ne viennent pas du même instant. ⚠️ C'est un cas qu'un EKF ne sait pas détecter : mélanger deux échantillons produit des données parfaitement plausibles et fausses.

## AF.5 Les covariances — la partie qui fait diverger les EKF

⚠️ **Une matrice de covariance à zéro ne signifie pas « pas d'information ». Pour un EKF, elle signifie « confiance infinie ».** C'est la façon la plus efficace de faire diverger une localisation, et elle ne produit aucun message d'erreur. Le §I.2 du dossier le signalait déjà pour le GPS ; il vaut identiquement pour l'IMU.

**Orientation.** Matrice diagonale, `diag(σ_roulis², σ_tangage², σ_lacet²)`.

| Terme | Valeur | Source |
|---|---|---|
| Roulis, tangage | 3,5° = 0,0611 rad | ✅ erreur dynamique de la datasheet BNO08x. Ces deux angles sont observés par la gravité : ils sont bons et stables. |
| Lacet, plancher | 5° = 0,0873 rad | ✅ §I.2 du dossier, « en pratique typiquement 5° ». On ne croit pas le capteur en dessous, même quand il s'annonce meilleur. |
| Lacet, valeur courante | `max(estimation du capteur, plancher)` | Le BNO085 fournit une estimation d'erreur de cap en radians ; c'est elle qui sert. |
| Lacet, si non fourni | 1 rad ≈ 57° | 📐 cas du *game rotation vector*, qui n'a aucune référence de cap. Déclarer le lacet **inconnu** est exact ; publier 0 serait un mensonge. Le firmware le signale par la sentinelle **6,5535 rad** — la borne haute exacte de l'encodage — que le pont reconnaît. |

⚠️ 📐 **Hypothèse assumée** : la covariance est diagonale dans les axes du capteur, et l'incertitude de cap est portée par l'axe z du capteur. Ce n'est exact que si le capteur est à peu près horizontal — l'incertitude de cap est en réalité autour de la verticale du lieu. Sur terrain plat, l'écart est négligeable ; sur une pente forte, il ne l'est plus. Le jour où ça compte, la correction consiste à tourner `diag(σ_rp², σ_rp², σ_lacet²)` du monde vers le capteur, pas à bricoler les écarts types.

**Gyromètre et accéléromètre.** 📐 Valeurs par défaut `0,01 rad/s` et `0,1 m/s²` — des ordres de grandeur, pas des spécifications. **Elles doivent être remplacées par la mesure du banc** (§AG, point 6). Une covariance inventée qui traîne trois mois est une dette qui se paie au moment où l'EKF se met à dériver sans raison apparente.

**Donnée absente.** ✅ REP-145 : premier élément de la matrice à `-1`. C'est la seule manière normalisée de dire « non fournie » ; laisser des zéros dirait « parfaitement connue ».

## AF.6 Les autres garanties de `/imu/data`

| Garantie | Pourquoi elle compte |
|---|---|
| **Quaternion normalisé** | La quantification Q14 laisse `‖q‖` s'écarter de 1 de quelques 1e-4. Un quaternion non unitaire est refusé par certains consommateurs et dégrade les autres en silence. La norme d'entrée est publiée dans `/retriever/imu_status` : au-delà de 1e-2, c'est une corruption. |
| **Accélération gravité comprise** | ✅ REP-145 : +g au repos, axe z vers le haut. Le firmware active `SH2_ACCELEROMETER` et non `SH2_LINEAR_ACCELERATION`. Se tromper ici est invisible jusqu'à ce que l'EKF dérive. |
| **Magnétomètre en teslas** | Le BNO085 donne des µT, `sensor_msgs/MagneticField` attend des T. Facteur 1e-6. L'oubli passe inaperçu tant qu'on ne regarde que la direction. |
| **QoS capteur** | `BEST_EFFORT`, profondeur faible. À 100 Hz, retransmettre un échantillon périmé n'a aucun intérêt, et sur Wi-Fi c'est nuisible (§I.4). |

## AF.7 Le choix de la fusion d'orientation

Trois modes, un seul choix par défaut, et la raison du changement à venir.

| Mode | Cap absolu | Immunité magnétique | Quand |
|---|:-:|:-:|---|
| `ROTATION_VECTOR` (9 axes) | ✅ | ❌ | **Défaut du banc.** C'est le seul mode qui permet de *juger* la qualité magnétique du site. |
| `GAME_ROTATION_VECTOR` (6 axes) | ❌ dérive en lacet | ✅ totale | Probablement le bon choix une fois l'IMU montée : le §N.3 désigne le magnétomètre comme le maillon faible avec quatre moteurs-roues à proximité. Le cap viendra alors du GPS et de l'odométrie. |
| `ARVR_STABILIZED_RV` | ✅ lissé | ❌ | Corrections de cap étalées au lieu d'être appliquées d'un coup. ⚠️ Un saut de cap est très mal digéré par un EKF. |

Le choix est une option Kconfig. 🔴 **La décision finale n'est pas prise et ne peut pas l'être avant la mesure** : publier `/imu/mag` sert précisément à la trancher, en observant de combien la norme du champ s'écarte du champ terrestre local une fois le capteur à sa place et les moteurs alimentés.

## AF.8 Foxglove

`foxglove_bridge` est packagé pour Jazzy ✅ et démarré par `bench_imu.launch.py` (`foxglove:=false` pour s'en passer).

⚠️ **Foxglove n'affiche pas `sensor_msgs/Imu` en 3D.** Il affiche des repères TF et des marqueurs. Pour voir l'orientation bouger, il faut donc lui donner l'un des deux. Le banc fait les deux :

| Panneau | Topic | Ce qu'on y regarde |
|---|---|---|
| **3D** | `/tf` + `/retriever/imu_marker` | Un pavé aux dimensions de la carte, orienté comme le capteur, dans le repère fixe `imu_world`. |
| **Plot** | `/imu/data` | `angular_velocity.z` pour voir le lacet, `linear_acceleration.z` pour voir la gravité. |
| **Raw Messages** | `/retriever/imu_status`, `/retriever/link_status` | Les covariances, les compteurs d'erreur, l'aller-retour. |
| **Diagnostics** | `/diagnostics` | Les trois vérifications du pont, en clair. |

⚠️ **`bench.publish_tf` est à désactiver dès que l'EKF entre en service.** C'est lui qui publie `odom → base_link` ; laisser le pont publier une deuxième transformation depuis l'IMU donnerait deux sources sur la même arête de l'arbre TF — la faute décrite au §I.3. Le nœud émet un avertissement au démarrage quand l'option est active, précisément pour qu'on ne l'oublie pas.

---

# AG. Recette du banc B1

**Critère de sortie** : `/imu/data` à 100 Hz dans Foxglove, covariances renseignées, zéro erreur de liaison sur dix minutes, et l'orientation du pavé 3D suit la carte quand on la tourne à la main.

**Dans l'ordre. Chaque point ne se fait que si le précédent est passé.**

### 1 — Le pontet PS0/PS1 ⚠️ à faire avant de brancher quoi que ce soit

Sur les cartes Adafruit et SparkFun, `PS0` et `PS1` sont strappés pour l'I²C. ⚠️ Tant que le pontet n'est pas modifié, **le composant démarre en I²C et le SPI ne répondra jamais** — sans message d'erreur, parce qu'il n'y a personne pour en émettre un.

`PS1` à 3,3 V, `PS0` au GPIO26, `BOOTN` à 3,3 V par 10 kΩ. Câblage complet dans `board_config.h`.

| Critère | ✅ |
|---|---|
| Pontet I²C ouvert, PS1 haut, PS0 sur GPIO | vérifié au multimètre, carte hors tension |

### 2 — Le protocole est cohérent avant de flasher

```bash
python3 tools/check_protocol_sync.py
make -C firmware/test
```

| Critère | ✅ |
|---|---|
| Trois implémentations d'accord, tests hôte au vert | 0 échec |

### 3 — Le nœud parle

```bash
git submodule update --init --recursive
cd firmware/esp32_safety && idf.py set-target esp32 && idf.py build flash
python3 tools/link_monitor.py --device /dev/ttyUSB0
```

| Critère | ✅ | Si ça échoue |
|---|---|---|
| `HEARTBEAT_SAFETY` à 10 Hz | oui | mauvais port, mauvais débit, ou nœud qui ne démarre pas |
| `crc_errors` et `format_errors` | stables après le démarrage | une croissance continue = débit ou câblage |
| Hash annoncé par le nœud | = celui de `generate.py --hash` | firmware pas à jour |

⚠️ Une bordée d'erreurs **au démarrage** est normale : ce sont les messages du bootloader ROM à 115 200. Ce qui compte est qu'elles cessent.

### 4 — Le capteur répond

Dans les journaux tunnellisés que `link_monitor.py` affiche :

```
[esp32 INFO] bno08x present : firmware x.y.z
```

| Critère | ✅ | Si ça échoue |
|---|---|---|
| `sh2_getProdIds()` répond | oui | dans l'ordre : pontet PS0/PS1, mode SPI 3, `H_INTN` câblé, `BOOTN` haut, horloge ≤ 3 MHz |
| `IMU_QUAT`, `IMU_GYRO`, `IMU_ACCEL` | 100 Hz chacune | si 0 Hz mais le produit répond : rapports refusés, lire le code de retour |
| `reset_count` dans `IMU_STATUS` | **0** | tout autre valeur = alimentation instable ou câblage SPI |

### 5 — La latence, mesurée et non devinée

`link_monitor.py` affiche la médiane de l'aller-retour et la valeur à reporter :

```
  aller-retour  médiane 2.84 ms   max 5.10 ms   →  latency_offset_ms ≈ 1.42
```

| Critère | ✅ |
|---|---|
| `link.latency_offset_ms` reporté dans `link_bench.yaml` | fait, et la valeur mesurée notée dans `docs/measurements/` |

### 6 — Le bruit, mesuré et non deviné

Capteur **immobile**, sur une table stable, 60 secondes :

```bash
ros2 launch retriever_bringup bench_imu.launch.py
ros2 bag record -o /tmp/imu_repos /imu/data          # laisser tourner 60 s
```

Écart type de chaque axe → `imu.angular_velocity_stddev` et `imu.linear_acceleration_stddev`.

| Critère | ✅ | Seuil |
|---|---|---|
| Biais gyro au repos | < 0,02 rad/s | §K.2 [P4] |
| Norme de l'accélération au repos | 9,81 ± 0,3 m/s² | §K.2 [P4] |
| Norme du quaternion | 1,00 ± 0,01 | §K.2 [P4] |
| Valeurs mesurées reportées dans `link_bench.yaml` | fait | remplacent les 📐 |

### 7 — Foxglove

```bash
ros2 launch retriever_bringup bench_imu.launch.py
```

Se connecter à `ws://<adresse>:8765`, monter les quatre panneaux du §AF.8.

| Critère | ✅ |
|---|---|
| Le pavé 3D suit la carte quand on la tourne à la main | oui |
| Les trois diagnostics du pont sont `OK` | oui |
| `/imu/data` à 100 Hz dans le panneau Plot | oui |

### 8 — Endurance

Dix minutes, capteur immobile, tout en marche.

| Critère | ✅ |
|---|---|
| `crc_errors`, `format_errors`, `overflows` | **inchangés** sur les dix minutes |
| `samples_dropped_node`, `samples_dropped_link` | 0 |
| `sensor_resets` | 0 |
| Cadence de `/imu/data` | 100 Hz ± 2 % |

### 9 — Le magnétomètre, pour préparer la décision du §AF.7

Capteur immobile, loin de tout métal, puis à 10 cm d'un moteur-roue non alimenté, puis alimenté.

| Mesure | À noter dans `docs/measurements/` |
|---|---|
| Norme de `/imu/mag` dans chacune des trois situations | Le champ terrestre local vaut 25 à 65 µT. L'écart dit si le cap magnétique restera exploitable — c'est ce qui tranchera entre `ROTATION_VECTOR` et `GAME_ROTATION_VECTOR`. |

---

# AH. Ce que ce document change dans le dossier

## AH.1 Corrections à reporter

| Section | Texte actuel | Correction |
|---|---|---|
| §P.1 | `firmware/*/platformio.ini` | ESP-IDF, pas PlatformIO. Arborescence du §AE.2. |
| §P.1 | `retriever_can/`, `retriever_can_bridge/` | `retriever_protocol/`, `retriever_link/` — le transport est abstrait, le nom ne doit pas dire le contraire. |
| §F.3 | `HEARTBEAT` DLC 4 | DLC 8 — le hash du protocole doit y tenir (§AD.2). |
| §F.3 | `IMU_GYRO_ACC` en deux trames | `0x211` gyro, `0x212` accel, plus `0x213` statut et `0x214` magnétomètre. |
| §F.4 | Régression linéaire par nœud | Non applicable tant qu'aucune trame ne porte d'horodatage. Voir §AF.4. |
| §I.1 | `retriever_can_bridge` | `retriever_link_bridge`. |
| §G.3 | `H_INTN` sur GPIO34 | Sur le banc, GPIO25 : tirage interne disponible. GPIO34 redevient correct sur la carte, où le tirage externe sera au schéma. |

## AH.2 Ce qui est confirmé sans changement

- Le CAN reste le bus temps réel. La série est un transport de développement, et la §AB.5 énumère tout ce qu'elle ne fournit pas.
- La séparation d'autorité du §G.1 est intacte : le banc ne donne aucune autorité au nœud SAFETY, il ne fait que produire des données.
- Les niveaux N4 et N5 restent matériels et hors du périmètre logiciel.
- Le BNO085 reste sur l'ESP32 en SPI, pour l'horodatage matériel (§G.2).
- `sensor_msgs/Imu` sur `/imu/data` : le §I.2 l'avait déjà retenu, y compris le passage du mode SPI au repli UART-RVC par simple changement de fichier de lancement.

## AH.3 Mesures nouvelles

| # | Mesure | Bloque | Où |
|---|---|---|---|
| **L1** | Aller-retour applicatif de la liaison série | `link.latency_offset_ms`, donc l'horodatage de tous les capteurs | §AG.5 |
| **L2** | Écart type du gyromètre et de l'accéléromètre au repos | Les covariances de `/imu/data`, donc le réglage de l'EKF | §AG.6 |
| **L3** | Norme du champ magnétique à trois distances d'un moteur | Le choix entre `ROTATION_VECTOR` et `GAME_ROTATION_VECTOR` | §AG.9 |

## AH.4 La suite, et pourquoi dans cet ordre

```
   B1  IMU → ROS → Foxglove                         ◄── ce document
        │  ce que ça prouve : le protocole, le cadrage, la génération,
        │  les conventions de repères, la chaîne de covariances
        ▼
   B2  Deuxième capteur sur le même socle
        │  GPS, ou températures. Coût attendu : une trame dans le YAML,
        │  une tâche, un publisher. Si c'est plus cher que ça, B1 a raté.
        ▼
   B3  ESP32-MOTION : décodage Hall, PCNT, retour de vitesse
        │  toujours en série, roues levées, aucun variateur alimenté
        ▼
   B4  ⚠️ Passage au CAN — dès que safety_bus_distribution existe
        │  §Q niveau 2 EN ENTIER. Rien de la chaîne de sécurité
        │  n'a été validé avant ce point, et rien ne peut l'être avant.
        ▼
   B5  FSM de sécurité, précharge, contacteur
```

⚠️ **B4 ne se place pas « quand la série marchera bien ».** Il se place quand la carte existe. La qualité du banc série ne dit rien sur la chaîne de sécurité, qui n'a pas commencé.

---

*Suite du dossier : `00-index-A-B.md` pour l'index général. Table des trames générée : `generated/protocol-tables.md`.*
