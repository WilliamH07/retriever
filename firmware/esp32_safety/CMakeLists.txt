# Projet ESP-IDF — nœud SAFETY de Retriever.
#
#   idf.py set-target esp32
#   idf.py build flash
#
# Les composants partagés vivent dans firmware/components/ : ils sont communs au
# nœud SAFETY et aux futurs nœuds MOTION, et c'est ce qui garantit qu'il n'y
# aura jamais deux implémentations du protocole ou du cadrage.

cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(retriever_safety)
