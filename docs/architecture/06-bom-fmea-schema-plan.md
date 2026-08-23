# S. Nomenclature (BOM)

⚠️ **Les prix sont des ordres de grandeur 📐 (France, août 2026), à confirmer.** Aucune référence produit précise n'est donnée quand je ne peux pas la vérifier ; à la place, je donne les **caractéristiques minimales à respecter**, ce qui est plus durable qu'une référence qui sera obsolète.

## S.1 Déjà possédé

| Élément | Qté | Fonction | Remarque |
|---|---:|---|---|
| Youyeetoo X1 SBC | 1 | Calculateur ROS 2 | ⚠️ Wiki officiel documente Ubuntu 22.04 ; 24.04 à valider |
| Batterie Xiaomi M365 | 1 | Source d'énergie | ⚠️ Courant max inconnu — **mesure M1** |
| ZS-X11H | 4 | Variateurs BLDC | ⚠️ **Mesures M4, M5, M12** avant tout branchement |
| Moteur-roue hoverboard 6,5" | 4 | Traction | 🔴 Non listés dans le matériel initial — **à confirmer que tu les as** |
| ESP32 | ≥3 | Contrôleurs temps réel | 🔴 **Question Q5** : modèle exact ? |
| YDLIDAR X4 | 1 | Lidar 2D | ❌ Intérieur uniquement |
| Kinect v2 | 1 | RGB-D | ❌ Intérieur uniquement, ⚠️ **mesure M9** |
| BNO085 | 1 | IMU 9 axes | ✅ Bon choix, à monter en SPI |
| GPS USB | 1 | Position absolue | 🔴 **Question Q4** : référence ? |
| ACS758LCB-100B | 1 | Mesure de courant | 🟡 Réaffecté à la protection, pas au SOC |
| Ventilateur PWM 12 V | 2 | Refroidissement | ✅ |
| Disjoncteur 100 A | 1 | — | ❌ **Non utilisé** — voir §D.3 |
| Coupe-batterie | 1 | Sectionnement | 🔴 **À caractériser** : est-il DC-rated ≥48 V ? Sinon → à remplacer |

## S.2 À acheter impérativement

Sans ces éléments, le robot ne peut pas être construit en sécurité.

### Protection et coupure

| Élément | Qté | Fonction | Spécification minimale | Prix 📐 | Où |
|---|---:|---|---|---:|---|
| Fusible MRBF + porte-fusible sur borne | 1 | Protection principale | **80 A, ≥58 V DC, AIC ≥2000 A @58 V** | 35 € | Marine/nautique |
| Fusibles MIDI 58 V + porte-fusibles | 4 | Protection variateurs | **25 A, ≥58 V DC** | 30 € | Marine/auto |
| Fusible MIDI 58 V + porte-fusible | 1 | DC/DC compute | **10 A, ≥58 V DC** | 8 € | idem |
| Fusible MIDI 58 V + porte-fusible | 2 | DC/DC aux + alim SAFETY | **5 A et 2 A, ≥58 V DC** | 14 € | idem |
| Sectionneur DC | 1 | Coupe-batterie | **≥48 V DC, ≥150 A continu**, rotatif à clé, corps étanche | 50 € | Marine |
| Contacteur / relais DC | 1 | Coupure commandée | **NO, ≥48 V DC, ≥100 A**, bobine 12 V | 45 € | Marine/auto/EV |
| **Champignon d'arrêt d'urgence** | 1 | Arrêt d'urgence | **Contact NF**, accrochage mécanique, Ø ≥40 mm, IP65 | 20 € | Électricité industrielle |
| Interrupteur à clé 2 positions | 1 | BANC/TERRAIN | 2 contacts NO, IP65 | 15 € | idem |
| Bouton poussoir « mise sous puissance » | 1 | Démarrage | NO, IP65, lumineux | 12 € | idem |

⚠️ **Rien de cette liste ne s'achète en GSB.** Ce sont des composants DC dont le pouvoir de coupure et la tension assignée en continu sont déclarés — c'est exactement ce qui manque au matériel de tableau domestique.

### Distribution

| Élément | Qté | Fonction | Spécification | Prix 📐 | Où |
|---|---:|---|---|---:|---|
| Barre cuivre Cu-ETP **étamée** 20 × 3 mm | 0,5 m | Busbars | **C11000, étamage ≥12,7 µm** | 30 € | Fournisseur électrique / Farnell / RS |
| Boulonnerie M6 acier 8.8 zingué | 20 | Fixation busbar | Tête hexa + écrou | 8 € | GSB ✅ |
| **Rondelles Belleville M6** | 20 | ⚠️ Compensation du fluage du cuivre | Acier ressort | 10 € | Fournisseur industriel |
| Rondelles plates larges M6 | 40 | Répartition | Inox ou acier zingué | 5 € | GSB ✅ |
| Entretoises isolantes PA6/POM 15 mm | 8 | Séparation des barres | M6 | 10 € | GSB ✅ |
| Plaque polycarbonate 3 mm | 0,25 m² | Capots busbar | Transparent | 15 € | GSB ✅ |

### Câblage

| Élément | Qté | Spécification | Prix 📐 | Où |
|---|---:|---|---:|---|
| Câble souple **cuivre étamé** 25 mm² rouge + noir | 2 × 1,5 m | Classe 5, isolant 105 °C, ISO 6722 ou marine | 45 € | Marine/auto |
| Câble souple étamé 6 mm² rouge + noir | 2 × 6 m | idem | 55 € | idem |
| Câble souple 2,5 / 1,5 / 1,0 mm² multicolore | 5 m chacun | Souple, 105 °C | 35 € | idem/GSB |
| Paire torsadée blindée 2 × 0,34 mm² + 2 × 0,5 | 5 m | CAN + alimentation | 15 € | Électronique |
| Câble blindé 5 × 0,25 mm² | 4 m | Hall | 12 € | idem |
| Cosses à œil M6 cuivre étamé 25 mm² | 6 | Sertissage | 12 € | Marine |
| Cosses à œil M6 cuivre étamé 6 mm² | 20 | idem | 18 € | idem |
| Cosses Faston isolées 6,3 mm | 24 | Phases moteur | 10 € | Auto |
| **Gaine thermorétractable double paroi à colle** | Assortiment | Étanchéité des cosses | 25 € | Marine/électronique |
| **Pince à sertir hydraulique 16 t** | 1 | ⚠️ **Sertissage 6 et 25 mm²** | 80 € | Outillage |
| Connecteurs Anderson SB50 rouge | 2 paires | Batterie | 25 € | Marine/auto |
| Connecteurs Molex Micro-Fit 4 pos | 10 | Bus CAN | 15 € | Électronique |
| Connecteurs JST-XH / PH assortis | lot | Capteurs, signaux | 20 € | Électronique |

### Électronique

| Élément | Qté | Fonction | Spécification | Prix 📐 |
|---|---:|---|---|---:|
| **Adaptateur USB-CAN gs_usb / candleLight** | 1 | Passerelle CAN | ⚠️ **Firmware candleLight/gs_usb impératif** (SocketCAN natif) | 30 € |
| Transceiver CAN SN65HVD230 / TCAN332 | 3 | Interface ESP32 ↔ CAN | 3,3 V | 12 € |
| DC/DC 36→12 V **isolé** | 1 | Rail COMPUTE | **In 20–60 V, out 12 V / 8 A, isolé, rendement ≥90 %** | 40 € |
| DC/DC 36→12 V | 1 | Rail AUX | In 20–60 V, out 12 V / 5 A | 20 € |
| DC/DC 12→5 V | 1 | Rail LOGIC | Out 5 V / 5 A | 12 € |
| DC/DC 36→5 V petit | 1 | Alim ESP32-SAFETY **amont contacteur** | In 20–60 V, out 5 V / 1 A | 10 € |
| MCP4728 (DAC 4 ch 12 bits I²C) | 2 | Consigne VR | | 12 € |
| AOP rail-to-rail double, alim 5 V | 2 | Ampli 0–3,3 → 0–5 V | MCP6002 ou équivalent | 5 € |
| ADS1115 (ADC 16 bits I²C) | 1 | ACS758 + tensions | | 8 € |
| 74LVC245 | 2 | Adaptation Hall 5 V → 3,3 V | Entrées 5 V tolérantes | 4 € |
| Isolateur numérique ADuM1201 | 1 | Liaison BMS | 2 canaux, ≥1 Mbit/s | 8 € |
| **Watchdog externe TPL5010** ou équivalent | 1 | Surveillance ESP32-SAFETY | Période ~1 s, sortie reset | 6 € |
| Résistance bobinée 10 Ω / 25 W | 1 | Précharge | ⚠️ **Tenue impulsionnelle ≥50 J** | 8 € |
| Relais signal 12 V / 10 A | 1 | Précharge | | 5 € |
| TVS 5KP45A ou 5KP48A | 1 | Bus | Unidirectionnel, **V_standoff > 42 V** | 4 € |
| TVS SMCJ45A ou SMCJ48A | 4 | Entrée variateur | Unidirectionnel, V_standoff > 42 V | 5 € |
| Condensateurs 470 µF / 63 V low-ESR | 4 | Filtrage variateur | ⚠️ **63 V minimum** | 12 € |
| MOSFET 2N7002 + résistances | lot | Sorties collecteur ouvert | | 6 € |
| NTC 10 kΩ + câbles | 3 | Températures | | 8 € |
| LED RGB haute luminosité + buzzer | 1 | Signalisation | | 10 € |
| LED rouge + résistance 10 kΩ / 1 W | 1 | ⚠️ Témoin « bus sous tension » | | 2 € |
| Hub USB 3.0 **alimenté** | 1 | Extension USB | Alimentation externe 5 V obligatoire | 25 € |
| Ferrites à clipser | lot | Filtrage USB/CAN | | 12 € |

### Mécanique et coffret

| Élément | Qté | Spécification | Prix 📐 | Où |
|---|---:|---|---:|---|
| Coffret IP65 + rail DIN | 1–2 | Selon volume | 40 € | GSB ✅ |
| Presse-étoupes M16/M20/M25 | 12 | ⚠️ Vérifier la plage pour le 25 mm² | 20 € | GSB ✅ |
| Plaque alu 3–5 mm | 0,2 m² | Dissipateur variateurs | 30 € | GSB ✅ |
| Pâte thermique | 1 | | 8 € | Informatique |
| Sangles + mousse EVA | — | Fixation du pack | 20 € | GSB ✅ |
| Gaine annelée + colliers UV | lot | Cheminement | 25 € | GSB ✅ |
| Étiqueteuse thermorétractable ou étiquettes | 1 | Repérage | 30 € | Électricité |

**Sous-total impératif : ≈ 1 090 €** 📐

## S.3 À acheter recommandé

Améliore fortement la sécurité, la fiabilité ou la maintenance.

| Élément | Qté | Fonction | Prix 📐 | Priorité |
|---|---:|---|---:|---|
| **Thermomètre infrarouge** | 1 | ⚠️ Contrôle des jonctions — **le meilleur rapport sécurité/prix du projet** | 30 € | ★★★ |
| **Extincteur classe D ou seau de sable sec** | 1 | Feu de lithium | 60 € | ★★★ |
| **Sac / caisse ignifuge LiPo** | 1 | Stockage du pack | 25 € | ★★★ |
| **Clé dynamométrique 2–25 N·m** | 1 | Couples de busbar reproductibles | 45 € | ★★★ |
| **Alimentation de laboratoire 0–60 V / 5 A à limitation de courant** | 1 | ⚠️ **Indispensable pour tous les tests de niveau 0 et 1** | 130 € | ★★★ |
| Pince ampèremétrique **DC** | 1 | Mesures de courant | 60 € | ★★ |
| Multimètre correct (True RMS, µA) | 1 | | 60 € | ★★ |
| Oscilloscope 2 voies ≥50 MHz | 1 | Mesures M3, M4, M7 ; debug CAN et PWM | 250 € | ★★ |
| E-stop **radio** (émetteur + récepteur, contact NF) | 1 | Arrêt d'urgence à distance | 90 € | ★★★ |
| SSD NVMe M.2 500 Go | 1 | Bags, cartes, système | 45 € | ★★ |
| Deuxième pack batterie compatible | 1 | Autonomie, redondance | ~90 € | ★★ |
| Chargeur 42 V / 2 A de qualité | 1 | | 30 € | ★★ |
| Antenne Wi-Fi déportée à gain | 2 | Portée | 25 € | ★ |
| Onduleur / alim de banc 12 V pour le X1 | 1 | Développement hors batterie | 25 € | ★★ |
| Roulettes folles + châssis de banc | 1 | Tests roues levées | 40 € | ★★ |

**Sous-total recommandé : ≈ 1 005 €** 📐 (dont ~440 € d'instrumentation réutilisable pour tous tes projets)

## S.4 Optionnel — améliorations futures

| Élément | Fonction | Prix 📐 | Phase |
|---|---|---:|---|
| **Lidar 2D extérieur IP65** (immunité solaire spécifiée) | Remplacement du X4 en extérieur | 400–1500 € | 4 |
| **GNSS RTK** (base + rover, ou abonnement NTRIP) | Précision 2–5 cm au lieu de 2–5 m | 300–600 € | 4 |
| **GNSS double antenne avec heading** | ⚠️ Résout le problème du cap (§N.3) | 400–800 € | 4 |
| Caméra stéréo extérieure | Remplacement du Kinect | 250–400 € | 4 |
| **Frein mécanique à manque de courant** ×2 | ⚠️ Tient une pente robot hors tension | 150 € | 4 |
| **Hacheur de freinage** (MOSFET + résistance de puissance) | Absorbe l'énergie de régénération | 40 € | 3–4 |
| Shunt 500 µΩ + INA228 | Mesure de courant à 0,1 % | 25 € | 3 |
| BMS de remplacement à seuils documentés | ⚠️ Lève l'incertitude M1 | 60 € | 3 |
| Pack batterie plus capacitaire (48 V, 20 Ah) | ⚠️ **Résout le problème de fond du §A.1-5** | 400 € | 4 |
| Variateurs BLDC pilotables (VESC, ODrive) | Contrôle en couple, télémétrie riche, freinage maîtrisé | 4 × 90 € | 4 |
| Caméra thermique smartphone | Diagnostic avancé | 250 € | 3 |
| Boîtier IP67 à refroidissement par conduction | Usage tout temps | 200 € | 4 |

## S.5 Résumé budgétaire

| Poste | Montant 📐 |
|---|---:|
| Impératif | 1 090 € |
| Recommandé | 1 005 € |
| **Total pour un robot sûr et instrumenté** | **≈ 2 100 €** |
| Optionnel (phase 4) | 1 500 – 4 000 € |

⚠️ **Répondant explicitement à la contrainte du cahier des charges** : les éléments critiques pour la sécurité (protections DC, sectionneur, cuivre, cosses, sertissage) **ne sont pas disponibles en grande surface de bricolage**, et là où ils semblent l'être (disjoncteur 100 A, fusibles à lame, dominos), le produit n'a pas les caractéristiques DC requises. **La sécurité électrique prime : ces éléments doivent venir de fournisseurs marine, automobile ou industriels.**

Ce que Leroy Merlin Mérignac fournit très bien : coffrets IP65, presse-étoupes, visserie, plaques polycarbonate et alu, gaines, colliers, entretoises, rail DIN, outillage. Soit environ **200 €** sur les 1 090 € impératifs.

---

# T. Analyse des risques (FMEA simplifiée)

**Échelles** — Probabilité : 1 = très improbable … 5 = fréquent · Gravité : 1 = négligeable … 5 = catastrophique (blessure, incendie) · Détection : 1 = détecté immédiatement … 5 = indétectable
**Criticité C = P × G × D** (max 125). Seuil d'action retenu : **C > 24** 📐

| # | Risque | P | G | D | **C** | Détection | Mitigation |
|---:|---|:-:|:-:|:-:|:-:|---|---|
| 1 | **Emballement thermique du pack Li-ion** | 2 | **5** | 3 | **30** | Température BMS, Δ cellules, odeur, gonflement | Surveillance BMS (T° + 10 cellules), seuils stricts, pack sanglé et isolé thermiquement, ventilation séparée, **stockage en sac ignifuge**, extincteur classe D, ⚠️ **historique du Δ cellules** |
| 2 | **Court-circuit franc du bus (kA)** | 2 | **5** | 1 | **10** | Fusible MRBF, chute de V_bus | MRBF 80 A / 58 V à ≤175 mm de la borne, AIC 2000 A, busbars **superposés et capotés**, aucune polarité nue |
| 3 | **Jonction dégradée → point chaud → incendie** | **4** | **5** | **4** | **80** ⚠️ | ⚠️ Difficile en direct. Signature : R apparente du bus qui augmente | ⚠️ **LE RISQUE N°1.** Rondelles Belleville, cuivre **étamé**, sertissage hydraulique, couple contrôlé + marque témoin, **recouple à 48 h / 6 mois**, **thermographie toutes les 50 h**, surveillance ΔV/ΔI |
| 4 | **Fusible à lame 32 V sur bus 42 V ne coupe pas** | 3 | **5** | 3 | **45** ⚠️ | Après coup (carbonisation) | ⚠️ **Aucun fusible ATO/MAXI sur le bus.** Uniquement MRBF/MIDI **58 V**. Les ATO 32 V restent admis **après** le DC/DC 12 V |
| 5 | **Arc entretenu sur connecteur desserré** | 3 | **5** | 4 | **60** ⚠️ | Odeur, bruit, thermographie | Reprise mécanique de tous les connecteurs, frein filet, contrôle visuel avant chaque sortie, connecteurs à forte pression (Anderson) |
| 6 | **Inversion de polarité** | 2 | 4 | 1 | **8** | Immédiate (rien ne fonctionne) | **Connecteurs détrompés + code couleur** (l'erreur devient physiquement impossible) |
| 7 | **Appel de courant sans précharge → contacts soudés** | 3 | 4 | 2 | **24** | Contacteur qui ne s'ouvre plus | Séquence de précharge obligatoire avec timeout, **détection de contacteur collé** (V_bus vs commande) |
| 8 | **BMS coupe en pleine manœuvre** | **4** | 3 | 2 | **24** | Perte totale immédiate | ⚠️ **Budget de courant 20 A / 30 A crête** appliqué par l'ESP32-SAFETY, mesure ACS758 à 500 Hz, bridage progressif dès 22 A. **Mesure M1 impérative** |
| 9 | **Surtension par freinage régénératif** | 3 | 4 | 2 | **24** | ACS758 négatif, V_bus > 43 V | Mesure bidirectionnelle, bridage du freinage au-dessus de 41 V, limite de décélération, ⚠️ raccordement BMS côté `P-` |
| 10 | **Les condensateurs des ZS-X11H sont en 50 V** | 3 | 4 | 2 | **24** | Inspection visuelle | ⚠️ **Mesure M12 avant toute mise sous tension.** Si 50 V → TVS + condensateurs 100 V, ou renoncer à charger le pack au-delà de 40 V |
| 11 | **Perte du X1 (crash, OOM, gel)** | **4** | 3 | 1 | **12** | Heartbeat matériel + CAN | Watchdogs N3/N4, arrêt en 300 ms. ⚠️ **Le X1 n'est pas le seul à pouvoir arrêter les moteurs** |
| 12 | **Perte d'un ESP32** | 3 | 3 | 1 | **9** | Heartbeat CAN | WDT interne + externe, état sûr au reset, `/SAFE` actif pendant le reset |
| 13 | **Bus CAN en défaut** | 3 | 3 | 1 | **9** | Compteurs CAN, bus-off | Paire torsadée blindée, terminaisons correctes, `restart-ms 100`, chaque nœud a son watchdog local |
| 14 | **Bug ROS 2 → commande dangereuse** | **4** | 4 | 3 | **48** ⚠️ | Plausibilité, limites firmware | ⚠️ **7 filtres successifs** (§L.2), limites `constexpr` firmware non contournables, CRC8 + numéro de séquence, budget de courant |
| 15 | **Fils de phase moteur inversés** | 3 | 3 | 1 | **9** | Test de sens de rotation [P5] | Test automatique roues levées, 4 s, à chaque boot en mode BANC |
| 16 | **Entrée VR flottante (fil coupé)** | 2 | **5** | 4 | **40** ⚠️ | ⚠️ Difficile : comportement indéfini | ⚠️ **Résistance 10 kΩ de rappel à la masse sur chaque entrée VR**, au plus près du variateur. Transforme la rupture en consigne nulle |
| 17 | **Moteur bloqué → surintensité → surchauffe** | 3 | 3 | 2 | **18** | Comptes Hall = 0 avec consigne ≠ 0 | Détection en 300 ms, coupure de la roue, fusible 25 A |
| 18 | **Perte du lidar en navigation** | 3 | 3 | 1 | **9** | `source_timeout` du collision monitor | Arrêt automatique, `DEGRADED`, vitesse plafonnée |
| 19 | **Covariance GPS nulle → EKF diverge** | 3 | 3 | **4** | **36** ⚠️ | ⚠️ Sournois : le robot part n'importe où | Test [P4] : covariance nulle = **FAIL**. Nœud de calcul depuis le HDOP si le driver ne la fournit pas |
| 20 | **Cap magnétique faux (perturbation moteur)** | **5** | 3 | **4** | **60** ⚠️ | ⚠️ Le robot navigue « bien » mais dans la mauvaise direction | ⚠️ **BNO085 sur mât ≥25 cm au-dessus des busbars**, calibration hard/soft iron, **initialisation du cap par le déplacement GPS**, évaluer Game Rotation Vector (dérive 0,5°/min) vs magnétomètre |
| 21 | **Patinage skid-steer → odométrie fausse** | **5** | 2 | 3 | **30** | Divergence odométrie/GPS | ⚠️ **Ne pas fusionner `vyaw` des roues** — utiliser le gyro. Étalonnage `wheel_separation_multiplier`. Fusion GPS |
| 22 | **Divergence firmware / logiciel** | **4** | 3 | 3 | **36** ⚠️ | Hash de protocole dans le heartbeat | ⚠️ `protocol.yaml` **source unique**, génération automatique, CI qui vérifie, refus d'armement en cas de divergence |
| 23 | **Champignon défaillant (contact oxydé)** | 2 | **5** | **5** | **50** ⚠️ | ⚠️ **Indétectable sans test actif** | ⚠️ **Test actif obligatoire à chaque session** [P6] : l'opérateur appuie, le système vérifie la transition. Contact **NF** (une rupture de fil = sécurité) |
| 24 | **Contacteur collé** | 2 | **5** | 3 | **30** | V_bus reste haute après commande d'ouverture | Détection [P6], alerte `CRITICAL`, buzzer continu, consigne d'ouvrir le coupe-batterie |
| 25 | **Bruit électrique → capteurs erratiques** | **4** | 2 | 3 | **24** | Valeurs aberrantes, erreurs CAN | Séparation des cheminements ≥10 cm, torsadage, blindage à un seul point, ferrites, DC/DC compute **isolé**, masse en étoile |
| 26 | **Alimentation instable → reboots** | 3 | 3 | 2 | **18** | Journaux, compteurs de reset | DC/DC dimensionnés ×2, condensateurs de découplage, rails surveillés par [P2] |
| 27 | **Robot en roue libre après coupure, en pente** | 3 | 4 | 2 | **24** | Observation | ⚠️ **Boucle e-stop à deux temps** : freinage électrique d'abord, contacteur ~1 s après. Évolution : frein mécanique à manque de courant |
| 28 | **Basculement du robot** | 2 | 3 | 2 | **12** | IMU : angle > 40° | Arrêt automatique, centre de gravité bas, garde au sol maîtrisée |
| 29 | **Perte réseau en téléopération** | **4** | 3 | 1 | **12** | Keepalive WireGuard | `STOPPING` en 500 ms |
| 30 | **Exposition de ROS 2 sur Internet** | 2 | 4 | 3 | **24** | Audit | ⚠️ **DDS confiné à localhost**, WireGuard seul port ouvert, nftables DROP par défaut, DDS bloqué même sur le VPN |
| 31 | **Disque plein → arrêt de la journalisation** | **4** | 2 | 2 | **16** | `diagnostic_common_diagnostics` | Rotation automatique, purge, `FAIL` à moins de 2 Go |
| 32 | **Le Kinect ne fonctionne pas (contrôleur ASMedia)** | 3 | 1 | 1 | **3** | Mesure M9 | Vérification avant toute intégration ; le Kinect n'est jamais critique |
| 33 | **Ubuntu 24.04 non supporté par le X1** | 2 | 4 | 1 | **8** | Test de boot | ⚠️ **À valider en tout premier (V1).** Repli : Ubuntu 22.04 + ROS 2 Humble, ou conteneur Docker Jazzy sur 22.04 |
| 34 | **Le driver YDLIDAR ne compile pas sur Jazzy** | 3 | 2 | 1 | **6** | Test de build | Pas de release binaire Jazzy ; build source depuis la branche `humble` (recommandation officielle YDLIDAR) |

## T.1 Les six risques prioritaires (C > 40)

| Rang | # | Risque | C | Action immédiate |
|---:|---:|---|:-:|---|
| **1** | 3 | Jonction dégradée → point chaud → incendie | **80** | Sertissage hydraulique + Belleville + étamage + couple contrôlé + **thermographie planifiée**. Non négociable. |
| **2** | 5 | Arc entretenu sur connecteur desserré | **60** | Reprise mécanique systématique, frein filet, inspection avant chaque sortie |
| **2** | 20 | Cap magnétique faux | **60** | Mât IMU ≥25 cm, calibration, init de cap par déplacement GPS |
| **4** | 23 | Champignon défaillant | **50** | Test actif obligatoire à chaque session |
| **5** | 14 | Bug ROS 2 → commande dangereuse | **48** | 7 filtres, limites firmware `constexpr`, budget de courant |
| **6** | 4 | Fusible 32 V sur bus 42 V | **45** | Aucun ATO/MAXI sur le bus. MRBF/MIDI 58 V uniquement |

⚠️ **Observation importante** : les deux risques les plus critiques (#3 et #5) sont des risques **de fabrication mécanique**, pas des risques logiciels. C'est contre-intuitif pour un projet perçu comme « robotique », mais c'est la réalité de tous les systèmes de puissance DC. **Le temps passé sur le sertissage et le serrage est plus rentable, en sécurité, que le temps passé sur l'architecture logicielle.**

---

# U. Schéma global final

```
╔══════════════════════════════════════════════════════════════════════════════╗
║                        POSTE OPÉRATEUR (distant)                             ║
║   Foxglove Studio (WebSocket:8765) · SSH · page d'état HTTP · manette         ║
╚═══════════════════════════════════╤══════════════════════════════════════════╝
                                    │  WireGuard UDP:51820  — seul port ouvert
                                    │  nftables DROP par défaut · DDS bloqué
╔═══════════════════════════════════▼══════════════════════════════════════════╗
║  YOUYEETOO X1 · Ubuntu 24.04 · ROS 2 Jazzy          [ NON TEMPS RÉEL ]        ║
║                                                                              ║
║  ┌─ SUPERVISION ──────────────────────────────────────────────────────────┐  ║
║  │ mission_manager · retriever_safety_bridge (MIROIR, sans autorité)      │  ║
║  │ retriever_power_monitor · retriever_selftest · diagnostic_aggregator   │  ║
║  │ foxglove_bridge · rosbag2 (buffer pré-défaut 60 s)                     │  ║
║  └────────────────────────────────────────────────────────────────────────┘  ║
║  ┌─ NAVIGATION ───────────────────────────────────────────────────────────┐  ║
║  │ Nav2 : planner · controller (MPPI + rotation_shim) · bt_navigator      │  ║
║  │ velocity_smoother → collision_monitor (VelocityPolygon) → twist_mux    │  ║
║  └────────────────────────────────────────────────────────────────────────┘  ║
║  ┌─ LOCALISATION ─────────────────────────────────────────────────────────┐  ║
║  │ ekf_odom (odom→base_link) · navsat_transform · ekf_map (map→odom)      │  ║
║  │ slam_toolbox (intérieur, phases 1–2 uniquement)                        │  ║
║  └────────────────────────────────────────────────────────────────────────┘  ║
║  ┌─ MATÉRIEL ─────────────────────────────────────────────────────────────┐  ║
║  │ controller_manager @100 Hz + retriever_hardware (SocketCAN)            │  ║
║  │   └ diff_drive_controller (4 roues : 2G / 2D)                          │  ║
║  │ retriever_can_bridge · drivers lidar / GPS / (Kinect)                  │  ║
║  └────────────────────────────────────────────────────────────────────────┘  ║
╚═══╤═══════════════════╤═══════════════════════════════════╤══════════════════╝
    │ USB 3.0 #1        │ USB 3.0 #2 → hub alimenté         │ GPIO
    │                   │   ├── YDLIDAR X4 (+5 V externe ⚠️) │ heartbeat 10 Hz
┌───▼────────┐          │   ├── GPS USB                     │ (indépendant du CAN)
│ Kinect v2  │          │   └── USB-CAN gs_usb              │
│ ⚠️ 12V/32W │          │        │                          │
│ secteur    │          │        ▼                          │
│ intérieur  │      ════╧════════╪══════ CAN 500 kbit/s ════╧═══════════════════
└────────────┘               ╤       ╤            ╤
                             │       │            │
            ┌────────────────▼──┐ ┌──▼─────────┐ ┌▼──────────────┐
            │  ESP32-SAFETY     │ │ESP32-MOT-AV│ │ESP32-MOT-AR   │
            │  ★ AUTORITÉ ★     │ │            │ │               │
            │  FSM sécurité 1kHz│ │ PI 200 Hz  │ │ PI 200 Hz     │
            │  alimenté EN AMONT│ │ ×2 roues   │ │ ×2 roues      │
            │  du contacteur    │ │            │ │               │
            └─┬──┬──┬──┬──┬──┬──┘ └──┬──┬──────┘ └──┬──┬─────────┘
              │  │  │  │  │  │       │  │           │  │
       BNO085 │  │  │  │  │  └ventilos│ └Hall×4     │  └Hall×4
        (SPI) │  │  │  │  └ACS758     └DAC+GPIO     └DAC+GPIO
   BMS UART ⚡┘  │  │  └V_bus/V_pack     │              │
   contacteur ───┘  │                    ▼              ▼
   précharge ───────┘              ZS-X11H ×2      ZS-X11H ×2
              │                          │              │
        ┌─────▼──────┐             Moteur-roue    Moteur-roue
        │ /SAFE ─────┼──────────────────► entrées EL + STOP des 4 variateurs
        │ (actif bas,│                    (sécurité positive)
        │  coll. ouv)│
        └────────────┘
   ┌──────────────────────┐
   │ 🔴 CHAMPIGNON (NF)   ├──► /SAFE directement (freinage immédiat)
   │                      ├──► bobine contacteur, via RC ~1 s
   │ ⏱ WATCHDOG TPL5010   ├──► reset ESP32-SAFETY + /SAFE
   └──────────────────────┘

╔══════════════════════════ CHAÎNE DE PUISSANCE ═══════════════════════════════╗
║                                                                              ║
║  ┌────────────┐  ① coupe-  ② MRBF   ③ ACS758  ④ CONTACTEUR                  ║
║  │ PACK M365  │   batterie   80 A     100 A      DC 100 A                   ║
║  │ 10S · 280Wh├──►  DC   ──► 58 V ──► bidir. ──►   NO    ──┬──► BUSBAR +     ║
║  │ 30–42 V    │    48 V     AIC2kA              ▲         │   Cu 20×3 étamé ║
║  │ BMS ⚡UART │                                  │         │   10 postes     ║
║  └────────────┘                        ⑤ précharge         │   2 libres      ║
║                                         10 Ω / 25 W        │                 ║
║                                         + timeout 1,5 s    │                 ║
║  ┌─────────┬─────────┬─────────┬─────────┬─────────┬───────┴─────┐           ║
║ MIDI25   MIDI25   MIDI25   MIDI25     MIDI10    MIDI5      MIDI2            ║
║  │         │         │         │         │         │           │            ║
║ ZS-X11H  ZS-X11H  ZS-X11H  ZS-X11H   DC/DC12V  DC/DC12V  DC/DC 5V           ║
║  AV-G     AV-D     AR-G     AR-D     COMPUTE     AUX     ★SAFETY★           ║
║  │         │         │         │      (isolé)     │      (amont             ║
║  ▼         ▼         ▼         ▼         │        ├─►ventilos contacteur)   ║
║ Moteur   Moteur   Moteur   Moteur      X1 +      ├─►bobine                  ║
║ 6,5"     6,5"     6,5"     6,5"       hub USB    └─►DC/DC 5V LOGIC          ║
║  │         │         │         │         │             │                    ║
║  └─────────┴─────────┴─────────┴─────────┴─────────────┴──► BUSBAR −         ║
║                                              = MASSE ÉTOILE UNIQUE          ║
║                                                                              ║
║  Budget : 20 A continu / 30 A crête 2 s (imposé par ESP32-SAFETY)           ║
║  ⚠️ La motorisation pourrait tirer 80 A — la batterie ne peut pas suivre.   ║
╚══════════════════════════════════════════════════════════════════════════════╝

  CHAÎNE D'ARRÊT, du plus rapide au plus autoritaire :
  N1 collision_monitor ~100ms │ N2 ros2_control ~50ms │ N3 ESP32-MOT 150ms
  N4 ESP32-SAFETY 20ms │ N5 CHAMPIGNON MATÉRIEL 10ms │ N6 coupe-batterie
  ★ Seul N5/N6 sont indépendants de tout logiciel ★
```

---

# V. Plan de réalisation par étapes

Chaque phase a un **critère de sortie mesurable**. On ne passe pas à la suivante sans l'avoir atteint.

## Phase 0 — Lever les incertitudes (avant tout achat de puissance)

**Durée 📐 : 1–2 semaines** · **Coût : ~200 €** (instrumentation)

| # | Tâche | Critère de sortie |
|---|---|---|
| V0.1 | Répondre aux **7 questions du §B.3** | Toutes documentées dans `docs/measurements/` |
| V0.2 | Acheter l'instrumentation de base : alimentation de labo à limitation de courant, multimètre, thermomètre IR | Disponible sur l'établi |
| V0.3 | **M12** — marquage des condensateurs des 4 ZS-X11H | ≥63 V confirmé, ou plan B décidé |
| V0.4 | **M5** — état du cavalier J1 | Documenté (n'affecte pas le choix DAC, mais informe) |
| V0.5 | **M9** — `lspci` sur le X1 : contrôleur USB 3.0 | Intel/NEC = OK Kinect · ASMedia = Kinect abandonné |
| V0.6 | **Installer Ubuntu 24.04 + ROS 2 Jazzy sur le X1** ⚠️ risque #33 | Boot, réseau, USB, UART, GPIO fonctionnels. Sinon : repli 22.04 + Humble, décision prise ici |
| V0.7 | **M2** — tension et résistance interne du pack | I_cc calculé → calibre de fusible confirmé |
| V0.8 | **M1** — ⚠️ seuil de coupure du BMS, sur banc protégé | Budget de courant recalé. **Si < 25 A → décision : 2 roues motrices ou changement de pack** |

⚠️ **V0.8 est le jalon le plus important du projet.** Tout le dimensionnement de puissance en dépend, et un résultat défavorable change l'architecture mécanique (4WD → 2WD) ou impose un nouveau pack. Mieux vaut le savoir maintenant.

## Phase 1 — Chaîne logicielle sans matériel

**Durée 📐 : 2–3 semaines** · **Coût : 0 €** · ⚠️ **Peut se faire en parallèle de la phase 0**

| # | Tâche | Critère de sortie |
|---|---|---|
| V1.1 | Créer le dépôt, l'arborescence, la CI | `colcon build` vert en CI |
| V1.2 | Écrire `protocol.yaml` + le générateur | `protocol.hpp` et `protocol.h` générés, tests unitaires passants |
| V1.3 | `retriever_msgs` | Compile |
| V1.4 | `retriever_description` : URDF, `physical_params.yaml` | `check_urdf` OK, TF complet dans RViz |
| V1.5 | `retriever_hardware` en mode mock puis loopback `vcan0` | Test d'intégration passant |
| V1.6 | `retriever_bringup` : lancement mock complet | Tous les nœuds actifs en < 20 s |
| V1.7 | Nav2 + EKF en simulation Gazebo | Le robot atteint un objectif à 10 m |
| V1.8 | Tests de niveau 5 automatisés | 100 % en CI |

**Critère de sortie de phase** : `ros2 launch retriever_bringup retriever.launch.py use_sim:=true` amène le robot simulé à naviguer vers un objectif. La totalité de la pile logicielle est validée **avant** qu'un seul fil de puissance ne soit serti.

## Phase 2 — Électronique basse tension

**Durée 📐 : 2–3 semaines** · **Coût : ~350 €**

| # | Tâche | Critère de sortie |
|---|---|---|
| V2.1 | Assembler les 3 ESP32 + transceivers CAN sur plaque d'essai | Bus CAN fonctionnel entre les 3, 0 erreur en 1 h |
| V2.2 | Adaptateur USB-CAN sur le X1 | `can0` UP, `candump` reçoit les trames des ESP32 |
| V2.3 | Firmware squelette : FSM, heartbeat, watchdogs | Tests de niveau 2 passants |
| V2.4 | BNO085 en SPI sur ESP32-SAFETY → CAN → `/imu/data` | 100 Hz, quaternion normé, test des 6 faces OK |
| V2.5 | MCP4728 + AOP : rampe 0–5 V vérifiée à l'oscilloscope | Linéaire, 0,00–5,00 V |
| V2.6 | ACS758 + ADS1115 : mesure sans courant | 2,50 V ± 0,05 |
| V2.7 | **M3** puis liaison BMS isolée | Registre 0x31 lu, SOC plausible |
| V2.8 | Ventilateurs PWM + tachy | Courbe thermique fonctionnelle |
| V2.9 | `retriever_can_bridge` : trames → topics ROS | `/imu/data`, `/battery_state`, `/retriever/safety_state` publiés |

**Critère de sortie** : le X1 voit l'IMU, la batterie et l'état de sécurité en ROS 2, sans aucun câble de puissance branché.

## Phase 3 — Chaîne de puissance, sans moteurs

**Durée 📐 : 2–3 semaines** · **Coût : ~600 €** · ⚠️ **Phase la plus dangereuse**

| # | Tâche | Critère de sortie |
|---|---|---|
| V3.1 | Fabriquer les busbars (perçage, étamage, capot) | Résistance de joint < 50 µΩ mesurée |
| V3.2 | Confectionner tous les câbles de puissance (sertissage hydraulique) | Tests de niveau 0 passants, test d'arrachement OK |
| V3.3 | Monter fusibles, sectionneur, contacteur, précharge | Câblage vérifié **deux fois**, par écrit |
| V3.4 | ⚠️ **Tests de niveau 1 à l'alimentation de laboratoire** (12 → 42 V, limitée en courant) | Tous les rails conformes, aucun courant anormal |
| V3.5 | Séquence de précharge + test de timeout sur court-circuit contrôlé | `FAULT_PRECHARGE` en < 1,5 s |
| V3.6 | ⚠️ **Première connexion du pack**, à 50 % de SoC, à l'extérieur, extincteur à portée | Courant au repos < 1,5 A |
| V3.7 | Chaîne d'arrêt d'urgence complète (champignon → `/SAFE` → contacteur RC) | Test actif : transition observée |
| V3.8 | Thermographie après 30 min | ΔT < 15 K entre jonctions identiques |
| V3.9 | Recouple à 48 h | Documenté |

**Critère de sortie** : le robot est sous tension, tous les rails sont bons, le champignon fonctionne, la thermographie est propre. **Aucun moteur n'est encore branché.**

## Phase 4 — Motorisation, roues levées

**Durée 📐 : 2–3 semaines** · **Coût : ~150 €**

| # | Tâche | Critère de sortie |
|---|---|---|
| V4.1 | **M4** — polarité `EL`/`STOP`, un moteur sur établi, alim de labo 24 V / 3 A | Documentée, temps d'arrêt mesuré |
| V4.2 | **M6, M7** — comptage Hall, niveau `SC` | 90 transitions/tour confirmées |
| V4.3 | Câblage Hall (74LVC245, RC, blindage) sur un moteur | PCNT compte dans le bon sens |
| V4.4 | Boucle PI de vitesse sur un moteur | Suivi < 10 %, réponse < 200 ms |
| V4.5 | Extension aux 4 moteurs, robot sur cales | Les 4 roues tournent, sens correct |
| V4.6 | Budget de courant + bridage progressif | Vérifié en chargeant artificiellement une roue |
| V4.7 | Tests de niveau 4 complets | Tous passants |
| V4.8 | Tests de niveau 6 (roues levées) : coupure du X1, du CAN, d'un ESP32, champignon | Arrêt < 500 ms dans tous les cas |

⚠️ **V4.8 est le jalon de sécurité.** Tant que ces tests ne passent pas, le robot ne touche pas le sol.

## Phase 5 — Premiers déplacements

**Durée 📐 : 1–2 semaines**

| # | Tâche | Critère de sortie |
|---|---|---|
| V5.1 | Roues au sol, **robot retenu par une sangle**, v ≤ 0,3 m/s, téléop | Comportement conforme |
| V5.2 | Libre, sol plat, v ≤ 0,5 m/s | Ligne droite sur 10 m, écart < 30 cm |
| V5.3 | Étalonnage `wheel_separation_multiplier` (rotation 360°) | ±5° |
| V5.4 | Odométrie sur carré de 5 m | Erreur de fermeture < 1 m |
| V5.5 | Test d'arrêt d'urgence **en mouvement au sol** | Distance d'arrêt mesurée et documentée |
| V5.6 | Mesure du courant de régénération en descente | < 15 A, sinon revoir la stratégie de freinage |

## Phase 6 — Navigation intérieure

**Durée 📐 : 2–3 semaines**

| # | Tâche | Critère de sortie |
|---|---|---|
| V6.1 | Intégrer le YDLIDAR X4 (build source, alim 5 V externe) | `/scan` à 7 Hz, > 60 % de points valides |
| V6.2 | Cohérence TF lidar/URDF | Erreur < 5 cm sur un mur connu |
| V6.3 | `slam_toolbox` en intérieur | Carte cohérente, boucle fermée |
| V6.4 | Nav2 intérieur | Objectif à 20 m atteint à ±30 cm |
| V6.5 | `collision_monitor` : obstacle sur la trajectoire | Arrêt avant contact |
| V6.6 | Mission de 30 min sans intervention | Journal propre |

## Phase 7 — Extérieur et GPS

**Durée 📐 : 3–4 semaines**

| # | Tâche | Critère de sortie |
|---|---|---|
| V7.1 | Intégrer le GPS, vérifier la covariance | ≥ 8 satellites, covariance non nulle |
| V7.2 | Monter le BNO085 sur mât, calibrer hard/soft iron | Cap stable moteurs en marche, ±10° |
| V7.3 | Comparer Rotation Vector (magnéto) vs Game RV (gyro) sur parcours en boucle | Le meilleur des deux retenu, documenté |
| V7.4 | Double EKF + `navsat_transform` | Trajectoire cohérente sur 100 m |
| V7.5 | Nav2 extérieur (MPPI, contraintes d'accélération) | Waypoints GPS atteints à ±3 m |
| V7.6 | Progression terrain : plat → irrégulier → pente 10 % | Aucun calage, courants journalisés |
| V7.7 | Mission complète 30 min en extérieur | Journal propre, autonomie mesurée |

## Phase 8 — Consolidation

**Durée 📐 : en continu**

| # | Tâche |
|---|---|
| V8.1 | Mettre à jour ce dossier avec toutes les valeurs mesurées (remplacer 🔴 et 📐 par ✅) |
| V8.2 | Documenter le plan de câblage réel, photos, étiquetage |
| V8.3 | Rédiger les procédures d'exploitation (mise en route, arrêt, maintenance) |
| V8.4 | Mettre en place le plan de maintenance (§R.7) |
| V8.5 | Évaluer les évolutions de phase 4 selon les limites rencontrées |

## V.1 Chemin critique et jalons

```
 Sem. 1-2   ████ Phase 0 — Incertitudes        ⚠️ M1 = jalon décisif
 Sem. 1-4   ████████ Phase 1 — Logiciel (en parallèle)
 Sem. 3-6   ████████ Phase 2 — Électronique BT
 Sem. 6-9   ████████ Phase 3 — Puissance       ⚠️ phase la plus dangereuse
 Sem. 9-12  ████████ Phase 4 — Moteurs levés   ⚠️ V4.8 = jalon sécurité
 Sem. 12-14 ████ Phase 5 — Premiers pas
 Sem. 14-17 ████████ Phase 6 — Nav intérieure
 Sem. 17-21 ██████████ Phase 7 — Extérieur + GPS
                                                → objectif « Nav2 autonome »
📐 ≈ 5 mois à raison de 8–12 h/semaine. Cohérent avec l'objectif de 3–6 mois.
```

**Trois jalons bloquants** :

| Jalon | Quoi | Si échec |
|---|---|---|
| **J1 — V0.8** | Seuil de coupure du BMS mesuré | Si < 25 A : passer en 2 roues motrices, ou changer de pack. **Décision d'architecture** |
| **J2 — V0.6** | Ubuntu 24.04 + ROS 2 Jazzy sur le X1 | Repli sur 22.04 + Humble, ou Docker. **Décision de plateforme** |
| **J3 — V4.8** | Tous les tests d'arrêt d'urgence passants, roues levées | **Interdiction de poser le robot au sol.** Non négociable |

## V.2 Ce qu'il faut faire en premier, cette semaine

Si tu ne fais qu'une chose :

1. **Répondre aux 7 questions du §B.3** — 20 minutes, et ça débloque tout le reste.
2. **`lspci -nn | grep -i usb` sur le X1** — 10 secondes, et ça décide du sort du Kinect.
3. **Regarder le marquage des condensateurs des 4 ZS-X11H** — 5 minutes, et ça évite peut-être 4 cartes détruites.
4. **Installer Ubuntu 24.04 + ROS 2 Jazzy sur le X1** — c'est le risque #33, et il est en tête du chemin critique.

Le reste peut attendre. Ces quatre points, non.

---

## Annexe — Sources

**Batterie et motorisation**
[Manuel Xiaomi Mi Electric Scooter](https://i01.appmifile.com/webfile/globalimg/Global_UG/Mi_Ecosystem/Mi_Electric_Scooter/en_V1.pdf) · [Manuel M365 Pro](https://i01.appmifile.com/webfile/globalimg/Global_UG/Mi_Ecosystem/Mi_Electric_Scooter_Pro/en-GB_V2.pdf) · [arXiv:2411.17184 — analyse matérielle du BMS M365](https://arxiv.org/html/2411.17184v1) · [BotoX/xiaomi-m365-compatible-bms](https://github.com/BotoX/xiaomi-m365-compatible-bms) · [CamiAlfa/M365-BLE-PROTOCOL](https://github.com/CamiAlfa/M365-BLE-PROTOCOL/blob/master/protocolo) · [ScooterHacking Wiki — ESC1](https://wiki.scooterhacking.org/doku.php?id=esc1-overview) · [MAD-EE — ZS-X11H](https://mad-ee.com/easy-inexpensive-hoverboard-motor-controller/) · [RoboFoundry — hoverboard motors](https://robofoundry.medium.com/cheaper-way-to-control-hoverboard-motors-79b02dd8a521) · [ODrive — docs/hoverboard.md](https://github.com/odriverobotics/ODrive/blob/master/docs/hoverboard.md)

**Capteurs et calculateur**
[YDLIDAR X4 Datasheet V2.0](https://download.kamami.pl/p1178202-YDLIDAR%20X4%20Data%20sheet%20V2.0.pdf) · [YDLIDAR X4 User Manual](https://static.generation-robots.com/media/ydlidar-x4-user-manual.pdf) · [BNO08X Datasheet Rev 1.17 (CEVA)](https://www.ceva-ip.com/wp-content/uploads/BNO080_085-Datasheet.pdf) · [Adafruit BNO085 guide](https://learn.adafruit.com/adafruit-9-dof-orientation-imu-fusion-breakout-bno085) · [ACS758 Datasheet Rev 20 (Allegro)](https://www.allegromicro.com/~/media/files/datasheets/acs758-datasheet.pdf) · [libfreenect2](https://github.com/OpenKinect/libfreenect2) · [libfreenect2 — Performance](https://github.com/OpenKinect/libfreenect2/wiki/Performance) · [krepa098/kinect2_ros2](https://github.com/krepa098/kinect2_ros2) · [Youyeetoo X1 wiki](https://wiki.youyeetoo.com/en/x1) · [CNX Software — X1 review part 2](https://www.cnx-software.com/2023/12/06/youyeetoo-x1-x86-sbc-review-gpio-uart-i2c-spi-nfc-poe-module-power-consumption/)

**ROS 2 Jazzy**
[REP-2000](https://www.ros.org/reps/rep-2000.html) · [ros2_control Jazzy — controllers](https://control.ros.org/jazzy/doc/ros2_controllers/doc/controllers_index.html) · [diff_drive_controller Jazzy](https://control.ros.org/jazzy/doc/ros2_controllers/diff_drive_controller/doc/userdoc.html) · [Guide de migration Jazzy](https://control.ros.org/jazzy/doc/ros2_controllers/doc/migration.html) · [Écrire un hardware component](https://control.ros.org/jazzy/doc/ros2_control/hardware_interface/doc/writing_new_hardware_component.html) · [Nav2 — Collision Monitor](https://docs.nav2.org/configuration/packages/collision_monitor/configuring-collision-monitor-node.html) · [Nav2 — MPPI](https://docs.nav2.org/configuration/packages/configuring-mppic.html) · [Nav2 — GPS Navigation](https://docs.nav2.org/tutorials/docs/navigation2_with_gps.html) · [robot_localization](https://index.ros.org/p/robot_localization/) · [slam_toolbox](https://index.ros.org/p/slam_toolbox/) · [Improved Dynamic Discovery (Jazzy)](https://docs.ros.org/en/jazzy/Tutorials/Advanced/Improved-Dynamic-Discovery.html) · [rmw_zenoh (jazzy)](https://github.com/ros2/rmw_zenoh/tree/jazzy) · [micro-ROS — hardware supporté](https://micro.ros.org/docs/overview/hardware/) · [micro_ros_platformio](https://github.com/micro-ROS/micro_ros_platformio) · [Vulcanexus — reconnexions micro-ROS](https://docs.vulcanexus.org/en/latest/rst/tutorials/micro/handle_reconnections/handle_reconnections.html) · [diagnostics](https://index.ros.org/r/diagnostics/) · [nav2_lifecycle_manager](https://docs.nav2.org/configuration/packages/configuring-lifecycle.html)

**Électricité DC, protections, busbars**
[Legrand F03693FR-01 — DX3 en courant continu](https://assets.legrand.com/pim/NP-FT-GT/F03693FR-01.pdf) · [Blue Sea 187-Series](https://www.bluesea.com/products/7146/) · [Blue Sea Class T 125 V / 20 kA](https://www.bluesea.com/products/5121/Class_T_Fuse_-_400_Amp) · [Victron — fusibles MIDI/MEGA/ANL 32, 58, 80 V](https://www.victronenergy.com/upload/documents/Datasheet-Midi,-Mega-and-ANL-fuses,-and-fuse-holders-EN-.pdf) · [Littelfuse ATO 32 V](https://www.mouser.com/datasheet/2/240/Littelfuse_BladeFuse_ATO32V-46883.pdf) · [Bussmann MRBF](https://www.currentconnected.com/product/cooper-bussmann-mrbf-terminal-fuse-30a-250a/) · [ABYC E-11 excerpts](https://www.paneltronics.com/images/technical/E11Excerpts.pdf) · [ISO 13297 — ampacité métrique](https://boathowto.com/wiresize/wiresize_tables_iso.pdf) · [ABYC E-11 Table 6A — groupement](https://boathowto.com/wiresize/wiresize_tables_abyc.pdf) · [Blue Sea — chute de tension](https://www.bluesea.com/support/articles/Circuit_Protection/1437/Part_1:_Choosing_the_Correct_Wire_Size_for_a_DC_Circuit) · [Copper Development Association — busbar ampacities](https://www.copper.org/applications/electrical/busbar/bus_table1.html) · [Storm Power — couple × revêtement sur la résistance de joint](https://stormpowercomponents.com/exploring-busbar-joint-resistance-a-study-on-the-effects-of-torque-and-plating/) · [MakerStage — Copper Bus Bar Design Guide](https://www.makerstage.com/resources/copper-electrical-contacts-design) · [Sensata — conception des circuits de précharge](https://www.sensata.com/sites/default/files/a/sensata-how-to-design-precharge-circuits-evs-whitepaper.pdf) · [TI SLVAE57 — Basics of Ideal Diodes](https://www.ti.com/lit/an/slvae57b/slvae57b.pdf) · [TI SLUP419 — Clearance and Creepage (IEC 60664-1)](https://www.ti.com/lit/pdf/slup419) · [CED Engineering — Arc Flash en DC](https://www.cedengineering.com/userfiles/Arc%20Flash%20Hazard%20Calculations%20in%20DC%20Systems%20R1.pdf) · [Enatel — essais de court-circuit Li-ion](https://www.enatel.net/wp-content/uploads/2024/08/Short-Circuit-Testing-of-Li-ion-in-standby-applications-Murray-Wyma.pdf) · [Leroy Merlin — profilés laiton (aucune barre cuivre)](https://www.leroymerlin.fr/produits/quincaillerie/corniere-tube-tole-et-profile/corniere-et-profile/profile-laiton/) · [Leroy Merlin — coffret IP65 rail DIN](https://www.leroymerlin.fr/produits/coffret-etanche-avec-rail-din-pour-5-modules-ip65-87615283.html)

---

*Fin du dossier d'architecture — version 1.0, 8 août 2026.*
*Ce document est vivant : chaque mesure réalisée doit y remplacer un 🔴 ou un 📐 par un ✅.*
