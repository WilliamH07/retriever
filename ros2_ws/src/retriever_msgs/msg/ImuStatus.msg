# Qualité et santé du capteur inertiel.
#
# Ce message existe parce que sensor_msgs/Imu ne dit pas si l'IMU va bien : il
# porte des valeurs, pas un état. Or les deux pannes les plus courantes d'une
# IMU sur SPI — le capteur qui se réinitialise en boucle et le capteur qui se
# tait — ne produisent aucune valeur aberrante. Elles ne se voient que là.

std_msgs/Header header

# Statut d'étalonnage rapporté par le BNO085, par grandeur.
uint8 STATUS_UNRELIABLE = 0
uint8 STATUS_LOW        = 1
uint8 STATUS_MEDIUM     = 2
uint8 STATUS_HIGH       = 3

uint8 status_orientation
uint8 status_gyro
uint8 status_accel
uint8 status_mag

# Estimation d'erreur d'orientation fournie par le capteur, en radians.
# Vaut 0 quand le mode choisi n'en fournit pas (game rotation vector).
float32 orientation_accuracy

# Doit rester à zéro. Tout autre valeur signale une alimentation instable, un
# défaut de câblage SPI, ou un capteur en fin de vie.
uint32 sensor_resets

# Échantillons perdus faute de place dans la file du microcontrôleur.
uint32 samples_dropped_node

# Triplets incomplets rejetés par le calculateur (trame manquante sur le lien).
uint32 samples_dropped_link

# Cadence réellement mesurée de /imu/data, en hertz.
float32 rate_hz

# Écart de la norme du quaternion à 1, AVANT normalisation. La quantification
# Q14 du transport en laisse quelques 1e-4 ; au-delà de 1e-2, c'est une
# corruption ou une erreur d'échelle.
float32 quaternion_norm_error
