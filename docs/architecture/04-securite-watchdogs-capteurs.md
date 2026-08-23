# L. Architecture de sécurité

## L.1 Les quatre sécurités, séparées

Le cahier des charges demande explicitement cette séparation. Elle est utile parce que **chaque catégorie a un mode de défaillance et un moyen de vérification différents**.

```
┌─────────────────────────────────────────────────────────────────────────┐
│ SÉCURITÉ FONCTIONNELLE — empêcher le logiciel de commander du dangereux │
│   Autorité, validation des commandes, saturation, budget de courant     │
│   Vérification : tests d'injection de commandes aberrantes (§Q)         │
├─────────────────────────────────────────────────────────────────────────┤
│ SÉCURITÉ ÉLECTRIQUE — protéger les personnes et le matériel             │
│   Fusibles, sectionnement, sections de câble, busbars, isolation        │
│   Vérification : mesure de résistance de jonction, thermographie        │
├─────────────────────────────────────────────────────────────────────────┤
│ SÉCURITÉ MÉCANIQUE — arrêt physique, protections, fixations             │
│   Champignon, contacteur, capotage, couples de serrage, freinage        │
│   Vérification : essais physiques, contrôle au couple, inspection       │
├─────────────────────────────────────────────────────────────────────────┤
│ SÉCURITÉ LOGICIELLE — watchdogs, timeouts, limites, états, permissions  │
│   FSM, watchdogs multi-niveaux, QoS, authentification réseau            │
│   Vérification : tests automatisés d'injection de panne (§Q)            │
└─────────────────────────────────────────────────────────────────────────┘
```

## L.2 Sécurité fonctionnelle

### Le principe d'autorité

| Fonction | Qui **propose** | Qui **autorise** | Qui **exécute** |
|---|---|---|---|
| Vitesse de consigne | Nav2 / téléop | ESP32-MOTION (sature) | ZS-X11H |
| Armement | X1 | **ESP32-SAFETY** | contacteur + `/SAFE` |
| Mise sous puissance | X1 ou bouton | **ESP32-SAFETY** | contacteur |
| Arrêt d'urgence | n'importe qui | **personne** (inconditionnel) | boucle matérielle |
| Budget de courant | — | **ESP32-SAFETY** | saturation des consignes |

### Validation des commandes — les quatre filtres successifs

```
   Nav2 émet cmd_vel
        │
   ①  velocity_smoother      limites d'accélération/jerk ROS
        │
   ②  collision_monitor      zones VelocityPolygon, source_timeout 2 s
        │
   ③  twist_mux              priorité : e-stop(255) > téléop(100) > nav(10)
        │
   ④  diff_drive_controller  limites de vitesse/accél/décél, cmd_vel_timeout 0,5 s
        │
   ⑤  retriever_hardware     plausibilité, encodage, CRC8, numéro de séquence
        │  ═══ CAN ═══
   ⑥  ESP32-MOTION           limites EN DUR dans le firmware :
        │                      v_max = 2,0 rad/s (constexpr)
        │                      a_max = 3,0 rad/s² (constexpr)
        │                      rejet si seq non incrémental ou CRC faux
        │                      rejet si mode BANC et v > 0,3 rad/s
        │
   ⑦  ESP32-SAFETY           budget de courant global → réduction homothétique
        │                      des 4 consignes si I > 22 A
        ▼
      ZS-X11H
```

⚠️ **Les limites du niveau ⑥ sont des `constexpr` compilées, pas des paramètres.** Une valeur reçue par CAN (`CONFIG`, 0x320) ne peut que **réduire** une limite, jamais l'augmenter. C'est la différence entre une limite et une suggestion.

⚠️ **Le numéro de séquence et le CRC8 dans les trames de commande** ne sont pas de la paranoïa : ils détectent une trame rejouée (un buffer CAN bloqué qui réémet la dernière commande) et une trame corrompue par du bruit. Sans eux, un bus CAN perturbé peut produire une commande de vitesse plausible mais fausse.

## L.3 Sécurité électrique

Récapitulatif des mesures décrites en §D et §E, avec la question « qu'est-ce que ça protège ? » :

| Protection | Contre quoi | Où | Efficacité |
|---|---|---|---|
| Fusible MRBF 80 A / 58 V | Court-circuit franc du bus | Borne + du pack, ≤175 mm | AIC 2000 A @58 V > I_cc estimé |
| 4 × MIDI 25 A | Court-circuit d'un variateur | Busbar + | Isole le variateur, pas le robot |
| MIDI 10 A / 5 A | Défaut d'un DC/DC | Busbar + | idem |
| Coupe-batterie DC 48 V | Intervention, stockage, urgence prolongée | Extérieur, accessible sans outil | Sectionnement visible |
| Précharge 10 Ω + timeout | Appel de courant (kA), soudure des contacts, fatigue du fusible | En parallèle du contacteur | **Détecte aussi un court-circuit avant mise sous puissance** |
| TVS 45–48 V sur bus et par variateur | Transitoires de commutation | Busbar + et entrée variateur | Écrête les **grands** transitoires ⚠️ pas les surtensions modérées (§D.8) |
| Connecteurs détrompés + code couleur | Inversion de polarité | Toute liaison de puissance | **La meilleure protection : rendre l'erreur impossible** |
| Rondelles Belleville + recouple | Fluage du cuivre → jonction résistive → point chaud | Chaque boulon de busbar | **Le mode de panne n°1** |
| Étamage | Oxydation du cuivre (l'oxyde est isolant) | Busbars, cosses | |
| Câble souple classe 5 étamé 105 °C | Fatigue par vibration, oxydation | Tout le câblage | |
| Capotage transparent des busbars | Contact accidentel, chute d'outil | Zone puissance | |
| Superposition (et non juxtaposition) des barres + / − | Pontage par corps étranger | Zone puissance | |
| Voyant « BUS SOUS TENSION » | Intervention sur un bus encore chargé | Façade | 10 mF mettent ~50 s à se décharger |

⚠️ **Rappel du contexte de risque** : à 42 V DC, on est **sous la limite TBTS/SELV** (120 V DC lisse, IEC 60364-4-41). Le danger n'est **pas** l'électrisation. Le danger est **thermique et incendiaire** :

- **1 mΩ = 10 W à 100 A** (0,6 W à 25 A). Une jonction dégradée chauffe, s'oxyde davantage, chauffe plus : c'est un emballement lent.
- **Un arc de 3–5 mm s'entretient indéfiniment à 42 V** (modèle Stokes-Oppenlander : V_arc ≈ 37 V à 3 mm sous 100 A, donc < 42 V). C'est exactement l'écart d'un boulon desserré ou d'un connecteur qui vibre. C'est un chalumeau de plusieurs kW localisé.
- **Une cellule Li-ion en emballement thermique** ne s'éteint pas à l'eau et dégage des gaz toxiques et inflammables.

**Bonne nouvelle contre-intuitive** : au-delà de ~8–10 mm de séparation, l'arc s'auto-éteint à 42 V. Ce n'est pas du 400 V photovoltaïque. La conception doit donc viser **les jonctions et les petites distances**, pas les grandes.

**Équipement de sécurité à avoir sur l'établi** :
- Extincteur adapté aux feux de métaux/lithium (classe D) ou, à défaut, un grand seau de sable sec. ⚠️ Un extincteur à eau ou à CO₂ n'éteint pas un emballement lithium.
- Bac métallique ou sac LiPo pour stocker le pack hors utilisation.
- Caméra thermique ou thermomètre infrarouge (30–60 €) — indispensable pour le contrôle des jonctions.
- Lunettes de protection lors de toute manœuvre sur le bus chargé.

## L.4 Sécurité mécanique

| Élément | Spécification |
|---|---|
| **Champignon d'arrêt d'urgence** | À accrochage mécanique (déverrouillage par rotation), contact **NF**, ⚠️ **NF obligatoire** : un contact NO ne détecte pas la rupture de fil. Diamètre ≥ 40 mm, rouge sur fond jaune. **Accessible depuis tous les côtés du robot**, ou au minimum depuis l'arrière et un côté. Non masqué par un capot. |
| **Coupe-batterie** | Rotatif à clé, extérieur, sans outil, signalisation permanente |
| **Capotage des busbars** | Polycarbonate transparent vissé — transparent pour permettre l'inspection visuelle sans démontage |
| **Fixation du pack** | ⚠️ Le pack doit être **maintenu mécaniquement**, pas posé. Sangles + berceau, avec un matériau amortissant (mousse EVA). Un pack qui se déplace arrache ses câbles. |
| **Isolation thermique du pack** | Éloigné des variateurs et des DC/DC. Un pack lithium ne doit pas dépasser 45 °C. |
| **Protection des pièces mobiles** | Carters sur les zones de pincement roue/châssis |
| **Reprise mécanique des câbles** | ⚠️ Tout connecteur doit avoir un collier de reprise à moins de 50 mm : **la traction ne doit jamais être reprise par les contacts** |
| **Rayon de courbure des câbles** | ≥ 6 × Ø extérieur pour les câbles de puissance |
| **Passage de cloison** | Presse-étoupe ou passe-fil caoutchouc — jamais de câble sur une tôle nue |
| **Couples de serrage** | Documentés, appliqués à la clé dynamométrique, marqués d'un trait témoin |
| **Contrôle périodique** | Couples, thermographie, inspection visuelle — planning en §R.7 |

⚠️ **Évolution recommandée en phase 4 : frein mécanique à manque de courant.** C'est la seule solution qui tient une pente avec le robot totalement hors tension. Le freinage électrique du ZS-X11H disparaît dès que le contacteur s'ouvre.

## L.5 Sécurité logicielle

| Mécanisme | Où | Détail |
|---|---|---|
| Watchdog matériel externe | ESP32-SAFETY | TPL5010 ou équivalent — reset + `/SAFE` si l'ESP32 ne rafraîchit pas |
| Watchdog interne | 3 × ESP32 | Task WDT FreeRTOS sur la tâche CONTROL |
| Timeout de commande | ESP32-MOTION | 150 ms sans trame `CMD_WHEELS` → VR=0 + frein |
| Timeout de heartbeat | ESP32-SAFETY | 300 ms sans heartbeat d'un nœud → `BRAKING` |
| Heartbeat matériel X1 | GPIO dédié | Créneau 10 Hz — indépendant du bus CAN |
| `cmd_vel_timeout` | `diff_drive_controller` | 0,5 s (défaut Jazzy ✅) |
| `source_timeout` | `nav2_collision_monitor` | 2,0 s — arrêt si les données capteur cessent ✅ |
| Bond | `nav2_lifecycle_manager` | Détecte la mort d'un nœud managé, fait transiter tout le groupe |
| Limites en dur | Firmware ESP32 | `constexpr`, non modifiables par CAN à la hausse |
| CRC8 + numéro de séquence | Trames de commande CAN | Détecte corruption et rejeu |
| Hash de protocole | Heartbeat | Détecte un firmware pas à jour |
| Permissions réseau | §O | Aucune commande sans VPN authentifié |
| `enable_odom_tf: false` | `diff_drive_controller` | Évite deux publieurs sur la même arête TF |

## L.6 Ce qui doit fonctionner **même si le PC principal plante**

C'est la question centrale du cahier des charges. Réponse exhaustive :

| Fonction | Fonctionne sans X1 ? | Comment |
|---|---|---|
| **Arrêt d'urgence par champignon** | ✅ **Oui, totalement** | Boucle matérielle pure, aucun composant actif |
| **Coupure de la puissance** | ✅ Oui | Contacteur NO, bobine en série avec le champignon |
| **Freinage électrique des moteurs** | ✅ Oui | Ligne `/SAFE` → entrées `EL`/`STOP` des variateurs |
| **Détection de la perte du X1** | ✅ Oui | Heartbeat matériel GPIO (10 Hz) + heartbeat CAN |
| **Arrêt automatique sur perte du X1** | ✅ Oui | ESP32-SAFETY → `BRAKING` → `FAULT` en 300 ms |
| **Limitation du courant batterie** | ✅ Oui | ESP32-SAFETY + ACS758, boucle 500 Hz |
| **Protection contre le calage moteur** | ✅ Oui | ESP32-MOTION, détection Hall |
| **Refroidissement** | ✅ Oui | ESP32-SAFETY pilote les ventilateurs sur température |
| **Surveillance de la batterie** | ✅ Oui | ESP32-SAFETY ↔ BMS en UART |
| **Détection de surchauffe** | ✅ Oui | NTC lus par les ESP32 |
| **Sécurité en cas de basculement** | ✅ Oui | IMU BNO085 lue par l'ESP32-SAFETY |
| **Signalisation d'état** | ✅ Oui | LED RGB + buzzer pilotés par l'ESP32-SAFETY |
| Navigation, perception, mission | ❌ Non | Et c'est **normal** : ces fonctions doivent **cesser**, pas continuer |
| Téléopération | ❌ Non | idem |
| Journalisation détaillée | 🟡 Partielle | L'ESP32-SAFETY conserve les 32 derniers événements en RAM et les rejoue au retour du X1 |

**Test de validation obligatoire (§Q, niveau 6)** : robot en mouvement à 1 m/s, roues levées puis au sol, exécuter `sudo systemctl stop retriever-bringup` puis, dans un second essai, **couper brutalement l'alimentation du X1**. Dans les deux cas : arrêt en moins de 500 ms, état `FAULT` avec la cause correcte, journal cohérent.

## L.7 Signalisation

Un robot autonome doit dire ce qu'il fait. C'est de la sécurité, pas du confort.

| État | LED RGB (façade) | Buzzer |
|---|---|---|
| `INIT` | Blanc fixe | — |
| `SAFE` | Vert clignotant lent (0,5 Hz) | — |
| `PRECHARGE` | Jaune clignotant rapide | — |
| `LIVE_DISARMED` | Bleu fixe | 1 bip court à l'entrée |
| `LIVE_ARMED` | **Orange clignotant (2 Hz)** | **2 bips à l'armement** |
| `ARMED` + mouvement | **Orange clignotant rapide (4 Hz)** | **Bip intermittent continu** ⚠️ |
| `DEGRADED` | Orange/rouge alterné | 3 bips à l'entrée |
| `FAULT` | Rouge fixe | 5 bips |
| `ESTOP` | **Rouge clignotant 5 Hz** | **Continu 2 s** |
| Bus sous tension | **LED rouge dédiée**, alimentée par le bus lui-même | — |

⚠️ **Le bip intermittent pendant le mouvement autonome n'est pas négociable.** Un robot de 35 kg qui démarre silencieusement derrière quelqu'un est un danger. La LED rouge « bus sous tension », alimentée directement par le bus (LED + 10 kΩ, sans microcontrôleur), est le seul indicateur fiable qu'on peut ou non toucher la zone de puissance.

---

# M. Watchdogs et gestion des pannes

## M.1 Architecture des watchdogs

```
┌─────────────────────────────────────────────────────────────────────────┐
│ NIVEAU 0 — WATCHDOG MATÉRIEL                                            │
│   TPL5010 externe → ESP32-SAFETY                                        │
│   Période 1 s. Pas de rafraîchissement → RESET de l'ESP32 + /SAFE actif │
│   ★ Le seul watchdog qui ne dépend d'aucun logiciel                     │
├─────────────────────────────────────────────────────────────────────────┤
│ NIVEAU 1 — WATCHDOG FIRMWARE                                            │
│   Task WDT FreeRTOS sur la tâche CONTROL de chaque ESP32                │
│   Période 50 ms → reset du nœud, état sûr au redémarrage                │
├─────────────────────────────────────────────────────────────────────────┤
│ NIVEAU 2 — WATCHDOG DE BUS (croisé)                                     │
│   ESP32-SAFETY surveille les heartbeats des 2 MOTION      (300 ms)      │
│   ESP32-MOTION surveille les trames CMD du X1             (150 ms)      │
│   ESP32-SAFETY surveille le heartbeat matériel du X1      (300 ms)      │
│   ESP32-MOTION surveille SAFETY_STATE                     (300 ms)      │
├─────────────────────────────────────────────────────────────────────────┤
│ NIVEAU 3 — WATCHDOG ROS 2                                               │
│   retriever_hardware : timeout de retour CAN → read() ERROR   (150 ms)  │
│   diff_drive_controller : cmd_vel_timeout                 (500 ms)      │
│   nav2_lifecycle_manager : bond sur chaque nœud managé                  │
│   diagnostic_updater : DiagnosedPublisher sur chaque topic critique     │
├─────────────────────────────────────────────────────────────────────────┤
│ NIVEAU 4 — WATCHDOG DE MISSION                                          │
│   mission_manager : durée max, progression min, distance max            │
│   systemd : Restart=on-failure + WatchdogSec sur retriever-bringup      │
└─────────────────────────────────────────────────────────────────────────┘
```

⚠️ **Pourquoi il n'y a PAS de « nœud watchdog » ROS 2** : un watchdog qui s'exécute dans le même processus, le même OS et sur le même CPU que ce qu'il surveille ne surveille rien. Si le noyau Linux se fige, le nœud watchdog se fige avec. Les watchdogs qui comptent sont ceux du niveau 0 à 2 — matériel et firmware.

## M.2 Table complète des pannes

| # | Panne | Détection | Délai | Réaction | Niveau | Récupération |
|---|---|---|---|---|---|---|
| 1 | **ROS 2 plante** (nœud tué, OOM, exception) | Perte du heartbeat matériel X1 + perte des trames CMD | 150–300 ms | ESP32-MOTION : VR=0 + frein · ESP32-SAFETY : `BRAKING` → `FAULT`, contacteur ouvert | **FAULT** | `systemd Restart=on-failure` relance ; auto-test complet ; acquittement opérateur requis |
| 2 | **Le X1 se fige** (kernel panic, gel matériel) | Idem — le heartbeat GPIO s'arrête même si l'USB reste énuméré ★ | 300 ms | Idem | **FAULT** | Manuelle (redémarrage) |
| 3 | **Le X1 perd son alimentation** | Idem, immédiat | < 150 ms | Idem | **FAULT** | Diagnostic du DC/DC COMPUTE |
| 4 | **Un ESP32-MOTION plante** | Perte de son heartbeat CAN | 300 ms | ESP32-SAFETY : `BRAKING` → `FAULT` | **FAULT** | Reset automatique par WDT interne ; si 3 resets en 60 s → `FAULT` définitif |
| 5 | **L'ESP32-SAFETY plante** | Watchdog externe TPL5010 | 1 s | Reset + `/SAFE` actif pendant le reset · les MOTION perdent `SAFETY_STATE` → frein | **FAULT** | Reset auto ; état sûr garanti pendant le reset |
| 6 | **Bus CAN en `bus-off`** | Compteur d'erreurs CAN | immédiat | Chaque nœud passe en état sûr indépendamment | **FAULT** | `restart-ms 100` (auto-recovery SocketCAN) ; si > 3 en 60 s → `FAULT` définitif |
| 7 | **Adaptateur USB-CAN débranché** | `can0` DOWN + perte des trames | 150 ms | Comme panne 1 | **FAULT** | Réénumération USB ; udev rule pour un nom stable |
| 8 | **Réseau Wi-Fi perdu** | Perte du keepalive WireGuard | 5 s | ⚠️ **Décision explicite** : mission autonome → **continue** · mission téléopérée → `STOPPING` immédiat | **DEGRADED** ou **STOPPING** | Reconnexion automatique |
| 9 | **Lidar disparaît** | `DiagnosedPublisher` sur `/scan` + `source_timeout` du collision monitor | 2 s | Collision monitor → action `stop`. Nav2 s'arrête. | **DEGRADED** | Relance du driver (3 essais) ; en extérieur, la mission GPS peut continuer sans lidar avec vitesse plafonnée à 0,4 m/s 📐 |
| 10 | **Kinect disparaît** | `DiagnosedPublisher` | 3 s | Aucun impact sur la sécurité | **WARN** | Relance du driver ; jamais bloquant |
| 11 | **GPS disparaît / perd le fix** | `status == NO_FIX` ou pas de message | 5 s / 60 s | `ekf_map` perd sa correction ; `ekf_odom` continue. Mission par waypoints GPS → abandon | **DEGRADED** | Attente de reprise du fix, 120 s max, puis retour en `READY` |
| 12 | **IMU disparaît** | Pas de `/imu/data` depuis 100 ms | 100 ms | ⚠️ Les deux EKF perdent leur meilleure source d'orientation. Vitesse plafonnée à 0,3 m/s, mission abandonnée après 5 s | **DEGRADED** → **FAULT** | Reset SPI du BNO085 (3 essais) |
| 13 | **Un variateur ne répond plus** | Consigne appliquée, aucune réaction en vitesse | 500 ms | Roue désactivée. 2 roues du même côté → `FAULT` | **DEGRADED** / **FAULT** | Reset de la roue (assertion `STOP` puis relâchement), 1 essai |
| 14 | **Un moteur reste bloqué** | Consigne ≠ 0 et comptes Hall = 0 | 300 ms | Consigne de cette roue à 0, alerte. Si le blocage persiste avec les autres roues qui tournent → risque de traînage → `STOPPING` | **DEGRADED** | Tentative de déblocage : consigne inverse 0,2 rad/s pendant 500 ms, 2 essais |
| 15 | **Capteur Hall défaillant** | Séquence d'états invalide | 5 occurrences / 1 s | Roue désactivée (odométrie fausse = pire que pas d'odométrie) | **DEGRADED** | Aucune (panne matérielle) |
| 16 | **Surintensité progressive** | ACS758 > 22 A | continu | Réduction homothétique des 4 consignes | **WARN** | Automatique |
| 17 | **Surintensité franche** | ACS758 > 35 A | < 20 ms | `/SAFE` + contacteur | **FAULT** | Acquittement + analyse obligatoire |
| 18 | **Sur-régénération** | ACS758 < −15 A | 100 ms | Bridage du taux de freinage | **WARN** | Automatique |
| 19 | **Tension bus basse** | V_bus < 29 V | 100 ms | `BRAKING` → `FAULT` | **FAULT** | Recharge de la batterie |
| 20 | **Tension bus haute** | V_bus > 43 V | 200 ms | Bridage du freinage, puis `FAULT` | **WARN** → **FAULT** | Analyse (régénération ? chargeur branché ?) |
| 21 | **Batterie faible** | SOC < 20 % | 2 s | `mission_manager` déclenche le retour au point de départ | **WARN** | Recharge |
| 22 | **Batterie critique** | SOC < 10 % ou V_pack < 31 V | 2 s | `STOPPING` puis désarmement | **DEGRADED** | Recharge |
| 23 | **Déséquilibre de cellules** | Δ > 150 mV | 5 s | `WARN` · Δ > 400 mV → refus d'armement | **WARN** / **FAULT** | ⚠️ Équilibrage ou remplacement du pack |
| 24 | **Surchauffe variateur** | NTC > 70 °C | 1 s | Ventilateurs à 100 %, bridage 50 % · > 85 °C → `FAULT` | **WARN** → **FAULT** | Refroidissement, analyse |
| 25 | **Surchauffe CPU** | > 85 °C | 5 s | Ventilateurs à 100 %, réduction de la fréquence de Nav2 | **WARN** | Automatique |
| 26 | **Ventilateur en panne** | Tachy à 0 alors que PWM > 20 % | 3 s | `WARN` + abaissement des seuils thermiques de 10 °C | **WARN** | Remplacement |
| 27 | **Basculement / inclinaison** | IMU : angle > 40° 📐 | 500 ms | `BRAKING` → `FAULT` | **FAULT** | Redressement manuel |
| 28 | **Câble débranché (puissance)** | Chute de V_bus, ou courant nul sur une branche | 100 ms | Selon la branche : `DEGRADED` ou `FAULT` | variable | Inspection |
| 29 | **Faux contact / jonction résistive** | ⚠️ **Difficile à détecter en direct.** Signature : chute de V_bus corrélée au courant, oscillations, échauffement local | lent | Alerte si la résistance apparente du bus (ΔV/ΔI) augmente de > 30 % vs la référence | **WARN** | ⚠️ **Thermographie obligatoire.** C'est le mode de panne le plus dangereux |
| 30 | **Divergence de version de protocole** | Hash dans le heartbeat | au boot | Refus d'armement | **FAULT** | Reflasher le nœud |
| 31 | **Contacteur collé** | V_bus reste haute alors que la commande est ouverte | 500 ms | ⚠️ **Alerte maximale.** Buzzer continu, consigne d'ouvrir le coupe-batterie | **CRITICAL** | Remplacement du contacteur |
| 32 | **Champignon défaillant** (contact oxydé) | Test actif [P6] au boot | au boot | Refus d'armement | **FAULT** | Remplacement |
| 33 | **Disque plein** | `diagnostic_common_diagnostics` | 30 s | Arrêt de l'enregistrement des bags, purge des plus anciens | **WARN** | Automatique + alerte |
| 34 | **Dérive d'horloge** | Comparaison `TIME_SYNC` / horloge locale | 60 s | Recalage de la régression linéaire ; si dérive > 100 ms/min → `WARN` | **WARN** | Automatique |

★ **Pourquoi le heartbeat matériel du X1 est distinct du bus CAN** : si le bus CAN tombe mais que Linux tourne, le heartbeat GPIO continue — on sait que c'est un problème de bus, pas de PC. Si Linux se fige, le créneau GPIO s'arrête même si l'adaptateur USB-CAN reste énuméré et que le noyau CAN continue de répondre. **Ce sont deux tests différents, et ils permettent un diagnostic différentiel.**

## M.3 Politique de récupération

| Type de panne | Récupération automatique ? | Tentatives | Après épuisement |
|---|---|---|---|
| Nœud ROS 2 mort | ✅ `systemd Restart=on-failure` | 3 en 5 min | `FAULT` définitif |
| Lifecycle node en erreur | ✅ `cleanup` → `configure` → `activate` | 3 | `DEGRADED` puis `FAULT` |
| `can0` en bus-off | ✅ `restart-ms 100` | 3 en 60 s | `FAULT` définitif |
| Driver capteur muet | ✅ Relance du nœud | 3 | `DEGRADED` |
| ESP32 figé | ✅ WDT interne / externe | 3 en 60 s | `FAULT` définitif |
| Moteur bloqué | ✅ Consigne inverse brève | 2 | Roue désactivée, `DEGRADED` |
| Surintensité | ❌ **Jamais automatique** | — | Acquittement + analyse obligatoires |
| Défaut de sécurité (P6) | ❌ **Jamais** | — | Intervention physique |
| Contacteur collé | ❌ **Jamais** | — | Remplacement |
| Déséquilibre cellules critique | ❌ **Jamais** | — | Intervention sur le pack |

⚠️ **Règle générale** : tout ce qui touche à la **puissance** ou à la **sécurité** exige un acquittement humain. Tout ce qui touche à la **perception** ou au **logiciel applicatif** peut se relancer seul, avec un compteur borné. Un système qui se relance indéfiniment sur un défaut de puissance est un système qui va finir par prendre feu.

## M.4 Dégradations tolérées en mouvement

| Dégradation | Mouvement autorisé ? | Limites imposées |
|---|---|---|
| Kinect absent | ✅ Oui | Aucune |
| GPS sans fix, en intérieur | ✅ Oui | Missions par waypoints GPS interdites |
| Lidar absent, en extérieur | ✅ Oui | v ≤ 0,4 m/s, missions GPS uniquement, collision monitor sur la seule caméra |
| Lidar absent, en intérieur | ❌ Non | Pas de perception d'obstacles |
| Une roue désactivée (Hall HS) | ✅ Oui | v ≤ 0,5 m/s, rotations sur place interdites, retour au point de départ uniquement |
| Deux roues du même côté HS | ❌ Non | `FAULT` |
| BMS muet | ✅ Oui | v ≤ 0,8 m/s, durée de mission ≤ 10 min, SOC estimé par tension |
| Ventilateur HS | ✅ Oui | Seuils thermiques abaissés de 10 °C |
| Δ cellules 150–400 mV | ✅ Oui | Courant plafonné à 12 A, mission courte |
| Réseau absent | ✅ Oui (mission autonome) | Bag complet activé, retour au point de départ à la fin |
| IMU absente | 🟡 Marginal | v ≤ 0,3 m/s, 5 s maximum, uniquement pour dégager le robot |

---

# N. Architecture capteurs et localisation

## N.1 Le problème de fond, posé franchement

L'objectif est la navigation autonome Nav2 en extérieur. Le parc de capteurs disponible est un parc d'intérieur. Ce n'est pas une opinion : c'est ce que disent les datasheets.

| Contrainte | YDLIDAR X4 | Conséquence en extérieur |
|---|---|---|
| Environnement lumineux de spécification | **0 / 550 / 2000 lux** ✅ | Plein soleil = 10 000 à 100 000 lux. Le récepteur sature : portée effondrée, mesures manquantes ou **fausses** |
| Température | **0 – 40 °C** ✅ | Sortie matinale à 3 °C ou journée d'été à 45 °C : hors spécification |
| Indice de protection | **Aucun (non spécifié)** ✅ | Optique tournante à nu : pluie, rosée, poussière, boue |
| Portée spécifiée | 0,12 – 10 m **« en intérieur, réflectivité 80 % »** ✅ | Sur de l'herbe (réflectivité faible, surface diffuse) la portée utile chute |

Et il y a un problème **indépendant du capteur** : un scan 2D à hauteur fixe en extérieur voit de l'herbe, des dénivelés, des trous, des surplombs — et pas de murs. Le scan-matching de `slam_toolbox` a besoin de géométrie **structurée et statique**. En terrain ouvert, le graphe de poses dérive et les fermetures de boucle échouent.

Pour le Kinect v2, c'est la même logique poussée plus loin : c'est un capteur de profondeur par **infrarouge actif (time-of-flight)**. Le soleil est une source IR massive. En extérieur, la carte de profondeur est inexploitable. S'y ajoutent : alimentation secteur 12 V / 2,67 A, USB 3.0 dédié sur un SBC qui n'a que deux ports, pipeline CPU à 200 ms/trame, libfreenect2 sans release depuis 2021, et aucun driver ROS 2 Jazzy.

## N.2 Architecture de localisation retenue

**Deux stratégies selon l'environnement, une seule pile logicielle.**

```
════════ MODE INTÉRIEUR (phases 1–2) ═════════════════════════════════════

   /scan (lidar) ──► slam_toolbox (online_async) ──► /map + TF map→odom
   odométrie roues ─┐
   /imu/data ───────┴──► ekf_odom ──► /odometry/filtered ──► TF odom→base_link

════════ MODE EXTÉRIEUR (phase 3+) ═══════════════════════════════════════

   odométrie roues ─┐
   /imu/data (gyro)─┴──► ekf_odom  ──► /odometry/filtered
                          world_frame: odom  ──► TF odom → base_link
                          two_d_mode: true                      (continu, sans saut)

   /gps/fix ────────┐
   /imu/data ───────┼──► navsat_transform_node ──► /odometry/gps
   /odometry/filtered/global ──┘                   + services fromLL / toLL

   odométrie roues ─┐
   /imu/data ───────┼──► ekf_map ──► /odometry/filtered/global
   /odometry/gps ───┘                world_frame: map ──► TF map → odom
                                     two_d_mode: true       (peut sauter)

   Lidar / caméra ──► pointcloud_to_laserscan + laser_filters
                      ──► local_costmap UNIQUEMENT (obstacles, pas localisation)
                      ──► nav2_collision_monitor
```

**Pourquoi `robot_localization` et pas `fuse`** : il circule que `robot_localization` ne serait plus maintenu. C'est **faux et périmé** — le paquet est en version 3.8.3 sur Jazzy (mise à jour mars 2026), avec **Tom Moore et Steve Macenski** comme mainteneurs. `fuse` (graphe de facteurs, Ceres) est conceptuellement supérieur pour des capteurs à latences hétérogènes, mais sa documentation est clairsemée et **il n'y a pas d'équivalent clé-en-main de `navsat_transform_node`**. Pour un premier robot GPS, le coût d'intégration est disproportionné.

### Paramètres clés et pièges

```yaml
ekf_odom:
  ros__parameters:
    frequency: 30.0
    two_d_mode: true              # ⚠️ OBLIGATOIRE sur un robot terrestre
    publish_tf: true
    world_frame: odom             # → publie odom → base_link
    odom0: /diff_drive_controller/odom
    odom0_config: [false,false,false,   # x,y,z          : NON (on fusionne les vitesses)
                   false,false,false,   # roll,pitch,yaw : NON
                   true, false,false,   # vx,vy,vz       : vx OUI
                   false,false,false,   # vroll,vpitch,vyaw : ⚠️ NON en skid-steer
                   false,false,false]
    imu0: /imu/data
    imu0_config: [false,false,false,
                  false,false,false,    # pas d'orientation absolue dans l'EKF local
                  false,false,false,
                  false,false,true,     # vyaw (gyro) OUI ← la bonne source de rotation
                  false,false,false]
    imu0_differential: false
    imu0_remove_gravitational_acceleration: true

ekf_map:
  ros__parameters:
    two_d_mode: true
    world_frame: map              # → publie map → odom
    odom0: /diff_drive_controller/odom
    odom1: /odometry/gps
    odom1_config: [true,true,false, false,false,false,
                   false,false,false, false,false,false, false,false,false]
    imu0: /imu/data
    imu0_config: [false,false,false,
                  false,false,true,     # yaw absolu OUI, mais SEULEMENT ICI
                  false,false,false,
                  false,false,true,
                  false,false,false]

navsat_transform:
  ros__parameters:
    frequency: 30.0
    magnetic_declination_radians: 0.0   # ⚠️ À RENSEIGNER pour Mérignac (≈ +0,5° E) 📐
    yaw_offset: 0.0                     # 0 si l'IMU suit REP-103 (0 = Est)
    zero_altitude: true
    use_odometry_yaw: true
    publish_filtered_gps: true
    wait_for_datum: false
    broadcast_utm_transform: false
```

**Les six règles à ne pas violer** :

| # | Règle | Pourquoi |
|---|---|---|
| 1 | **`two_d_mode: true`** partout | Le bruit vertical du GPS (2–3× le bruit horizontal) polluerait toute l'estimation |
| 2 | **Fusionner les vitesses des roues, pas les positions** | L'intégration par le filtre est meilleure que celle du contrôleur |
| 3 | **⚠️ Ne PAS fusionner `vyaw` des roues en skid-steer** | Le patinage en rotation rend la vitesse angulaire odométrique fausse de **20–50 %**. C'est le gyro qui donne la rotation. |
| 4 | **Le yaw absolu de l'IMU n'est fusionné que dans UN filtre** (le global) | Le fusionner deux fois crée des corrélations non modélisées |
| 5 | **`enable_odom_tf: false`** sur `diff_drive_controller` | Une seule source par arête TF |
| 6 | **Covariances réalistes obligatoires** | Un `NavSatFix` à covariance nulle fait diverger l'EKF. Utiliser le HDOP réel. |

## N.3 ⚠️ Le cap : le point faible du système

C'est le problème n°1 de tout robot terrestre à GPS non-RTK, et il mérite d'être posé clairement.

Le GPS donne la **position**, pas l'**orientation** (sauf en double antenne). L'orientation absolue vient du **magnétomètre** du BNO085. Or un magnétomètre monté sur un robot avec quatre moteurs BLDC et 25 A de courant continu dans des busbars en cuivre est **massivement perturbé** :

- Champ magnétique des courants (une barre parcourue par 25 A crée ~50 µT à 10 cm — soit **la valeur du champ terrestre**).
- Masses ferromagnétiques du châssis (hard iron / soft iron).
- Variation du champ perturbateur **avec le courant**, donc avec la vitesse.

Options, par qualité croissante :

| Option | Qualité de cap | Coût | Effort |
|---|---|---|---|
| BNO085 magnétomètre nu, monté n'importe où | ❌ Inutilisable | 0 | 0 |
| **BNO085 monté haut, loin des moteurs, sur mât non ferreux, + calibration hard/soft iron sérieuse** | 🟡 ±10–20° | 0 | Moyen — **minimum vital** |
| **Initialisation du cap par le déplacement** (« course over ground » GPS sur le premier mètre parcouru), puis propagation par le gyro | ✅ Bon en mouvement | 0 | Faible — **recommandé** |
| Le tout combiné | ✅ ±5° | 0 | Moyen |
| **Double antenne GNSS avec heading RTK**, injecté comme orientation IMU | ✅✅ ±0,3° | 300–800 € | Faible |

📐 **Recommandation** : monter le BNO085 sur un petit mât en nylon/alu **au moins 25 cm au-dessus des busbars et à 30 cm des moteurs**, faire une calibration complète en figure de 8, et implémenter l'initialisation de cap par le déplacement GPS. C'est gratuit et ça donne un résultat exploitable. Le double-antenne est l'évolution naturelle si la précision devient limitante.

⚠️ **Le BNO085 Game Rotation Vector** (6 axes, sans magnétomètre) a une dérive de cap documentée de **0,5°/min** ✅. Sur une mission de 10 minutes, c'est 5° de dérive — souvent préférable à un magnétomètre perturbé qui se trompe de 30° de façon imprévisible. **À évaluer expérimentalement en comparant les deux modes sur un parcours en boucle.**

## N.4 Kinect v2 — quoi en faire

| Verdict par usage | |
|---|---|
| Navigation extérieure | ❌ Inutilisable (IR actif au soleil) |
| Navigation intérieure | 🟡 Possible mais coûteux en intégration |
| **Développement et validation de la pile de perception, sur banc** | ✅ **Excellent** |
| Détection d'obstacles bas et de trous, en intérieur | ✅ Bon |
| Enregistrement de jeux de données pour développer hors robot | ✅ Excellent |

**Chemin d'intégration si tu veux l'utiliser** :

1. **M9 d'abord** : `lspci -nn | grep -i usb`. Si le contrôleur USB 3.0 est **ASMedia**, arrêter là — libfreenect2 ne fonctionne pas dessus ✅.
2. Construire libfreenect2 **avec OpenCL activé** : `intel-opencl-icd` pour l'iGPU du N5105, plus VA-API pour le décodage JPEG. ⚠️ Le README de `kinect2_ros2` **désactive explicitement CUDA et OpenCL** dans ses instructions — il faut les réactiver, sinon le pipeline CPU à ~200 ms/trame plafonne à 5 fps en saturant un cœur.
3. Porter `kinect2_ros2` de Humble vers Jazzy. L'API `rclcpp` est stable entre les deux : l'effort est probablement faible, mais non nul.
4. Alimentation : DC/DC 42→12 V **dédié, 40 W**, protégé par son propre fusible.
5. **Ne jamais le mettre dans le bringup critique.** Un fichier de lancement séparé, lancé à la demande.

**Alternative architecturale sans changer de matériel immédiatement** : sur le robot extérieur, utiliser **une simple caméra USB RGB** (globale ou grand angle) pour la supervision visuelle à distance et l'enregistrement, et laisser la détection d'obstacles au lidar (par temps couvert) et au collision monitor. La perception 3D n'est pas nécessaire pour du waypoint GPS.

**Évolution recommandée** : une caméra de profondeur adaptée à l'extérieur (stéréo passive ou stéréo active à lumière structurée avec bonne réjection solaire). C'est la phase 4.

## N.5 GPS

| Point | Recommandation |
|---|---|
| Driver | `nmea_navsat_driver` (packagé Jazzy ✅) ou `ublox` selon la réponse à **Q4** |
| Antenne | ⚠️ **Le plus haut possible, plan de masse dégagé, aucune masse métallique au-dessus.** C'est ce qui compte le plus, plus que le récepteur lui-même |
| Fréquence | 5 Hz si le récepteur le permet ; 1 Hz suffit avec un bon EKF |
| **Covariance** | ⚠️ **Obligatoire et réaliste.** Si le driver publie des zéros, écrire un petit nœud qui la calcule depuis le HDOP : `σ ≈ HDOP × UERE` avec UERE ≈ 3 m 📐 |
| Précision attendue | 2–5 m en GPS simple. Suffisant pour du waypoint, insuffisant pour du suivi de trajectoire fin |
| Évolution | RTK (base + rover, ou service NTRIP) → 2–5 cm. C'est **le** meilleur investissement pour de la navigation extérieure précise |
| Test de sanité | Le robot immobile 5 minutes : la dispersion des positions donne directement la qualité réelle du récepteur sur le site |

## N.6 Gestion de la batterie

### Répartition des rôles

| Grandeur | Source | Pourquoi |
|---|---|---|
| **SOC (%)** | **BMS, registre 0x31** | Le BMS a le comptage coulométrique et connaît l'historique du pack |
| **Tensions des 10 cellules** | **BMS, registre 0x40** | ⚠️ **L'indicateur de santé n°1** — impossible à obtenir autrement |
| Température du pack | BMS, registre 0x31 | |
| Nombre de cycles | BMS, registre 0x1B | |
| **Courant total instantané** | **ACS758** | 500 Hz, temps de réponse 4 µs — le BMS répond à 2 Hz |
| **Courant de régénération** | **ACS758** (bidirectionnel) | Le BMS ne le distingue pas forcément |
| Tension du bus | Pont diviseur + ADS1115 | Mesure locale, indépendante du BMS |
| Puissance instantanée | V_bus × I_ACS758 | |
| Énergie consommée sur la session | Intégration | 📐 Précision limitée par l'offset de l'ACS758 |

### Limites de l'ACS758 pour cet usage — chiffré

| Source d'erreur | Valeur ✅ | Sur une mesure de 10 A |
|---|---|---|
| Sensibilité (-100B) | 20 mV/A | — |
| Bruit de sortie | 6 mV (±3σ) | ±0,3 A → **3 %** |
| Offset électrique à 25 °C | ±5 mV | ±0,25 A → 2,5 % |
| **Dérive d'offset sur −40…150 °C** | **±20 mV** | **±1 A → 10 %** |
| Non-linéarité | ±1,25 % pleine échelle | ±1,25 A → 12,5 % |
| Erreur totale | −1,3 % à +2,4 % de I_P | ±1,3 à 2,4 A |

**Conclusion sans ambiguïté** : sur des courants de 5 à 20 A, l'erreur peut atteindre **1 à 2 A**. Intégrée sur une heure, c'est **1 à 2 Ah** d'erreur sur un pack de 7,8 Ah — soit **13 à 26 % du SOC**. **Le comptage coulométrique par ACS758 est inexploitable ici.**

Et ce n'est pas un défaut du composant : c'est un capteur **100 A** utilisé sur des courants de 10 A. Il est six à dix fois trop gros. En revanche, pour ce à quoi je l'utilise — détecter un dépassement de 22 A ou un pic à 35 A — il est parfait : rapide (4 µs), isolé (4800 V), et l'erreur de 1 A n'a aucune importance sur un seuil de 35 A.

**Si tu veux vraiment une mesure de courant précise** (et le BMS y répond déjà) : un **shunt de 500 µΩ + INA226/INA228** en I²C donnerait 0,1 % — mais ça ajoute une résistance en série, une dissipation, et une contrainte de câblage pour une information que le BMS fournit gratuitement.

### Stratégie de seuils

| SOC | V_pack indicatif 📐 | État | Action |
|---|---|---|---|
| > 40 % | > 37 V | Normal | — |
| 30 % | ≈ 36 V | `WARN` | Notification opérateur |
| **20 %** | ≈ 35 V | `WARN` | **Retour au point de départ automatique** |
| 10 % | ≈ 33,5 V | `DEGRADED` | `STOPPING` puis désarmement |
| — | **< 31 V** | `FAULT` | Coupure. En dessous, on abîme le pack |
| — | < 30 V | `CRITICAL` | Le BMS va couper de lui-même |

⚠️ **La correspondance SOC ↔ tension d'un pack Li-ion est très plate entre 30 % et 80 %** : la tension n'est pas un bon indicateur de charge. Ces valeurs de V_pack ne sont qu'un **repli** quand le BMS ne répond pas. Le SOC du BMS est la référence.

### Historique et détection d'anomalie

Une base SQLite locale enregistre, à chaque session :
- SOC de début et de fin, énergie consommée estimée ;
- **min / max / delta des 10 cellules**, en début et en fin de décharge ;
- température max, courant max, nombre de cycles ;
- résistance interne apparente estimée (ΔV_pack / ΔI lors d'un échelon de courant).

**Trois signaux d'alarme à surveiller dans le temps** :

| Signal | Signification |
|---|---|
| **Le Δ inter-cellules augmente d'une session à l'autre** | Une cellule vieillit plus vite. C'est le début de la fin du pack. |
| **La résistance interne apparente augmente** | Vieillissement, ou — plus inquiétant — jonction dégradée dans le circuit |
| **L'énergie utile disponible diminue** | Perte de capacité normale, à quantifier |

⚠️ Un pack de trottinette réutilisé a un historique inconnu. **Ces trois courbes sont ce qui te dira quand le remplacer avant qu'il ne devienne dangereux.**

## N.7 Ventilateurs

| Question | Réponse |
|---|---|
| **Qui les commande ?** | **ESP32-SAFETY**, pas le X1. Le refroidissement doit fonctionner même si Linux est planté — c'est précisément quand le CPU chauffe le plus. |
| Alimentation | 12 V AUX, protégé par un fusible 2 A |
| **Fréquence PWM** | **25 kHz** — standard 4 fils (spécification Intel). ⚠️ En dessous de 20 kHz, le ventilateur siffle de façon audible et désagréable |
| Type de PWM | Si ventilateurs **4 fils** : signal PWM sur le fil dédié, **collecteur ouvert 5 V**, alimentation 12 V permanente. Si **2 fils** : MOSFET low-side côté négatif + diode de roue libre, PWM 25 kHz |
| Consigne | `max(T_dissipateur_AV, T_dissipateur_AR, T_CPU)` |
| **Stratégie** | Courbe en escalier avec **hystérésis** : < 40 °C → 0 % · 40–50 °C → 30 % · 50–60 °C → 50 % · 60–70 °C → 75 % · > 70 °C → 100 %. Hystérésis de 5 °C à la descente pour éviter le pompage |
| Démarrage | ⚠️ **Impulsion à 100 % pendant 500 ms** avant d'appliquer la consigne : beaucoup de ventilateurs ne démarrent pas à 30 % depuis l'arrêt |
| Monitoring | Lecture du signal **tachy** (2 impulsions/tour) par PCNT. Publié sur CAN (`0x220`) |
| Panne | Tachy à 0 alors que PWM > 20 % pendant 3 s → `WARN` + **abaissement de tous les seuils thermiques de 10 °C** |
| Test au boot | 100 % pendant 2 s, vérification du tachy → [P1] |
| Placement | ⚠️ **Un en extraction sur la zone puissance** (variateurs), **un en soufflage sur la zone compute**. Avec des filtres si l'environnement est poussiéreux, et un flux d'air qui ne traverse pas la zone batterie. |

⚠️ **Le flux d'air ne doit pas passer sur la batterie.** Souffler de l'air chaud venu des variateurs sur un pack lithium est exactement ce qu'il ne faut pas faire.

---

*Suite : `05-reseau-code-tests-physique.md` — sections O, P, Q, R.*
