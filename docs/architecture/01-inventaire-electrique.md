# C. Inventaire et classification des composants

## C.1 Tableau maître

Légende criticité : **C1** = sa perte provoque un danger · **C2** = sa perte arrête la mission · **C3** = sa perte dégrade la mission · **C4** = confort.
Légende dépendance X1 : **Aucune** = fonctionne X1 éteint · **Faible** = fonctionne en mode dégradé · **Totale** = inutilisable sans X1.

| Composant | Fonction | Alimentation | Interface | Rôle bus | Criticité | Dépendance X1 | Comportement en panne |
|---|---|---|---|---|---|---|---|
| **Batterie M365** | Source d'énergie | — (source) | UART 115200 (BMS) | Esclave | **C1** | Aucune | BMS coupe → perte totale, robot en roue libre |
| **Coupe-batterie** | Sectionnement manuel | — | Mécanique | — | **C1** | Aucune | Ouvert = sûr |
| **Fusible MRBF 80 A** | Protection court-circuit | — | — | — | **C1** | Aucune | Fondu = sûr |
| **Contacteur DC** | Coupure commandée | Bobine 12 V AUX | GPIO ESP32-SAFETY + boucle NF | Esclave matériel | **C1** | Aucune | Ouvert au repos (NO) = sûr |
| **Champignon E-stop** | Arrêt d'urgence | — | Contact NF | — | **C1** | Aucune | Contact ouvert = sûr (sécurité positive) |
| **ACS758LCB-100B** | Mesure courant bus | 5 V LOGIC | Analogique 0–5 V | Esclave | **C1** | Aucune | Sortie figée → détecté par plausibilité |
| **ESP32-SAFETY** | Sécurité, puissance, IMU, ventilos | 5 V LOGIC (amont contacteur) | CAN + SPI + ADC + GPIO | **Maître sécurité** | **C1** | Aucune | Watchdog externe → `/SAFE` |
| **ESP32-MOTION AV** | Commande roues avant | 5 V LOGIC | CAN + DAC + PCNT + GPIO | Esclave CAN | **C1** | Faible | Watchdog → VR=0 + frein ; SAFETY détecte |
| **ESP32-MOTION AR** | Commande roues arrière | 5 V LOGIC | idem | Esclave CAN | **C1** | Faible | idem |
| **ZS-X11H × 4** | Variateur BLDC | **BUS+ (30–42 V)** | Analogique VR + GPIO + Hall | Esclave | **C1** | Aucune | Pas de commande → moteur libre |
| **Moteur-roue × 4** | Traction | Phases variateur | Hall 5 V | — | **C1** | Aucune | Bloqué → surintensité détectée |
| **Youyeetoo X1** | Calculateur ROS 2 | **12 V COMPUTE** | USB, Ethernet, UART, I²C | Maître mission | **C2** | — | Le robot s'arrête proprement (N3/N4) |
| **YDLIDAR X4** | Lidar 2D | **5 V dédié** ⚠️ | UART→USB (CP2102) | Esclave | **C3** | Totale | Nav2 en mode dégradé |
| **Kinect v2** | RGB-D | **12 V / 2,67 A secteur** | USB 3.0 dédié | Esclave | **C4** | Totale | Perception 3D perdue |
| **BNO085** | IMU 9 axes | 3,3 V (depuis 5 V) | **SPI vers ESP32-SAFETY** | Esclave | **C2** | Aucune | EKF sans IMU → odométrie seule |
| **GPS USB** | Position absolue | 5 V USB | USB CDC | Esclave | **C2** | Totale | Localisation globale perdue |
| **Ventilateur PWM 12 V × 2** | Refroidissement | **12 V AUX** | PWM 25 kHz + tach | Esclave | **C3** | Aucune | Tach à 0 → dérating thermique |
| **DC/DC 42→12 V COMPUTE** | Rail calculateur | BUS+ | — | — | **C2** | Aucune | Perte du X1 → N3/N4 |
| **DC/DC 42→12 V AUX** | Rail auxiliaire | BUS+ | — | — | **C1** | Aucune | Perte bobine → contacteur ouvre = sûr |
| **DC/DC 12→5 V LOGIC** | Rail logique | 12 V AUX | — | — | **C1** | Aucune | Perte ESP32 → contacteur ouvre = sûr |
| **Adaptateur USB-CAN** | Passerelle CAN | 5 V USB | USB ↔ CAN | Esclave | **C2** | Totale | Perte du lien → N3 sur les ESP32 |

## C.2 Fiches détaillées

### C.2.1 Batterie Xiaomi M365

| Paramètre | Valeur | Confiance |
|---|---|---|
| Capacité | **280 Wh** (Classic) / 474 Wh (Pro) | ✅ manuel Xiaomi |
| Tension nominale | **36 V** (37 V pour le Pro) | ✅ manuel Xiaomi |
| Tension max (charge) | **42 V** | ✅ manuel Xiaomi |
| Configuration | **10S3P**, 30 cellules 18650, 7,80 Ah, marquage `10INR19/66-3` | 🟡 analyse matérielle publiée (arXiv 2411.17184) |
| Seuil surtension cellule | 4200 mV (dangereux) / 4700 mV (critique) | 🟡 firmware BMS analysé |
| Seuil sous-tension cellule | 2750 mV (dangereux) / 1580 mV (critique) | 🟡 idem |
| Seuil de déséquilibre | 30 mV (alerte) / 800 mV (critique) | 🟡 idem |
| BMS | Carte propriétaire : MCU **ST STM8L151K6** + AFE **TI BQ76930** | 🟡 idem |
| **Courant continu max** | 🔴 **NON PUBLIÉ** | — |
| **Courant de pointe / seuil court-circuit** | 🔴 **NON PUBLIÉ** | — |
| Connecteur de puissance d'origine | **Amass XT-30** (côté ESC) | 🟡 ScooterHacking |
| Connecteur BMS | **JST PAP03-V (3 points)** : `R`=RX ESC ← TX BMS, `T`=TX ESC → RX BMS, `L`=feu stop | 🟡 idem |
| Protocole BMS | UART **115200 bauds**, trames `55 AA` Xiaomi/Ninebot | 🟡 rétro-ingénierie recoupée |
| Niveau logique BMS | 🔴 **3,3 V présumé, non documenté** | — |

⚠️ **Le connecteur XT-30 d'origine est prévu pour ~30 A.** Il n'est **pas** compatible avec un budget supérieur. Voir §D.5.

⚠️ Le BMS doit être raccordé côté **`P-`** et non `C-`, sous peine de surtension à la régénération (🟡 documenté par la communauté BMS M365).

### C.2.2 ZS-X11H (× 4)

| Paramètre | Valeur | Confiance |
|---|---|---|
| Tension d'entrée | **6–60 V** ou **9–60 V** selon le revendeur | 🔴 **contradiction** |
| Courant nominal | **16 A** | 🟡 3 revendeurs concordants |
| Courant de pointe | **20 A** (refroidissement actif recommandé) | 🟡 idem |
| Puissance | 350–400 W | 🟡 |
| Sortie 5 V logique | ≤ **30 mA** | 🔴 texte revendeur unique |
| Fusible interne | **Aucun** — fusible externe obligatoire | 🟡 |
| Seuil de protection surintensité | 🔴 **non publié** | — |
| Capteurs Hall | **Obligatoires** (pas de mode sensorless). Connecteur JST 5 fils : GND(noir), Hc(vert), Hb(jaune), Ha(blanc), 5V(rouge) | 🟡 |
| Entrée `VR` | Consigne analogique **0–5 V** ; démarrage mesuré à ≈ **0,07 V** | 🟡 mesure publiée |
| Entrée `DIR`/`ZF` | Sens de rotation, **actif bas** | 🟡 2 sources concordantes |
| Entrée `EL`/`BRAKE` | 🔴 **Polarité contradictoire** : une source dit actif-haut 5 V, l'autre dit « à la masse = arrêt » | **MESURE M4** |
| Entrée `STOP` | Enable/arrêt, actif bas | 🔴 source unique |
| Sortie `SC` | Impulsion de vitesse, **bascule à chaque transition Hall** → 90 transitions/tour sur 6,5". **Non signée.** Amplitude/impédance non publiées | 🟡 / **MESURE M7** |
| Mode PWM | Nécessite le **cavalier J1**, souvent **non peuplé** (soudure sous le dissipateur) + potentiomètre au minimum. 50 Hz–20 kHz / 2,5–5 V (une source) ou 1–10 kHz (une autre) | 🔴 **MESURE M5** |
| Dimensions | 63 × 45 × 31 mm, dissipateur intégré | 🟡 |

⚠️ **Le ZS-X11H n'a ni limitation de courant réglable, ni mode couple, ni rampe d'accélération documentée.** Toute la protection en courant doit être faite en amont (fusible) et en aval de la consigne (limitation logicielle ESP32 + surveillance ACS758).

### C.2.3 Moteurs-roues hoverboard 6,5"

| Paramètre | Valeur | Confiance |
|---|---|---|
| Paires de pôles | **15** (30 aimants) → 90 états Hall/tour | 🟡 fort (mesure ODrive publiée) |
| Résistance de phase | ≈ **0,179 Ω** | 🟡 mesure ODrive |
| Inductance de phase | ≈ **336 µH** | 🟡 idem |
| Capteurs Hall | 3, numériques, **alimentés en 5 V**, 120° électriques | 🟡 |
| Tension nominale | 36 V | 🟡 |
| Puissance | 250 W ou 350 W selon fournisseur | 🟡 |
| Vitesse à vide sous 36 V | ≈ **578 tr/min** (mesuré) — les « 800 tr/min » des revendeurs sont douteux | 🟡 |
| Masse | ≈ 2,9 kg | 🔴 |
| **Courant de calage** | 🔴 **non publié** — *estimation dérivée : ≈ 100 A à 36 V* (R_ligne ≈ 0,358 Ω), hors saturation | calcul, **pas** une spec |
| **Couple** | 🔴 **non publié** — *estimation dérivée : ≈ 8 N·m à 16 A* (Kt ≈ 0,52 N·m/A dérivé d'un Kv d'exemple ≈16 rpm/V) | calcul, **pas** une spec |

⚠️ L'estimation de calage à ~100 A par moteur est cohérente avec les 20 A de pointe du ZS-X11H : **un moteur bloqué en butée verra sa carte partir en protection ou en fumée avant d'atteindre le calage**. La détection de calage doit être logicielle et rapide (§M).

### C.2.4 Youyeetoo X1

| Paramètre | Valeur | Confiance |
|---|---|---|
| SoC | Intel **Celeron N5105** (Jasper Lake, 4 cœurs, 2,0/2,9 GHz, TDP 10 W) | ✅ |
| RAM | LPDDR4 soudée 4/8/16 Go | ✅ |
| **Alimentation** | **12 V DC**, jack 5,5×2,5 mm **ou header XH2.54 2 broches** | ✅ |
| Consommation mesurée | Éteint 0,56 W · **Idle 6,9 W** · **Charge 21,6 W** (1,76 A à 12 V) | ✅ mesure indépendante CNX |
| USB | **2 × USB-A 3.0** + 2 × USB-A 2.0 + 2 × USB 2.0 sur header | ✅ |
| Ethernet | 1 × RJ45 Gigabit | ✅ |
| UART | **3 × TTL 3,3 V** → `/dev/ttyS0`, `/dev/ttyS4`, `/dev/ttyS5` | ✅ |
| I²C | 1 bus (I2C3) | ✅ |
| SPI | 1, **nécessite une recompilation du noyau** | ✅ |
| **CAN** | ❌ **Aucun contrôleur natif** | ✅ |
| GPIO | 5, niveau 1,8 V **ou 3,3 V** configurable au BIOS | ✅ |
| OS documenté | **Ubuntu 22.04 uniquement** dans le wiki officiel. 24.04 non mentionné. | ✅ (fait) / 📐 (Jasper Lake est mainline depuis le noyau 5.10 : 24.04 devrait fonctionner, le risque est sur les E/S bas niveau) |

⚠️ **Le header XH2.54 2 broches en 12 V est un vrai atout** : alimentation directe depuis le DC/DC, sans jack coaxial (qui est un point de défaillance en vibration).

⚠️ **Contrainte USB 3.0 forte** : 2 ports seulement. Le Kinect v2 en exige un **dédié et non partagé** (transferts isochrones), sur contrôleur Intel ou NEC/Renesas — **ASMedia ne fonctionne pas** (✅ libfreenect2). D'où la mesure **M9**.

### C.2.5 Capteurs — synthèse chiffrée

| Capteur | Alim | Courant | Pointe | Interface | Point d'attention |
|---|---|---|---|---|---|
| **YDLIDAR X4** | **4,8–5,2 V** ✅ | 350–500 mA ✅ | **1000 mA au démarrage moteur** ✅ | UART 3,3 V 128000 bauds → adaptateur CP2102 | ⚠️ Le manuel avertit que le port USB de nombreuses cartes ne fournit pas assez de courant : **alimenter le port `USB_PWR` séparément en 5 V**. Ne pas utiliser de power bank (ondulation). |
| **BNO085** | VDD 2,4–3,6 V, VDDIO 1,7–3,6 V ✅ | ≈ **11 mA** (RV 9 axes @100 Hz, 35 mW) ✅ | — | **SPI 3 MHz** (recommandé), I²C 400 k, UART-RVC 115200 | ⚠️ **Ne pas utiliser l'I²C** : Adafruit documente que le BNO085 « viole le protocole I²C » et est **incompatible avec l'ESP32**. ⚠️ VDD doit monter **avant ou avec** VDDIO. Init ≥ 90 ms après reset. |
| **Kinect v2** | **12 V / 2,67 A (32 W)** secteur ✅ | — | — | USB 3.0 dédié | ⚠️ 5–10 % de la bande passante USB 3.0. Pipeline CPU ≈ **200 ms/trame** → OpenCL/CUDA obligatoire. Pas de driver ROS 2 Jazzy. |
| **GPS USB** | 5 V USB | 🔴 à mesurer (M11) | — | USB CDC | — |
| **ACS758LCB-100B** | **3,0–5,5 V**, calibré à 5 V ✅ | 10 mA typ ✅ | — | Analogique | Sensibilité **20 mV/A**, zéro à **V_CC/2**, bruit 6 mV (0,3 A), dérive offset **±20 mV (±1 A)**, BP 120 kHz, t_réponse 4 µs, R_primaire **100 µΩ** (→ **1 W à 100 A**), isolation 4800 V_RMS. **NRND depuis mars 2025.** |

## C.3 Bilan de puissance

### Rail 12 V COMPUTE

| Charge | Typ. | Max | Note |
|---|---|---|---|
| Youyeetoo X1 | 7 W | **22 W** | ✅ mesuré |
| Hub USB 3.0 alimenté + périphériques | 5 W | 12 W | 📐 |
| Kinect v2 | *alimenté séparément* | *32 W* | ✅ bloc dédié |
| **Total (Kinect exclu)** | **12 W** | **34 W** | |
| **DC/DC recommandé** | | **12 V / 8 A (96 W)** | Marge ×2,8 |

### Rail 12 V AUX

| Charge | Typ. | Max |
|---|---|---|
| 2 × ventilateur PWM | 3 W | 10 W 📐 |
| Bobine contacteur DC | 2 W (avec économiseur) | 8 W 📐 |
| Relais de précharge | — | 1 W |
| DC/DC 12→5 V LOGIC (voir ci-dessous) | 8 W | 18 W |
| **Total** | **13 W** | **37 W** |
| **DC/DC recommandé** | | **12 V / 5 A (60 W)** |

### Rail 5 V LOGIC

| Charge | Typ. | Max |
|---|---|---|
| 3 × ESP32 (WiFi désactivé) | 3 × 0,5 W | 3 × 1,5 W |
| YDLIDAR X4 | 2,5 W | **5 W (pointe 1 A)** ✅ |
| BNO085 | 0,04 W | 0,1 W |
| ACS758 | 0,05 W | 0,07 W |
| Transceivers CAN, level shifters, DAC | 0,5 W | 1 W |
| Marge capteurs futurs | — | 5 W |
| **Total** | **≈ 6 W** | **≈ 16 W** |
| **DC/DC recommandé** | | **5 V / 5 A (25 W)** |

### Bilan côté bus 36 V

| Rail | P_max | Rendement 📐 | I_bus à 36 V | Fusible retenu |
|---|---|---|---|---|
| 12 V COMPUTE | 34 W | 90 % | 1,05 A | **MIDI 58 V — 10 A** |
| 12 V AUX (5 V inclus) | 37 W | 88 % | 1,17 A | **MIDI 58 V — 5 A** |
| Motorisation (budget) | 720 W continu / 1080 W crête 2 s | — | **20 A / 30 A** | 4 × **MIDI 58 V — 25 A** |
| **Total continu** | | | **≈ 22,2 A** | **MRBF 58 V — 80 A** (tête) |

Le fusible de tête n'est pas dimensionné sur le courant nominal mais sur **la somme des courants de pointe simultanés que les câbles peuvent supporter sans dommage** (4 × 20 A + 3 A = 83 A). Un fusible plus petit fondrait sur un franchissement d'obstacle légitime. La protection contre un excès de courant *raisonnable mais trop long* est faite en logiciel (§M), pas par le fusible — c'est le rôle correct de chacun.

---

# D. Architecture électrique

## D.1 Principe : cinq zones, une seule masse de référence

```
ZONE 0 — SOURCE           ZONE 1 — PROTECTION & COUPURE       ZONE 2 — DISTRIBUTION
┌──────────────┐          ┌───────────────────────────┐       ┌──────────────────┐
│ Pack M365    │          │ Coupe-bat · MRBF · ACS758 │       │ Busbar + / −     │
│ + BMS        │─────────►│ Contacteur · Précharge    │──────►│ 6 dérivations    │
└──────────────┘          └───────────────────────────┘       └──────────────────┘
                                       ▲                                 │
                                       │ /SAFE + bobine                  │
ZONE 4 — SÉCURITÉ ────────────────────┘                                 │
┌────────────────────────────┐                     ┌────────────────────┴─────┐
│ ESP32-SAFETY · champignon  │                     ▼                          ▼
│ watchdog matériel          │        ZONE 3 — PUISSANCE            ZONE 5 — ÉLECTRONIQUE
└────────────────────────────┘        4 × ZS-X11H → 4 moteurs      DC/DC → X1, ESP32, capteurs
```

## D.2 Schéma électrique de puissance

```
        PACK Xiaomi M365 10S · 30–42 V · 280 Wh
        ┌───────────────────────────────────────┐
        │  BMS interne (STM8L + BQ76930)        │
        │  UART 115200 ──────────────────────────────────► ESP32-SAFETY (via isolateur)
        └────┬──────────────────────────────┬───┘
          P+ │                              │ P−
             │                              │
   ╔═════════▼══════════╗                   │
   ║ ① COUPE-BATTERIE   ║  Sectionneur DC rotatif à clé, ≥48 V DC, ≥150 A
   ║   accessible sans  ║  Manœuvre uniquement à courant nul (procédure §L)
   ║   outil, extérieur ║
   ╚═════════┬══════════╝
             │
   ╔═════════▼══════════╗
   ║ ② FUSIBLE MRBF     ║  80 A · 58 V DC · AIC 2000 A @58 V
   ║   80 A             ║  ⚠️ Monté DIRECTEMENT sur la borne + du pack
   ╚═════════┬══════════╝     (règle ABYC : ≤ 175 mm du point de connexion)
             │
   ╔═════════▼══════════╗
   ║ ③ ACS758LCB-100B   ║  Conducteur primaire 100 µΩ · sortie 0–5 V → ADC ESP32-SAFETY
   ║   (bidirectionnel) ║  Mesure aussi le courant de RÉGÉNÉRATION (valeur négative)
   ╚═════════┬══════════╝
             │
       ┌─────┴────────────────────────────────┐
       │                                      │
   ╔═══▼════════════════╗          ╔══════════▼═══════════════╗
   ║ ④ CONTACTEUR DC    ║          ║ ⑤ PRÉCHARGE              ║
   ║   NO, bobine 12 V  ║          ║   R = 10 Ω / 25 W bobinée║
   ║   ≥100 A / 48 V DC ║◄─────────╢   + relais signal 10 A   ║
   ╚═══╤════════════════╝  parallèle╚══════════════════════════╝
       │
       │  BOBINE alimentée à travers :
       │  12V_AUX ─ champignon NF ─ transistor ESP32-SAFETY ─ RC 1 s ─ bobine
       │
   ╔═══▼═══════════════════════════════════════════════════════════════╗
   ║                    ⑥ BUSBAR + (Cu-ETP 20×3 étamé)                 ║
   ╚═╤═══════╤═══════╤═══════╤════════════╤════════════╤══════════╤════╝
     │       │       │       │            │            │          │
  MIDI25  MIDI25  MIDI25  MIDI25       MIDI10       MIDI5     [2 postes
     │       │       │       │            │            │       libres]
     ▼       ▼       ▼       ▼            ▼            ▼
  ZS-X11H ZS-X11H ZS-X11H ZS-X11H     DC/DC 12V    DC/DC 12V
   AV-G    AV-D    AR-G    AR-D        COMPUTE        AUX
     │       │       │       │            │            │
     ▼       ▼       ▼       ▼          X1 +      ventilos,
  Moteur  Moteur  Moteur  Moteur       hub USB    bobine,
   AV-G    AV-D    AR-G    AR-D                   DC/DC 5V
     │       │       │       │            │            │
   ╔═╧═══════╧═══════╧═══════╧════════════╧════════════╧══════════════╗
   ║                    ⑦ BUSBAR − (Cu-ETP 20×3 étamé)                 ║
   ║               = POINT DE MASSE UNIQUE DU ROBOT                    ║
   ╚═══════════════════════════════┬═══════════════════════════════════╝
                                   │
                                   └──────► retour P− du pack
```

## D.3 Justification de chaque protection

### ① Coupe-batterie — où, et lequel

**Position** : entre la borne `+` du pack et le fusible. **Physiquement accessible depuis l'extérieur du robot, sans outil, sans ouvrir de capot**, et identifié par un marquage rouge.

**Ce qu'il ne faut PAS prendre** : un interrupteur-sectionneur modulaire de tableau domestique (type Legrand DX³-IS 100 A). Ces appareils sont marqués **400 V~**, leur pouvoir de fermeture/coupure est déclaré en AC uniquement, ils n'ont **aucune fonction de protection**, et aucune tenue vibratoire spécifiée.

**Ce qu'il faut** : un **sectionneur DC de type marine**, exemple de classe : Blue Sea m-Series, 300 A continu, **48 V DC**, contacts à forte pression, corps étanche. 📐 ~40–60 €.

> **Nuance honnête** : la règle « jamais d'appareillage AC en DC » est trop simpliste à 42 V. Legrand publie explicitement une note (F03693FR-01) autorisant ses DX3 en DC : **60 V DC pour un unipolaire 1 module**, avec un seuil magnétique multiplié par **1,4** et une endurance réduite à 2000 manœuvres. Un DX3 est donc *dans son domaine d'emploi déclaré* à 42 V. Les vraies raisons de le refuser ici sont : le « 100 A » de GSB est un sectionneur sans protection ; le pouvoir de coupure DC n'est pas marqué sur le produit ; aucune qualification vibratoire ; et un thermique 100 A ne protège aucun câble de moins de 35 mm².

### ② Fusible principal — pourquoi MRBF 80 A et pas un disjoncteur 100 A

**Le courant de court-circuit prospectif du pack** est le paramètre dimensionnant. Il faut le calculer à partir de la résistance interne (**mesure M2**) :

| Hypothèse 10S3P | R_int estimée | I_cc aux bornes |
|---|---|---|
| Cellules 18650 à 30 mΩ, 3P | 100 mΩ | ≈ 420 A |
| Cellules à 15 mΩ, 3P | 50 mΩ | ≈ 840 A |
| Cellules basse impédance | 30 mΩ | ≈ 1400 A |

📐 Ces valeurs sont des ordres de grandeur : à confirmer par **M2**. On dimensionne pour l'hypothèse haute.

| Candidat | U_DC | AIC | Verdict |
|---|---|---|---|
| Fusible lame ATO/MAXI | **32 V** ✅ | 1000 A @32 V | ❌ **42 V > 32 V**. À 42 V, l'arc d'un ATO ne s'éteint pas franchement : carbonisation du porte-fusible, fusion du boîtier, incendie |
| MIDI / MEGA standard | **32 V** ✅ | 1000–2000 A @32 V | ❌ idem — sauf **versions 58 V / 80 V** (Victron) ✅ |
| **MRBF (terminal fuse)** | **58 V** ✅ | **2000 A @58 V** ✅ | ✅ **Retenu** — se monte directement sur la borne du pack |
| Class T | **125 V** ✅ | **20 000 A @125 V** ✅ | ✅ Techniquement supérieur, mais calibre minimum 110 A → trop gros ici |
| Disjoncteur Blue Sea 187 | 48 V | 1500 A @48 V | 🟡 Acceptable en dérivé, **pas** en tête de pack |

→ **MRBF 80 A / 58 V**, monté sur la borne `+` du pack, à moins de 175 mm (règle ABYC E-11). Si **M2** révèle un I_cc supérieur à 1500 A, passer en **Class T 110 A**.

### ③ ACS758 — position dans le circuit

Placé **après** le fusible et **avant** le contacteur, sur le conducteur `+`. Cette position permet de mesurer :
- le courant de précharge (diagnostic de court-circuit avant fermeture du contacteur) ;
- le courant total de tous les consommateurs ;
- le **courant de régénération** (négatif, grâce à la version bidirectionnelle).

Câblage du signal : sortie analogique → filtre RC (**1 kΩ + 10 nF**, la datasheet impose R_LOAD ≥ 4,7 kΩ et C_LOAD ≤ 10 nF ✅) → ADC de l'ESP32-SAFETY. ⚠️ L'ADC de l'ESP32 ne tolère que 3,3 V : **pont diviseur 2:1 obligatoire** (2 × 10 kΩ), ce qui divise aussi la résolution par 2. Alternative recommandée : **ADS1115 en I²C** (16 bits, entrée 0–5 V avec référence interne, PGA) — bien meilleur et découple la mesure du bruit numérique de l'ESP32.

⚠️ **Rappel** : cette mesure sert à la **protection**, pas au SOC. Avec ±1 A d'erreur d'offset thermique et 0,3 A de bruit, elle ne permet pas de compter les ampères-heures. Le SOC vient du BMS (§N.6).

### ④ Contacteur DC — le cœur de la sécurité positive

Un contacteur **normalement ouvert** : au repos, sans alimentation, il est ouvert. Toute panne d'alimentation logique, de microcontrôleur ou de faisceau conduit à l'état sûr.

Sa bobine est alimentée par une **chaîne série** :

```
12V_AUX ──► contact NF du champignon ──► contact NF e-stop radio (futur)
        ──► transistor de commande (ESP32-SAFETY, actif haut)
        ──► [réseau RC de temporisation ~1 s]
        ──► bobine du contacteur ──► GND
```

**Pourquoi la temporisation RC** : le champignon coupe la chaîne instantanément. Mais si le contacteur s'ouvre immédiatement, les ZS-X11H perdent leur alimentation et **ne peuvent plus freiner**. La ligne `/SAFE` (qui va aux entrées `EL`/`STOP` des variateurs) est tirée à la masse **par le même contact**, de façon directe et non temporisée. Le freinage électrique a donc ~1 s pour agir avant la coupure de puissance.

Choix : contacteur DC dédié (type Gigavac, TE EV200) ou, plus économique, un **relais de puissance automobile DC 48 V / 150 A**. 📐 Prévoir un **économiseur de bobine** (PWM après enclenchement) pour ne pas dissiper 8 W en permanence.

### ⑤ Précharge — dimensionnement chiffré

Sans précharge, la fermeture du contacteur sur les condensateurs déchargés des quatre variateurs produit un courant crête limité seulement par la résistance de boucle :

> I_crête ≈ V / R_boucle ≈ 42 / 0,005 ≈ **8400 A** (bridé en pratique par l'inductance et la R interne du pack, mais on reste dans les kA)

Conséquences réelles et documentées : soudure des contacts, cratérisation des connecteurs, fatigue des condensateurs, **vieillissement invisible du fusible** (fusion partielle → coupure intempestive des mois plus tard).

**Calcul** (formules Sensata) avec 📐 C_bus totale estimée à **10 mF** (à confirmer par **M10**) et un objectif de 0,5 s :

| Grandeur | Formule | Valeur |
|---|---|---|
| τ visé | t / 5 | 0,1 s |
| **R précharge** | τ / C | **10 Ω** |
| I crête | V/R | 4,2 A |
| P crête | V²/R | 176 W (décroissante) |
| **Énergie dissipée** | ½·C·V² | **8,8 J** (indépendante de R) |
| P moyenne sur 0,5 s | E/t | 17,6 W |

→ **Résistance 10 Ω, 25 W, bobinée, tenue impulsionnelle ≥ 50 J.** Le dimensionnement se fait sur l'**énergie impulsionnelle**, pas sur la puissance continue : vérifier la courbe *single pulse energy* du fabricant (Vishay, Ohmite, Arcol AP/HS).

⚠️ **Défaut à couvrir absolument** : si le bus est en court-circuit en aval, la résistance encaisse 42²/10 = **176 W en continu** et finit en fumée. → L'ESP32-SAFETY impose un **timeout** : si V_bus n'atteint pas 90 % de V_pack après 1,5 s, la séquence est abandonnée, le relais de précharge ouvert, et l'état passe en `FAULT_PRECHARGE`. C'est aussi **le diagnostic de court-circuit avant mise sous puissance** — un test gratuit et précieux.

**Séquence de mise sous tension** (pilotée par l'ESP32-SAFETY) :

```
1. Coupe-batterie fermé manuellement (V_pack présent, contacteur ouvert)
2. ESP32-SAFETY démarre sur 5V_LOGIC (alimenté EN AMONT du contacteur)
3. Auto-test : V_pack plausible ? champignon relâché ? ACS758 au zéro ?
4. Fermeture du relais de précharge
5. Surveillance de la montée de V_bus (mesure par pont diviseur)
      ├─ V_bus ≥ 0,90 × V_pack avant 1,5 s → OK
      └─ sinon → ABANDON, FAULT_PRECHARGE
6. Fermeture du contacteur principal
7. Ouverture du relais de précharge
8. État LIVE_DISARMED — puissance présente, moteurs interdits
```

**Séquence de mise hors tension** :

```
1. Consignes à zéro, attente de vitesse nulle (retour Hall)
2. Assertion du frein électrique, attente 300 ms
3. Ouverture du contacteur principal (à courant quasi nul)
4. Décharge du bus par résistance de bleed permanente (§D.8)
5. Confirmation V_bus < 12 V en moins de 5 s
6. Ouverture du coupe-batterie (manuel)
```

⚠️ **Interdiction absolue** : débrancher un connecteur de puissance sous charge. Chaque connecteur de puissance doit avoir un **contact pilote** (broche courte) coupant l'`enable` avant l'ouverture des broches de puissance — ou, à défaut, une procédure écrite affichée sur le robot.

## D.4 ⚠️ Le point le plus dangereux du montage : 42 V sur les ZS-X11H

Les sources revendeurs se contredisent : **6–60 V** pour certaines, **9–60 V** pour d'autres. À 42 V pleine charge on est théoriquement à 70 % de la limite haute — confortable. **Mais** :

**Mesure M12 impérative** : lire le marquage des condensateurs électrolytiques d'entrée de chaque carte.
- Marqués **63 V** → conforme, marge correcte.
- Marqués **50 V** → à 42 V on est à 84 % de leur tenue nominale. Avec les surtensions de commutation inductive et la régénération, c'est **insuffisant**. Il faudrait alors, soit remplacer les condensateurs par des 63 V, soit **plafonner la charge du pack à 40 V** (le chargeur d'origine monte à 42 V — il faudrait un chargeur réglable). Ajouter une TVS ne suffit pas : une TVS dont le standoff est > 42 V écrête trop haut pour protéger des condensateurs 50 V.
- Marqués **35 V** ou moins → **ne pas brancher**. La carte est prévue pour du 24 V.

Cette vérification prend deux minutes et évite quatre cartes détruites à la première charge complète.

## D.5 Connectique de puissance

| Liaison | Connecteur d'origine | Retenu | Pourquoi |
|---|---|---|---|
| Pack → coupe-batterie | **XT-30** (≈30 A) 🟡 | **Anderson SB50** ou **XT90-S** | Le XT-30 est sous-dimensionné pour 80 A de pointe. Le XT90-**S** intègre une résistance anti-étincelle. |
| Busbar → ZS-X11H | vis | Cosse à œil M6 sertie | Reprise mécanique, contrôle du couple |
| Variateur → moteur (phases) | Cosses ressort L 4,8/6,3 mm 🟡 | Cosses Faston isolées 6,3 mm **+ gaine thermo à colle** | Standard hoverboard, conserver |
| Halls moteur | JST PH 5 points | Conserver, **+ ferrite + torsadage** | Signaux sensibles à proximité des phases |
| Bus CAN | — | **Connecteurs Molex Micro-Fit 4 pos** détrompés | Détrompage obligatoire (CANH/CANL/GND/+5 V) |

⚠️ **Codes couleur et détrompage** : deux connecteurs différents ne doivent **jamais** pouvoir s'échanger. Voir §R.5.

## D.6 Dimensionnement des câbles

**Méthode retenue** (conservatrice) : partir de la table **ABYC E-11 Table VI-A** (conducteur seul), puis appliquer explicitement le facteur de groupement Table 6A puis le facteur de température.

Facteurs de groupement ABYC ✅ : 2–3 conducteurs → **0,70** · 4–6 → 0,60 · 7–24 → 0,50
Facteur de température 📐 : `k = √((T_iso − T_amb)/(T_iso − 30))` → pour isolant 105 °C à 60 °C ambiant : **k = 0,77**

| Tronçon | I max | Calcul | **Section retenue** | Type |
|---|---|---|---|---|
| Pack → busbar (tête) | 83 A crête | 25 mm² : 170 × 0,70 × 0,77 = **92 A** ✅ | **25 mm²** | Souple classe 5, **cuivre étamé**, isolant 105 °C |
| Busbar → ZS-X11H (×4) | 25 A (fusible) | 6 mm² : 60 × 0,60 × 0,77 = **28 A** ✅ | **6 mm²** | idem |
| ZS-X11H → moteur (phases) | 25 A | idem | **6 mm²** | idem, torsadé par 3 |
| Busbar → DC/DC 12 V COMPUTE | 10 A | 2,5 mm² : 35 × 0,60 × 0,77 = **16 A** ✅ | **2,5 mm²** | idem |
| Busbar → DC/DC 12 V AUX | 5 A | 1,5 mm² : 25 × 0,60 × 0,77 = **11,5 A** ✅ | **1,5 mm²** | idem |
| Sorties 12 V | 8 A / 5 A | | **1,5 mm²** | Souple 105 °C |
| Sorties 5 V | 5 A | | **1,0 mm²** | idem |
| Signaux CAN | — | | **2 × 0,34 mm² torsadé blindé** + 2 × 0,5 mm² alim | Blindage relié à la masse **d'un seul côté** |
| Halls moteur | — | | **5 × 0,25 mm² blindé** | Blindage côté variateur uniquement |

❌ **Interdits** : H07V-U / H07V-R rigide (fatigue par vibration), fil monobrin, câble non étamé en zone humide.

**Chute de tension** — vérification, et un résultat contre-intuitif :
> ΔU = 2·ρ·L·I/S avec ρ_Cu ≈ **0,0205 Ω·mm²/m à 70 °C** (et non 0,0172 à 20 °C : +19 %)
> Tronçon de tête, L = 1 m, I = 25 A, S = 25 mm² → ΔU = **41 mV** (0,11 %)

**À ces longueurs, la chute de tension dans les câbles n'est pas le critère dimensionnant — l'ampacité l'est, d'un facteur ~10.** Le vrai budget de chute de tension est dans les **jonctions** :

| Élément | R typique | ΔU @25 A | Dissipation |
|---|---|---|---|
| 1 m de 25 mm² | 0,82 mΩ | 21 mV | 0,5 W |
| Cosse sertie **correcte** | 20–50 µΩ | 1 mV | 0,03 W |
| Cosse sertie **médiocre / oxydée** | 0,5–2 mΩ | 12–50 mV | **0,3–1,3 W** |
| Boulon busbar M6 **bien serré** | 3–10 µΩ | <1 mV | négligeable |
| Boulon busbar **desserré** | 1–10 mΩ | 25–250 mV | **0,6–6 W 🔥** |

**Règle à retenir : 1 mΩ = 10 W à 100 A, ou 0,6 W à 25 A.** À notre échelle de courant, une jonction dégradée ne prend pas feu instantanément — mais elle chauffe, s'oxyde davantage, et la résistance croît. C'est un emballement lent. La thermographie périodique (§R.7) est le seul moyen de le voir venir.

## D.7 Rails d'alimentation et séparation des masses

```
                        BUSBAR + (30–42 V)
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
   ┌────▼─────┐         ┌────▼─────┐        (motorisation)
   │ DC/DC 12V│         │ DC/DC 12V│
   │ COMPUTE  │         │   AUX    │
   │ ISOLÉ ★  │         │ non isolé│
   └────┬─────┘         └────┬─────┘
        │ 12V_C              │ 12V_A
        │                    ├──────────────► ventilateurs PWM
        ├──► Youyeetoo X1    ├──────────────► bobine contacteur (via champignon)
        ├──► hub USB 3.0     ├──────────────► relais précharge
        │                    │
        │               ┌────▼─────┐
        │               │DC/DC 5V  │
        │               │  LOGIC   │
        │               └────┬─────┘
        │                    │ 5V_L
        │                    ├──► ESP32 × 3 (+ transceivers CAN)
        │                    ├──► BNO085 (via LDO 3,3 V)
        │                    ├──► YDLIDAR X4 (port USB_PWR)  ⚠️ pointe 1 A
        │                    ├──► ACS758 + ADS1115
        │                    └──► 74LVC245 (adaptation Hall)
        │
   Toutes les masses ────────────────────────► BUSBAR −
```

★ **Pourquoi isoler le DC/DC COMPUTE** : le X1 est relié au reste du monde par des câbles USB (lidar, GPS, Kinect, USB-CAN) dont le blindage porte la masse. Si sa masse est directement le busbar négatif — parcouru par 20 A de courant moteur haché — les boucles de masse injectent du bruit dans les liaisons USB. Un DC/DC **isolé galvaniquement** casse la boucle. 📐 Coût supplémentaire ~20–30 €, bénéfice majeur sur la stabilité USB.

**Topologie de masse retenue : étoile à point unique sur le busbar −.**

| Règle | Raison |
|---|---|
| Le busbar − est le **seul** point de jonction des masses | Évite les boucles |
| Les masses de puissance (variateurs, moteurs) arrivent sur des postes **distincts** de celles de l'électronique | Évite que le courant moteur traverse le chemin de retour de la logique |
| La masse logique (5 V) arrive sur **un seul** poste du busbar −, en un point éloigné des retours moteurs | Réduit la tension de mode commun |
| Le blindage des câbles CAN et Hall est relié **d'un seul côté** | Évite le courant de boucle dans le blindage |
| Le châssis métallique **n'est pas** un conducteur de retour | Un châssis parcouru par du courant crée des différences de potentiel imprévisibles |
| Le châssis est relié au busbar − **en un seul point**, par une tresse | Équipotentialité sans boucle |

⚠️ **À 42 V DC, le risque n'est pas l'électrisation** (la limite TBTS/SELV est de 120 V DC lisse selon IEC 60364-4-41). **Le risque est thermique et incendiaire.** Toute la conception doit viser le feu, pas le choc électrique.

## D.8 Protections complémentaires

| Protection | Où | Composant 📐 | Pourquoi |
|---|---|---|---|
| **Inversion de polarité** | Entrée générale | **1. Connecteurs détrompés** (Anderson SB50 rouge, XT90-S) **2.** Si besoin d'une protection active : LTC4359 + 4 MOSFET 100 V / 1,5 mΩ en parallèle | Une Schottky à 80 A dissiperait **40–60 W** : inexploitable. Le MOSFET « diode idéale » : ~2,5 W. **Mais rendre l'erreur physiquement impossible reste la meilleure protection.** |
| **Surtension / transitoires** | Busbar + | **TVS unidirectionnelle, V_standoff 45–48 V** (ex. 5KP45A / 5KP48A) entre + et − | ⚠️ **Compromis à assumer** : le standoff doit être **> 42 V** (sinon la TVS conduit en pleine charge et chauffe), mais la tension d'écrêtage d'une telle TVS est alors de **~72–80 V** — au-dessus de la tenue de condensateurs 63 V. Une TVS seule ne peut donc pas protéger des condensateurs 63 V sur un bus 42 V. Son rôle réel est d'écrêter les **grands** transitoires (coupure inductive, ESD) ; la protection contre les surtensions modérées repose sur la **capacité de bus** et sur le bridage du freinage régénératif (§H.6). |
| **Surtension** | Entrée de chaque ZS-X11H | **TVS SMCJ45A / SMCJ48A** + condensateur film 100 V / 100 nF | Protection locale, boucle courte. Même compromis que ci-dessus. |
| **Décharge du bus** | Busbar + / − | Résistance de bleed **10 kΩ / 1 W** permanente | 42 V → <12 V en ≈ 5·R·C = 5 × 10 000 × 0,010 = **500 s**. ❌ Trop lent ! → **1 kΩ / 5 W** → 50 s. Toujours trop lent. → **Décharge active** : MOSFET + 47 Ω / 50 W piloté par l'ESP32-SAFETY à l'extinction, ou accepter 50 s avec un voyant « BUS SOUS TENSION » |
| **Filtrage moteur** | Chaque ZS-X11H | Condensateur **470 µF / 63 V low-ESR** + 100 nF au plus près des bornes d'alimentation | Réduit l'ondulation renvoyée sur le bus et le rayonnement |
| **Filtrage capteurs** | Entrée 5 V de chaque capteur | Ferrite + 10 µF + 100 nF | Découplage local |
| **Défaut d'isolement** | — | Non requis à 42 V SELV | Un différentiel 30 mA **ne fonctionne pas en DC** (tore saturé) et serait inutile ici |

⚠️ Le point « décharge du bus » mérite attention : avec 10 mF de condensateurs, le bus reste dangereux pour le matériel (et douloureux pour l'outillage) plusieurs dizaines de secondes après coupure. **Un voyant LED alimenté par le bus lui-même** (LED + 10 kΩ) est le moyen le plus simple et le plus fiable de le signaler : tant qu'elle est allumée, on ne touche pas.

## D.9 Liaison BMS — précautions

```
   BMS M365 (JST PAP03)              ESP32-SAFETY
   ┌──────────────┐                  ┌──────────────┐
   │  T (TX BMS)  ├──► [ISOLATEUR] ──► RX2 (GPIO16) │
   │  R (RX BMS)  ◄── [  NUMÉRIQUE] ◄─┤ TX2 (GPIO17)│
   │  L (feu)     │    ADuM1201 ou    │              │
   └──────┬───────┘    optocoupleur   └──────────────┘
          │            haute vitesse
    pas de masse dédiée : le retour se fait par le négatif de puissance
```

🔴 **Trois points à vérifier avant de câbler (mesure M3)** :
1. **Niveau logique réel** (3,3 V présumé — non documenté).
2. **Le connecteur n'a pas de masse dédiée** : le retour passe par le négatif de puissance. Si l'ESP32 est référencé ailleurs, on crée une différence de potentiel sur une ligne de données. → **L'isolation galvanique n'est pas une précaution : c'est une nécessité.**
3. Le bus BMS **n'a ni chiffrement, ni authentification** : n'importe qui sur ce bus peut écrire dans les registres du BMS. On l'utilise en **lecture seule**, jamais en écriture.

**Implémentation logicielle** : l'ESP32-SAFETY interroge le registre `0x31` toutes les 500 ms (SOC, courant, tension pack, température) et le registre `0x40` toutes les 5 s (10 tensions de cellule). Les valeurs sont publiées sur CAN, puis dans ROS 2 en `sensor_msgs/BatteryState` + un message custom `retriever_msgs/CellVoltages`.

---

# E. Architecture des busbars

## E.1 Faut-il des busbars ?

Question légitime à 25 A nominal. Alternatives :

| Option | Avantages | Inconvénients | Verdict |
|---|---|---|---|
| Distribution par cosses empilées sur un boulon | Zéro fabrication | Empilement de 6 cosses = pression inégale, désassemblage total pour intervenir sur une branche, aucune évolutivité | ❌ |
| Bornier de distribution du commerce (Blue Sea 2105, Victron Lynx) | Sûr, capoté, prêt à l'emploi | 40–120 € ; nombre de postes figé | ✅ **Option de repli si Q6 = non** |
| **Busbar en cuivre plat, fabriqué** | Nombre de postes choisi, postes libres pour l'évolution, faible résistance, coût matière faible | Perçage, taraudage, étamage, capotage à faire | ✅ **Retenu** |

Le critère décisif est l'exigence explicite du cahier des charges : *« possibilité d'ajouter de nouveaux consommateurs sans recâbler »*. Un busbar avec 2 postes libres répond exactement à ça.

## E.2 Matériau — et pourquoi surtout pas du laiton

| Matériau | Conductivité (% IACS) | ρ (µΩ·cm) | Facteur vs Cu |
|---|---|---|---|
| **Cuivre Cu-ETP (C11000)** | **100 %** | 1,72 | **1,0×** ✅ |
| Aluminium 6101-T6 | 57 % | 3,0 | 1,75× |
| **Laiton CuZn37** | **~28 %** | ~6,2 | **3,6×** ❌ |
| Acier inox A2 | ~2,4 % | ~72 | ~42× ❌❌ |

⚠️ **Fait vérifié : Leroy Merlin ne vend pas de barre plate en cuivre.** Le rayon « profilé laiton » ne contient que des cornières 8×8×1 et 15×15×1, du rond Ø3 et du tube Ø6×0,5. Les rubriques « barre cuivre » du site renvoient à de la zinguerie de couverture.

Et de toute façon, **un busbar en laiton dissiperait 3,6× plus** : 10,4 W/m au lieu de 2,9 W/m à 100 A. → **Le cuivre doit être acheté hors GSB** (fournisseur électrique Rexel/Sonepar, ou Farnell/RS, ou barres préfabriquées marine).

**Finition** :

| Finition | R de contact | Corrosion | Verdict |
|---|---|---|---|
| Cuivre nu | La plus basse **à t=0** | ⚠️ Cu₂O/CuO se forme en continu et **l'oxyde est isolant** | Acceptable **uniquement** sous joint boulonné serré ; à proscrire en zone humide |
| **Étamé (Sn ≥ 12,7 µm)** | Légèrement supérieure | ✅ SnO₂ fin, se rompt sous pression | ✅ **Retenu** |
| Nickel | Plus élevée, **très sensible au couple** | ✅ | ❌ si le couple n'est pas maîtrisé |
| Argent + sous-couche Ni | La plus basse et la plus stable | ✅ | Optimum technique, ~4,5× le prix |

Une étude expérimentale publiée montre que **couple et revêtement expliquent ~86 % de la variance de résistance de joint**, que le nickel est le plus sensible au couple, et que **4 N·m donnent quasiment la même résistance que 9 N·m** sur du M6. Pour un robot (vibrations + humidité) : **étamé**, sans hésiter.

## E.3 Dimensionnement

Densité de courant admissible d'une barre plate en cuivre 🟡 :

| Condition | A/mm² |
|---|---|
| Convection naturelle, **coffret fermé** | 1,0 – 1,5 |
| Barre nue, bien ventilée | 1,8 – 2,5 |
| Air forcé | 3,0 – 4,0 |

Notre cas : coffret partiellement fermé, ventilé → viser **≤ 1,5 A/mm²** au courant crête.

| Barre | Section | Densité @83 A crête | R (mΩ/m @20 °C) | Pertes @25 A / @83 A |
|---|---|---|---|---|
| 15 × 2 | 30 mm² | 2,8 A/mm² | 0,57 | 0,36 / 3,9 W/m |
| **20 × 3** | **60 mm²** | **1,4 A/mm²** ✅ | **0,29** | **0,18 / 2,0 W/m** ✅ |
| 25 × 3 | 75 mm² | 1,1 A/mm² | 0,23 | 0,14 / 1,6 W/m |
| 30 × 5 | 150 mm² | 0,6 A/mm² | 0,11 | surdimensionné |

→ **Cu-ETP 20 × 3 mm étamé.** C'est aussi la largeur minimale qui permet un perçage M6 sans affaiblir la barre : règle de l'art **largeur ≥ 2,5 × Ø du trou** → 20 mm pour un M6 (Ø 6,5 mm perçé) est confortable.

## E.4 Architecture physique retenue

Deux barres identiques, **superposées sur deux plans différents** (jamais côte à côte à plat), séparées par un entretoise isolant.

```
   VUE DE DESSUS — BUSBAR + (Cu-ETP 20 × 3 × 220 mm étamé)
   ╔══════════════════════════════════════════════════════════════════╗
   ║ ⊕      ⊕      ⊕      ⊕      ⊕      ⊕      ⊕      ⊕      ⊕      ⊕ ║
   ╚══╤═══════╤══════╤══════╤══════╤══════╤══════╤══════╤══════╤══════╤═╝
      1       2      3      4      5      6      7      8      9     10
      │       │      │      │      │      │      │      │      │      │
    ENTRÉE  ZS-AVG ZS-AVD ZS-ARG ZS-ARD DC/DC  DC/DC  LIBRE  LIBRE  FIXATION
   (contacteur)                          12V_C  12V_A                 (isolée)
      │
      └─ 25 mm²

   Entraxe 20 mm · Ø perçage 6,5 mm (M6) · retrait bord 10 mm
   Longueur = 2×10 (bords) + 9×20 (entraxes) + 20 (fixation) = 220 mm

   ─────────── entretoise isolante PA6/POM 15 mm ───────────

   VUE DE DESSOUS — BUSBAR − (identique, décalé de 10 mm en X)
   ╔══════════════════════════════════════════════════════════════════╗
   ║ ⊖      ⊖      ⊖      ⊖      ⊖      ⊖      ⊖      ⊖      ⊖      ⊖ ║
   ╚══════════════════════════════════════════════════════════════════╝
      1       2      3      4      5      6      7      8      9     10
   RETOUR  ZS-AVG ZS-AVD ZS-ARG ZS-ARD DC/DC  DC/DC  MASSE  LIBRE  FIXATION
    PACK                                12V_C  12V_A  LOGIQUE
                                                       ★
   ★ La masse logique (5 V) arrive sur un poste DÉDIÉ et ÉLOIGNÉ des retours moteurs
```

**Pourquoi ce plan** :
- **Superposition et non juxtaposition** : un outil qui tombe ne peut pas ponter les deux polarités.
- **Décalage de 10 mm en X** entre les deux barres : les cosses ne se superposent pas verticalement, l'accès à la visserie reste possible barre par barre.
- Les **postes de puissance moteur sont groupés** (2–5) et **séparés des postes électroniques** (6–7) : le courant moteur ne traverse pas le chemin de retour de la logique.
- **Deux postes libres** (8–9) : critère d'évolutivité du cahier des charges.
- **Le poste de masse logique (−8) est le plus éloigné des retours moteurs** : c'est la position qui minimise la tension de mode commun.

## E.5 Spécification de fabrication

| Paramètre | Valeur | Justification |
|---|---|---|
| Matériau | **Cu-ETP C11000**, plat 20 × 3 mm | 100 % IACS |
| Finition | **Étamage** (électrolytique ou chimique, ≥ 12,7 µm) | Tenue à l'oxydation |
| Longueur | 220 mm × 2 | 10 postes |
| Perçage | Ø **6,5 mm**, entraxe **20 mm**, retrait bord 10 mm | M6 |
| Visserie | **Boulon M6 acier 8.8 zingué**, tête hexagonale | ⚠️ **Jamais d'inox A2 dans le chemin de courant** (ρ ≈ 42× le cuivre). Le boulon ne conduit pas : il **presse**. Le courant passe cosse↔barre. |
| Rondelles | **Rondelle plate large** (répartition) + **rondelle Belleville** | ⚠️ **Le cuivre flue à froid** : un boulon serré perd 20–40 % de sa précharge en quelques semaines. La Belleville absorbe le fluage. **Non négociable.** |
| Couple | **M6 → 9 N·m** 📐 (l'étude citée montre que 4 N·m suffisent électriquement ; 9 N·m assure la tenue mécanique en vibration) | Clé dynamométrique obligatoire |
| Frein filet | **Loctite 243** ou écrou Nylstop | Vibration |
| Marquage témoin | **Trait de peinture** vis/barre après serrage | Un contrôle visuel de 3 s détecte un desserrage |
| Recouple | **à 48 h, à 6 mois, puis annuel** | Fluage du cuivre |
| Recouvrement de cosse | ≥ largeur de la barre (20 mm) | Surface de contact |
| Résistance de joint cible | **< 50 µΩ** | À 25 A → 0,03 W |
| Capotage | **Capot polycarbonate transparent vissé**, ou gaine thermorétractable à colle laissant nues seulement les zones de contact | Contact accidentel, chute d'outil |
| Distance entre polarités | **≥ 15 mm** d'air 📐 | Les exigences normatives IEC 60664-1 à 42 V sont dérisoires (~1,25 mm de ligne de fuite en DP2/IIIa) — **elles ne sont pas le critère**. Le critère est **le corps étranger conducteur** : copeau, brin de câble, clé. À 42 V avec 800–1400 A disponibles, c'est une machine à souder. |

❌ **Ne jamais souder à l'étain un joint de puissance.** L'étain flue et fond à 183–227 °C : en cas de défaut, le joint se défait avant que le fusible ne fonde.

## E.6 Sertissage — le point de fabrication le plus critique

Un sertissage médiocre est **le mode de défaillance n°1** de ce type de montage.

| ❌ À proscrire | ✅ À faire |
|---|---|
| Pince à sertir manuelle de GSB (sertissage en « B », non contrôlé) | **Pince hydraulique à matrices hexagonales** (16 t), matrice adaptée à la section |
| Cosses laiton nickelé bas de gamme | **Cosses cuivre étamé** à collerette, calibre exact |
| Sertir puis « rattraper » avec de la soudure | Sertir uniquement ; la soudure crée une zone rigide qui casse en fatigue |
| Gaine thermo simple paroi | **Gaine thermorétractable double paroi à colle** (adhesive-lined) — étanchéité + reprise mécanique |
| Contrôle « ça a l'air bien » | **Test d'arrachement** sur une cosse sacrificielle + **thermographie sous charge** |

📐 Une pince hydraulique 16 t coûte ~60–120 €. C'est le meilleur rapport sécurité/prix de tout le projet. À défaut : faire sertir les 25 mm² chez un électricien ou un magasin de fourniture électrique (souvent gratuit à l'achat des cosses).

## E.7 Ce qui vient de Leroy Merlin, et ce qui n'en vient pas

### ✅ Utilisable en GSB

| Article | Usage | Réserve |
|---|---|---|
| Coffret étanche IP65 avec rail DIN | Boîtier de commande, ESP32, BMS | Vérifier le volume et la tenue thermique |
| Presse-étoupes M16/M20/M25 | Entrées de câbles | ⚠️ Vérifier la plage de serrage réelle vs Ø du 25 mm² — souvent trop petits |
| Gaine thermorétractable 2:1 | Repérage, isolation secondaire | ❌ Pas pour les cosses de puissance (prendre de la double paroi à colle ailleurs) |
| Gaine annelée ICTA / spiralée | Protection mécanique de faisceau | ✅ |
| Colliers Rilsan noirs UV + embases adhésives | Cheminement | Ne jamais serrer au point de déformer l'isolant |
| Visserie inox A2 M4/M5/M6 | **Fixation mécanique uniquement** | ❌ **Jamais dans le chemin de courant** |
| Rail DIN 35 mm | Support de composants **de signal** | Zingué → préférer inox si humidité |
| Borniers à vis / Wago 221 | Signal, **≤ 20 A** | ⚠️ Wago 221 = **32 A max, AC**. ❌ Interdit sur le bus |
| Plaque PVC / polycarbonate | Platine de montage, capots | Découpe facile |
| Tresse de masse cuivre étamée | Liaison châssis ↔ busbar − | Ampacité non spécifiée en GSB : sur-dimensionner |

### ❌ Liste rouge — à ne PAS acheter en GSB pour ce robot

| Interdit | Pourquoi | À la place |
|---|---|---|
| **Disjoncteur modulaire « 100 A » sur le bus** | La référence 100 A de tableau est un **sectionneur sans protection** ; Icu DC non marqué ; aucune tenue vibratoire ; un thermique 100 A ne protège aucun câble < 35 mm² | **MRBF 80 A / 58 V** + sectionneur DC marine |
| **Fusible ATO / MAXI sur le bus 42 V** | **32 V DC** — 42 V est 31 % au-dessus. L'arc ne s'éteint pas franchement : carbonisation, fusion du boîtier, incendie. AIC 1000 A < I_cc du pack | MRBF (58 V) ou MIDI/MEGA **version 58/80 V** |
| **Interrupteur différentiel 30 mA** | **Ne fonctionne pas en DC** (tore saturé). Et inutile à 42 V SELV | Rien (non requis) |
| **Dominos « sucre », barrettes** | Vis sur brins nus, fluage, aucun contrôle de couple | Cosses serties + boulon sur busbar |
| **Wago sur le bus de puissance** | 32 A max, marquage AC | Idem |
| **Câble H07V-U/R rigide** | Rupture par fatigue en vibration | Câble souple classe 5 **cuivre étamé** |
| **Barre / profilé laiton comme busbar** | 3,6× moins conducteur que le cuivre | Cu-ETP étamé, hors GSB |
| **Prises/fiches domestiques ou de jardinage** | Pas de détrompage DC, pas de tenue à l'arc, pas de contact pilote | Anderson SB, XT90-S |
| **Cosses + pince à sertir de GSB pour 25 mm²** | 0,5–2 mΩ = point chaud | Pince hydraulique hexagonale |

---

*Suite : `02-comms-esp32-moteurs.md` — sections F, G, H.*
