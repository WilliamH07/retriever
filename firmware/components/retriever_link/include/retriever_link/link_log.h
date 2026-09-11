/* ===========================================================================
 *  link_log.h — tunnel de journalisation, voir link_log.c
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#ifndef RETRIEVER_LINK_LOG_H
#define RETRIEVER_LINK_LOG_H

#include <stddef.h>
#include <stdint.h>

#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Détourne la sortie de journalisation d'ESP-IDF vers des trames LOG.
 * À appeler APRÈS rt_link_init(). Les messages émis avant restent sur la
 * console, s'il y en a une.
 */
void rt_link_log_install(void);

/** Émet un texte déjà formaté. Utilisable sans passer par ESP_LOGx. */
void rt_link_log_emit(uint8_t level, const char *text, size_t len);

/** Fragments perdus faute de place en file. Publié dans les diagnostics. */
uint32_t rt_link_log_dropped(void);

#ifdef __cplusplus
}
#endif

#endif /* RETRIEVER_LINK_LOG_H */
