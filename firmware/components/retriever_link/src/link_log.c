/* ===========================================================================
 *  link_log.c — tunnel de journalisation sur la liaison
 *
 *  Sur une DevKitC, l'UART0 et la console ESP-IDF sortent par le même pont
 *  USB. Comme la liaison occupe cette UART, il faut choisir : soit on perd les
 *  journaux, soit on ajoute un second câble, soit on fait passer les journaux
 *  DANS la liaison. C'est la troisième option, et elle a un effet secondaire
 *  qui vaut à lui seul le détour : les journaux du firmware arrivent dans
 *  /rosout, horodatés par ROS, enregistrés dans les bags, et visibles dans
 *  Foxglove au même endroit que tout le reste.
 *
 *  Trois règles, dans cet ordre :
 *    1. ne jamais bloquer  — un fragment qui ne rentre pas est perdu et compté
 *    2. ne jamais récurser — si le transport journalise, on ne rejournalise pas
 *    3. ne jamais passer devant — file normale, jamais la file urgente
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#include "retriever_link/link_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "retriever_link/link.h"

#define LOG_LINE_MAX 192
#define CHARS_PER_FRAME 7

static SemaphoreHandle_t s_mutex;
static vprintf_like_t s_previous;
static bool s_enabled;
static uint32_t s_dropped;
static volatile bool s_in_hook;   /* garde de récursion, voir règle 2 */

static uint8_t level_from_prefix(const char *s)
{
    /* ESP-IDF préfixe la ligne par une couleur ANSI puis une lettre de niveau.
     * On cherche la lettre, sans supposer que la couleur est activée. */
    for (const char *p = s; *p && p < s + 12; ++p) {
        switch (*p) {
        case 'E': return RT_LOG_LEVEL_ERROR;
        case 'W': return RT_LOG_LEVEL_WARN;
        case 'I': return RT_LOG_LEVEL_INFO;
        case 'D':
        case 'V': return RT_LOG_LEVEL_DEBUG;
        default: break;
        }
    }
    return RT_LOG_LEVEL_INFO;
}

void rt_link_log_emit(uint8_t level, const char *text, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        const size_t n = (len - sent > CHARS_PER_FRAME) ? CHARS_PER_FRAME : (len - sent);
        const bool last = (sent + n >= len);

        rt_log_t m;
        memset(&m, 0, sizeof(m));
        m.header = (uint8_t)(((level & 0x07u) << 5) | (last ? 0x10u : 0x00u) | (uint8_t)n);
        /* Champ par champ : la structure générée n'est pas un tableau et on ne
         * suppose rien sur la disposition mémoire que le compilateur en fait. */
        uint8_t *const chars[CHARS_PER_FRAME] = {&m.c0, &m.c1, &m.c2, &m.c3,
                                                 &m.c4, &m.c5, &m.c6};
        for (size_t i = 0; i < n; ++i) {
            *chars[i] = (uint8_t)text[sent + i];
        }

        rt_frame_t f;
        rt_log_pack(&m, &f);
        if (rt_link_send(&f, 0) != ESP_OK) {
            s_dropped++;
            return;   /* inutile d'insister : la ligne est déjà tronquée */
        }
        sent += n;
    }
}

static int log_hook(const char *fmt, va_list args)
{
    if (s_in_hook) {
        return 0;
    }
    if (!s_enabled || s_mutex == NULL) {
        return s_previous ? s_previous(fmt, args) : 0;
    }
    if (xSemaphoreTake(s_mutex, 0) != pdTRUE) {
        s_dropped++;
        return 0;
    }

    s_in_hook = true;
    static char line[LOG_LINE_MAX];
    int n = vsnprintf(line, sizeof(line), fmt, args);
    if (n > 0) {
        size_t len = (size_t)n < sizeof(line) ? (size_t)n : sizeof(line) - 1u;
        /* Les fins de ligne et les séquences ANSI ne servent à rien une fois
         * dans /rosout : on les enlève ici plutôt que côté ROS, ça économise
         * de la bande passante sur le lien. */
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            len--;
        }
        const char *start = line;
        if (len > 0 && line[0] == '\033') {
            const char *m = memchr(line, 'm', len);
            if (m) {
                const size_t skip = (size_t)(m - line) + 1u;
                start += skip;
                len -= skip;
            }
        }
        while (len > 0 && (start[len - 1] == '\033' || start[len - 1] == '[')) {
            len--;
        }
        if (len > 0) {
            rt_link_log_emit(level_from_prefix(line), start, len);
        }
    }
    s_in_hook = false;

    xSemaphoreGive(s_mutex);
    return n;
}

void rt_link_log_install(void)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
    }
    s_enabled = true;
    s_previous = esp_log_set_vprintf(log_hook);
}

uint32_t rt_link_log_dropped(void) { return s_dropped; }
