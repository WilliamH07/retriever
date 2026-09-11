# Composant ESP-IDF : couche de liaison Retriever.
#
# Le répertoire `portable/` contient du C99 sans aucune dépendance ESP-IDF. Ces
# fichiers sont compilés à l'identique par le firmware, par le nœud ROS 2 et par
# les tests hôte : une seule implémentation du cadrage, donc aucune divergence
# possible entre les deux bouts de la liaison.

idf_component_register(
    SRCS
        "portable/rt_cobs.c"
        "portable/rt_crc16.c"
        "portable/rt_framing.c"
        "src/link.c"
        "src/link_uart.c"
        "src/link_twai.c"
        "src/link_log.c"
    INCLUDE_DIRS
        "include"
        "portable"
    PRIV_INCLUDE_DIRS
        "src"
    REQUIRES
        retriever_protocol
        driver
        esp_timer
)
