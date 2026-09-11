# Composant ESP-IDF : capteur inertiel.
#
# La pile SH-2 / SHTP vient de CEVA (https://github.com/ceva-dsp/sh2), publiée
# sous Apache 2.0 — la même licence que le code de ce projet. Elle est attachée
# en sous-module git, épinglée sur la version 1.4.0, et n'est jamais modifiée.
# Ce qui appartient au projet, c'est le portage : sh2_hal_esp32_spi.c.
#
#   git submodule update --init --recursive
#   # ou : tools/fetch_firmware_deps.sh

set(SH2_DIR "${CMAKE_CURRENT_LIST_DIR}/vendor/sh2")

if(NOT EXISTS "${SH2_DIR}/sh2.c")
    message(FATAL_ERROR
        "La bibliotheque SH-2 est absente de ${SH2_DIR}.\n"
        "Lancer : git submodule update --init --recursive\n"
        "ou      : tools/fetch_firmware_deps.sh")
endif()

idf_component_register(
    SRCS
        "src/imu_bno085.c"
        "src/sh2_hal_esp32_spi.c"
        "${SH2_DIR}/sh2.c"
        "${SH2_DIR}/sh2_SensorValue.c"
        "${SH2_DIR}/sh2_util.c"
        "${SH2_DIR}/shtp.c"
        "${SH2_DIR}/euler.c"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "src"
        "${SH2_DIR}"
    REQUIRES
        driver
        esp_timer
)

# La pile CEVA est du code tiers : on ne la compile pas avec les avertissements
# durcis du projet, sinon chaque mise a jour du sous-module casse le build sans
# que ce soit notre code qui soit en cause.
set_source_files_properties(
    "${SH2_DIR}/sh2.c" "${SH2_DIR}/sh2_SensorValue.c" "${SH2_DIR}/sh2_util.c"
    "${SH2_DIR}/shtp.c" "${SH2_DIR}/euler.c"
    PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-sign-compare")
