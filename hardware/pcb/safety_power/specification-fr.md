# Carte de distribution de puissance — spécification

Husky A300. Batterie 10S4P 37 V / 474 Wh → rails **37 V**, **12 V**, **5 V**.

---

## 1. Ce que dit ta batterie

`10INR19/66-4` est une désignation normalisée IEC 61960. Elle se décode :

| | |
|---|---|
| `10` | **10 éléments en série** |
| `INR` | chimie lithium NMC, format cylindrique |
| `19/66` | Ø19 × 66 mm arrondi → **cellules 18650** |
| `-4` | **4 en parallèle** |

Donc **10S4P**, et 12 800 mAh / 4 = **3 200 mAh par cellule**. Vérification : 37 V × 12,8 Ah = 473,6 Wh, ce qui colle avec les 474 Wh annoncés.

### Conséquence n°1 — ta tension n'est pas 37 V

C'est la valeur nominale. La vraie plage est :

| État | Tension |
|---|---|
| Pleine charge (4,2 V/cellule) | **42,0 V** |
| Nominal (3,7 V/cellule) | 37,0 V |
| Coupure BMS (3,0 V/cellule) | **30,0 V** |

**Tout composant sur ce rail doit tenir 42 V, avec de la marge.** Un module DC-DC « 8–40 V » sera détruit à la première charge complète. C'est l'erreur classique et elle est définitive.

Règle de dimensionnement : **choisis des composants tenus à 60 V**, jamais 40 ou 50.

### Conséquence n°2 — le courant disponible

Des cellules 18650 de 3 200 mAh sont des cellules **d'énergie**, pas de puissance (classe Samsung 32E, LG MJ1, Panasonic GA). Elles sortent typiquement 2C à 3C en continu, soit 6,4 à 10 A chacune.

En 4P : **26 à 40 A** au niveau des cellules. Mais c'est presque toujours le **BMS** qui limite en premier — sur un pack 10S4P de ce type, il est généralement calibré entre **20 et 30 A en continu**, 40 à 60 A en pointe.

> **À vérifier sur l'étiquette de ton pack ou du BMS.** C'est le chiffre qui commande tout le dimensionnement en aval. Tant que je ne l'ai pas, je travaille avec **25 A continu / 40 A pointe**.

### Autonomie

Sur 474 Wh, en gardant 20 % de réserve (on ne vide jamais un pack lithium à fond) → **379 Wh utiles**.

| Consommation moyenne | Autonomie |
|---|---|
| 80 W (électronique seule, robot à l'arrêt) | 4 h 40 |
| 150 W (roulage lent sur plat) | 2 h 30 |
| 200 W | 1 h 55 |
| 300 W | 1 h 20 |
| 400 W (terrain difficile) | 55 min |

---

## 2. Le disjoncteur 100 A : il ne sert à rien

C'est le point le plus important de ce document.

Ton pack sort **25 à 30 A** en continu. Un disjoncteur de 100 A ne se déclenchera **jamais** avant que le BMS ne coupe, ou avant que tes câbles ne chauffent. Une protection qui ne se déclenche jamais avant l'élément qu'elle protège est décorative.

Pire : en cas de court-circuit franc, c'est le BMS qui encaissera l'intégralité du défaut, alors que c'est justement lui qu'on veut protéger.

**Redimensionne : 40 A.** Ça laisse la marge nécessaire aux pointes d'accélération des quatre moteurs, et ça coupe bien avant que quoi que ce soit ne fonde.

Rôles à ne pas confondre :

| Élément | Protège |
|---|---|
| **BMS** | les cellules (sur-décharge, surcharge, température) |
| **Fusible principal 40 A** | le câblage, en cas de court-circuit franc |
| **Fusibles de branche** | chaque consommateur, indépendamment des autres |

---

## 3. Architecture

```
Batterie 10S4P  30–42 V
   │
   ├── BMS (dans le pack)
   │
   ├── Coupe-batterie manuel
   │      └─ [précharge : R 100 Ω / 10 W + bouton poussoir en parallèle]
   │
   ├── Fusible principal 40 A (ANL ou MIDI)
   │
   ▼
┌─────────────── CARTE DE DISTRIBUTION ───────────────┐
│                                                      │
│  BUS 37 V  (plan de cuivre 2 oz, 10 mm)             │
│    ├ F1  30 A  → contrôleur(s) moteurs               │
│    ├ F2   5 A  → convertisseur 12 V                  │
│    ├ F3   3 A  → convertisseur 5 V                   │
│    └ F4   5 A  → réserve                             │
│                                                      │
│  BUS 12 V  (sortie DC-DC n°1)                        │
│    ├ 5 A → Youyeetoo X1                              │
│    ├ 2 A → ventilateurs                              │
│    ├ 2 A → LIDAR / capteurs                          │
│    └ 2 A → réserve                                   │
│                                                      │
│  BUS 5 V  (sortie DC-DC n°2)                         │
│    ├ 1 A → carte Hall + ESP32                        │
│    ├ 1 A → capteurs                                  │
│    └ 1 A → réserve                                   │
└──────────────────────────────────────────────────────┘
```

---

## 4. Les deux convertisseurs

### Bonne nouvelle sur le Youyeetoo X1

Il s'alimente en **12 V DC** (prise 5,5 × 2,5 mm, adaptateur recommandé 12 V / 3 A, soit 36 W), avec un Celeron N5105 de 10 W de TDP. Sa consommation réelle sera bien en dessous des 36 W la plupart du temps.

**Tu n'as donc pas besoin d'un rail 19 V.** Ton calculateur partage le rail 12 V avec les ventilateurs et les capteurs. Ça simplifie beaucoup la carte.

### Spécification des convertisseurs

Je te donne des **exigences**, pas des références — la disponibilité varie trop et une référence périmée te ferait perdre du temps.

| | Convertisseur 12 V | Convertisseur 5 V |
|---|---|---|
| Plage d'entrée | **20 – 60 V minimum** | **20 – 60 V minimum** |
| Sortie | 12 V, **8 A** (96 W) | 5 V, **5 A** (25 W) |
| Rendement visé | ≥ 90 % | ≥ 88 % |
| Protection | court-circuit + thermique intégrées | idem |

Les 8 A du rail 12 V ne sont pas du gaspillage : X1 (3 A) + ventilateurs + LIDAR + réserve, et un convertisseur qui travaille à 50 % de sa capacité chauffe peu et dure longtemps.

**Le critère éliminatoire, c'est la plage d'entrée.** Beaucoup de modules bon marché sont donnés « 8–40 V » — ils grillent à 42 V. Cherche du **60 V**, typiquement dans les modules vendus pour le 48 V industriel ou le poids-lourd.

### Modules du commerce, pas de régulateur maison

Pour cette carte, je te recommande fortement une **carte porteuse** : le PCB porte les bus, les fusibles, les connecteurs, les LED et les mesures, et les deux convertisseurs sont des **modules enfichés ou vissés**, remplaçables.

Concevoir un abaisseur découpage 60 V / 8 A de zéro, c'est un exercice à part entière : boucle de commutation, thermique, CEM, layout au millimètre. Ce n'est pas la marche à monter juste après ta première carte. Et un module qui lâche se remplace en deux minutes au lieu de condamner la carte.

---

## 5. Protections

| Protection | Choix | Contre quoi |
|---|---|---|
| **Inversion de polarité** | **connecteurs détrompés** (XT90 ou Anderson) | Une diode à 30 A dissiperait 20 W. Un connecteur qu'on ne peut pas brancher à l'envers coûte 2 € et ne chauffe jamais. |
| **Surtension bus 37 V** | TVS **SMCJ48A** (48 V de seuil, au-dessus des 42 V) | pics de récupération des moteurs |
| **Réservoir d'énergie** | 2 × 470 µF / **63 V** faible ESR | absorbe les pics de freinage |
| **Court-circuit général** | fusible principal 40 A | le câblage |
| **Défaut de branche** | fusibles lame automobile sur support | isole un défaut sans tout couper |
| **Surtension 12 V / 5 V** | TVS SMBJ15A et SMBJ6.0A | un convertisseur qui lâche en direct |

### Le point que personne n'anticipe : la récupération

Tes quatre moteurs-roues sont des générateurs dès que le robot descend une pente ou freine. Le courant repart **vers la batterie**, et la tension du bus **monte**.

Si le BMS coupe à ce moment-là (pack déjà plein), le courant n'a plus nulle part où aller et la tension du bus peut grimper très au-dessus de 42 V en quelques millisecondes. C'est ce qui tue les cartes de puissance sur les robots à moteurs-roues.

D'où les 470 µF et la TVS. Ils ne sont pas optionnels.

---

## 6. Le PCB : calculs de cuivre

Largeurs calculées selon IPC-2221, couche externe, échauffement 30 °C :

| Courant | 1 oz | **2 oz** | 3 oz |
|---|---|---|---|
| 40 A | 24,6 mm | **12,3 mm** | 8,2 mm |
| 30 A | 16,5 mm | **8,3 mm** | 5,5 mm |
| 20 A | 9,5 mm | **4,7 mm** | 3,2 mm |
| 10 A | 3,6 mm | **1,8 mm** | 1,2 mm |
| 5 A | 1,4 mm | **0,7 mm** | 0,5 mm |

### Ce que je retiens

- **Cuivre 2 oz (70 µm)** — le surcoût est faible et il divise par deux toutes les largeurs
- **Bus 37 V : plan de 10 mm minimum**, coulé sur les **deux faces** et cousu de vias tous les 5 mm. Deux faces reliées, c'est presque deux fois la section.
- Branches 12 V : **2,5 mm**
- Branches 5 V : **1,5 mm**
- Sous les fusibles et connecteurs de puissance : ouvre le vernis épargne sur la piste et **dépose un cordon de soudure** dessus. C'est du cuivre gratuit, ça peut doubler la capacité localement.

Avec ça, une carte 2 couches classique encaisse tes 30 A sans problème. Pas besoin de barre de cuivre rapportée.

---

## 7. La précharge — 3 € qui sauvent ton coupe-batterie

Tu as choisi le coupe-batterie manuel seul. C'est jouable, mais il faut connaître le problème.

Tes contrôleurs moteurs ont de gros condensateurs d'entrée. Quand tu fermes le coupe-batterie, ils sont vides : pendant quelques millisecondes, ils se comportent comme un **court-circuit**. Le courant d'appel monte à plusieurs centaines d'ampères, tu vois une étincelle, et à chaque fois les contacts se piquent un peu plus. Au bout de quelques dizaines de cycles, le coupe-batterie chauffe ou colle.

**La parade** : une résistance de **100 Ω / 10 W** montée en permanence **en parallèle** du coupe-batterie, ou en série avec un bouton poussoir.

Procédure : tu appuies sur le poussoir 2 secondes — les condensateurs se chargent doucement à travers la résistance — puis tu fermes le coupe-batterie sur une tension déjà égalisée. Plus d'étincelle, plus d'usure.

Ça ne change rien à ton architecture et ça se rajoute sur le faisceau, pas sur la carte.

---

## 8. Instrumentation — à décider

Optionnel, mais sur un robot autonome ça change la vie :

- **Tension du bus** : pont diviseur vers un ADC → tu connais ton état de charge
- **Courant principal** : capteur à effet Hall type ACS758 (50 A) ou shunt + INA226
- **Courant par branche** : INA226 sur I²C, une par rail
- **LED de présence** par rail (37 V, 12 V, 5 V) — diagnostic immédiat à l'œil

Le tout peut être lu par un petit micro sur la carte, ou remonté en I²C vers ton ESP32 ou le X1.

---

## 9. Ce qu'il me manque pour figer le dimensionnement

1. **Le courant continu du BMS** — l'étiquette du pack ou du BMS. C'est le chiffre qui commande tout.
2. **Les contrôleurs moteurs** : un seul pour les quatre roues, un par essieu, ou un par roue ? Ça change le nombre de branches 37 V et le calibre des fusibles.
3. **La liste des consommateurs 12 V** : quel LIDAR, combien de ventilateurs, quoi d'autre.
4. **L'encombrement disponible** sur l'étage bas du robot, pour dimensionner la carte.
