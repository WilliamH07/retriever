/* ===========================================================================
 *  link_backend.h — interface interne d'un transport
 *
 *  Trois fonctions. C'est tout ce qu'un transport doit savoir faire, et c'est
 *  la mesure de ce que coûte réellement le passage de la série au CAN.
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#ifndef RETRIEVER_LINK_BACKEND_H
#define RETRIEVER_LINK_BACKEND_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "retriever_link/link.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    esp_err_t (*init)(const rt_link_config_t *cfg);
    esp_err_t (*send)(const rt_frame_t *f);
    /** Lit ce qui arrive et appelle rt_link_deliver() pour chaque trame valide. */
    void (*poll)(TickType_t wait);
    /** Complète les compteurs propres au transport. Peut être NULL. */
    void (*stats)(rt_link_stats_t *out);
} rt_link_backend_ops_t;

/** Appelé par un transport pour livrer une trame reçue et validée. */
void rt_link_deliver(const rt_frame_t *f);

const rt_link_backend_ops_t *rt_link_backend_uart(void);
const rt_link_backend_ops_t *rt_link_backend_twai(void);

#ifdef __cplusplus
}
#endif

#endif /* RETRIEVER_LINK_BACKEND_H */
