# Composant ESP-IDF : en-tête de protocole généré.
#
# Aucune source à compiler : l'en-tête est entièrement en fonctions
# `static inline`, ce que le compilateur replie sur place. Le composant existe
# pour que le chemin d'inclusion soit déclaré une seule fois, et pour que la
# dépendance « ce firmware utilise le protocole » apparaisse dans le graphe.

idf_component_register(INCLUDE_DIRS "../../protocol/generated")
