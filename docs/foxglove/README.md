# Mises en page Foxglove

Versionnées ici pour une raison simple : une mise en page Foxglove vit dans le
stockage local de l'application, elle n'est ni partagée entre machines ni
sauvegardée, et **sa suppression est définitive** — il n'y a pas d'annulation.
Reconstruire six panneaux à la main après chaque mise à jour, changement de
poste ou fausse manœuvre est exactement le genre de travail qu'un fichier
versionné supprime.

## `bench_imu.json` — banc B1

Les six panneaux de la recette de banc :

| Panneau | Contenu |
|---|---|
| 3D | repère fixe `imu_world`, le marqueur suit `imu_link` |
| Plot | `/imu/data.angular_velocity` sur les trois axes |
| Plot | `/imu/data.linear_acceleration` sur les trois axes, **gravité comprise** |
| Diagnostics | les trois vérifications du pont : capteur, liaison, nœud |
| Raw Messages | `/retriever/imu_status` — qualités et état d'étalonnage |
| Raw Messages | `/retriever/link_status` — hash du protocole, erreurs, aller-retour |

### Importer

Dans Foxglove Studio, menu des mises en page (l'icône à deux rectangles, en
haut à droite) → **Import from file…** → choisir ce fichier.

Se connecter ensuite à `ws://<adresse-du-calculateur>:8765`.

⚠️ Le panneau 3D n'affiche quelque chose que si `bench.publish_tf` est actif
dans `ros2_ws/src/retriever_bringup/config/link_bench.yaml`. C'est une aide de
banc : sur le robot, c'est l'EKF qui publie cette transformation, et laisser les
deux actifs donnerait deux sources sur la même arête de l'arbre TF.

### Ce qu'on regarde, et dans quel ordre

1. **Diagnostics** — les trois lignes au vert. C'est le seul panneau qui dit
   « tout va bien » sans qu'on ait à interpréter des chiffres.
2. **3D** — on bouge le capteur, le repère suit. Vérifie la chaîne entière
   d'un coup d'œil, y compris les conventions de repère.
3. **Plots** — au repos, `linear_acceleration` doit avoir une norme de
   9,81 m/s². Un écart constant dans toutes les orientations est une erreur
   d'échelle ; un écart qui varie avec l'orientation est un biais, et c'est ce
   que corrige l'étalonnage (`tools/imu_cal.py`, voir `docs/DEMARRAGE.md` §4 bis).
4. **`/retriever/link_status`** — `protocol_match` vrai, `unknown_frames` à 0.

*Copyright (c) 2026 William Hanczyk — Apache License 2.0*
