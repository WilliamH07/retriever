# État de la liaison entre le calculateur et un microcontrôleur.
#
# Les compteurs d'erreur sont cumulatifs depuis le démarrage du nœud. C'est leur
# DÉRIVÉE qui est intéressante : quelques erreurs au branchement sont normales,
# quelques erreurs par seconde ne le sont pas.

std_msgs/Header header

string transport          # "serie /dev/ttyUSB0 a 921600 bauds" ou "socketcan can0"
bool connected

uint64 rx_frames
uint64 tx_frames
uint64 crc_errors         # cadrage correct, CRC faux : du bruit sur la ligne
uint64 format_errors      # COBS invalide ou en-tête incohérent : pas du bruit
uint64 overflows          # flux désynchronisé
uint64 unknown_frames     # identifiant absent du protocole

# Aller-retour applicatif mesuré par LINK_PING / LINK_PONG.
float32 round_trip_ms
float32 round_trip_ms_max

# ⚠️ Une divergence de hash est un refus d'armement, pas un avertissement
# (§K.2 [P3]). Un firmware pas à jour après un git pull est le mode de
# défaillance le plus pénible d'un système embarqué distribué.
uint32 protocol_hash_host
uint32 protocol_hash_node
bool protocol_match
