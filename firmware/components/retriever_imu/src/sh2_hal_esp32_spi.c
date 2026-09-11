/* ===========================================================================
 *  sh2_hal_esp32_spi.c — portage du HAL SH-2 sur le SPI d'ESP-IDF
 *
 *  La bibliothèque SH-2 de CEVA (Apache 2.0, la même licence que ce projet)
 *  implémente SHTP et le protocole capteur. Elle demande cinq fonctions à la
 *  plateforme : open, close, read, write, getTimeUs. Ce fichier les fournit.
 *
 *  On ne réécrit PAS SHTP. C'est une pile à états avec des numéros de séquence
 *  par canal, un contrôle de flux et une gestion de reset ; la réimplémenter
 *  serait recopier un travail déjà fait, déjà débogué et publié sous une
 *  licence compatible. Le seul code spécifique au projet est celui-ci.
 *
 *  Protocole SPI du BNO08x, tel qu'il se déroule réellement — et ce point a
 *  coûté une séance de banc, alors il est écrit en toutes lettres :
 *
 *    lecture   H_INTN bas ──► TRANSACTION 1 : 4 octets d'en-tête, CS REMONTE
 *              longueur = (h0 | h1<<8) & 0x7FFF, en-tête compris
 *              ──► attendre que H_INTN se réabaisse
 *              ──► TRANSACTION 2 : le paquet ENTIER, en-tête RELU, CS remonte
 *
 *    écriture  attendre H_INTN bas ──► CS bas ──► écrire ──► CS haut
 *
 *  ⚠️ L'écriture NE TOUCHE PAS à PS0/WAKE. La datasheet décrit bien PS0 comme
 *  un signal de réveil après le reset, mais le pilote de référence Adafruit —
 *  qui fonctionne — ne s'en sert jamais : il attend H_INTN et écrit. Le
 *  composant n'est pas mis en veille par ce firmware, il n'y a donc rien à
 *  réveiller. Tirer PS0 à la masse à chaque écriture ajoutait un aléa sans
 *  contrepartie, sur la broche qui sélectionne aussi le protocole.
 *
 *  ⚠️⚠️ LE SPI EST FULL-DUPLEX, ET LE BNO08x LIT MOSI PENDANT NOS LECTURES.
 *  Il n'existe pas de « transaction de lecture » pour ce composant : chaque
 *  transaction est un échange. Ce que le maître sort sur MOSI pendant qu'il
 *  rentre un paquet, le composant le lit comme un en-tête SHTP venant de
 *  l'hôte. ESP-IDF, quand `tx_buffer` vaut NULL, ne garnit pas la FIFO
 *  d'émission : MOSI rejoue le contenu précédent. Le composant voit donc des
 *  paquets malformés, en empile une erreur sur le canal 0 à chaque lecture, et
 *  la liste d'erreurs s'allonge indéfiniment — ce qui maintient H_INTN bas,
 *  donc relance une lecture, donc une erreur de plus. Le pilote de référence
 *  Adafruit passe explicitement `sendvalue = 0x00` à chaque lecture ; c'est le
 *  même geste que le tampon de zéros ci-dessous. Un en-tête `00 00 00 00` se
 *  lit « longueur nulle », et c'est précisément ce qu'on veut dire.
 *
 *  ⚠️ Les deux transactions de lecture sont SÉPARÉES, et le paquet est relu
 *  depuis son début. Garder CS bas entre l'en-tête et le corps — ce qui paraît
 *  plus efficace, et que faisait la première version — ne donne PAS une lecture
 *  en deux temps : le composant y voit une lecture suivie d'une ÉCRITURE, il
 *  refuse le paquet malformé, empile une erreur SHTP sur le canal 0, et ne
 *  consomme jamais le paquet en attente. Symptôme observé : des milliers de
 *  lectures par seconde du même paquet, une liste d'erreurs qui s'allonge, et
 *  zéro événement capteur.
 *
 *  ⚠️ ÉTAT : écrit d'après la datasheet, le manuel de référence SH-2 et le
 *  portage de référence sh2-demo-nucleo. NON VALIDÉ SUR MATÉRIEL. La recette
 *  du banc B1 (§AG) contient les vérifications à faire dans l'ordre, et la
 *  toute première est un `sh2_getProdIds()` qui répond : si elle ne passe pas,
 *  rien de ce qui suit n'a de sens.
 *
 *  Copyright (c) 2026 William Hanczyk — Apache License 2.0
 * =========================================================================== */

#include "sh2_hal_esp32_spi.h"

#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "sh2.spi";

/* Datasheet : le composant a besoin d'un temps d'initialisation après reset,
 * et la bonne pratique est d'attendre l'assertion de H_INTN plutôt qu'un délai
 * fixe ✅ (§C15 point 4). Ce plafond n'est qu'un garde-fou. */
#define RESET_SETTLE_MS 300
#define INTN_WAIT_MS    100

/* ⚠️ Les tampons que nous passe la pile SH-2 ne sont ni alignés ni de longueur
 * multiple de 4. Le pilote SPI d'ESP-IDF exige les deux pour le DMA : à défaut,
 * il alloue un tampon de rebond au tas — à 100 Hz, et dans le chemin même dont
 * tout ce fichier cherche à protéger l'horodatage. On lit donc dans un tampon
 * statique aligné, et on recopie. C'est aussi ce qu'exige la règle §G.4-1 :
 * aucune allocation après l'initialisation. */
static WORD_ALIGNED_ATTR uint8_t s_staging[SH2_HAL_MAX_TRANSFER_IN];

/* Ce que l'on sort sur MOSI pendant une lecture. En BSS, donc nul, et il le
 * reste : personne n'écrit dedans. Voir le bandeau — ce n'est pas une
 * précaution de style, c'est la condition pour que le composant ne prenne pas
 * chacune de nos lectures pour une écriture malformée. */
static WORD_ALIGNED_ATTR uint8_t s_zeros[SH2_HAL_MAX_TRANSFER_IN];

typedef struct {
    sh2_Hal_t hal;              /* DOIT rester en premier : sh2 nous rend ce pointeur */
    spi_device_handle_t spi;
    SemaphoreHandle_t intn;
    rt_sh2_hal_pins_t pins;
    bool open;
} esp32_hal_t;

static esp32_hal_t s_hal;
static rt_sh2_hal_counters_t s_counters;

void rt_sh2_hal_get_counters(rt_sh2_hal_counters_t *out) { *out = s_counters; }

/* --------------------------------------------------------------------------
 *  H_INTN
 * ----------------------------------------------------------------------- */

static void IRAM_ATTR intn_isr(void *arg)
{
    (void)arg;
    BaseType_t hp = pdFALSE;
    xSemaphoreGiveFromISR(s_hal.intn, &hp);
    if (hp) {
        portYIELD_FROM_ISR();
    }
}

static bool wait_intn(uint32_t ms)
{
    /* La ligne est à niveau : si elle est déjà basse, l'interruption est peut-être
     * passée avant qu'on attende. On teste le niveau avant de bloquer. */
    if (gpio_get_level((gpio_num_t)s_hal.pins.intn) == 0) {
        xSemaphoreTake(s_hal.intn, 0);
        return true;
    }
    return xSemaphoreTake(s_hal.intn, pdMS_TO_TICKS(ms)) == pdTRUE;
}

bool rt_sh2_hal_wait_intn(uint32_t ms) { return wait_intn(ms); }

/* --------------------------------------------------------------------------
 *  Transferts
 * ----------------------------------------------------------------------- */

static esp_err_t spi_rx(uint8_t *dst, size_t len)
{
    if (len == 0u) {
        return ESP_OK;
    }
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    if (len > sizeof(s_zeros)) {
        return ESP_ERR_INVALID_SIZE;
    }
    t.length = len * 8u;
    t.rxlength = len * 8u;
    t.rx_buffer = dst;
    /* ⚠️ Obligatoire, voir le bandeau : sans tampon d'émission, MOSI rejoue la
     * transaction précédente et le composant compte une erreur SHTP par
     * lecture. */
    t.tx_buffer = s_zeros;
    /* Pas de SPI_TRANS_CS_KEEP_ACTIVE : chaque lecture est une transaction
     * complète, CS compris. Voir le bandeau en tête de fichier. */
    return spi_device_polling_transmit(s_hal.spi, &t);
}

static esp_err_t spi_tx(const uint8_t *src, size_t len)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8u;
    t.tx_buffer = src;
    return spi_device_polling_transmit(s_hal.spi, &t);
}

/* --------------------------------------------------------------------------
 *  Interface SH-2
 * ----------------------------------------------------------------------- */

static int hal_open(sh2_Hal_t *self)
{
    (void)self;
    if (s_hal.open) {
        return SH2_ERR;
    }

    /* Séquence de sélection du bus (§C15) : PS1 et PS0 hauts AVANT le reset et
     * jusqu'après la première assertion de H_INTN. PS1 est câblé au 3V3 ; PS0
     * est piloté ici, et le RESTE HAUT ensuite — plus personne n'y touche. */
    gpio_set_level((gpio_num_t)s_hal.pins.ps0, 1);
    gpio_set_level((gpio_num_t)s_hal.pins.rstn, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    xSemaphoreTake(s_hal.intn, 0);
    gpio_set_level((gpio_num_t)s_hal.pins.rstn, 1);

    if (!wait_intn(RESET_SETTLE_MS)) {
        ESP_LOGE(TAG, "pas de H_INTN apres reset : verifier PS0/PS1, BOOTN, et le cablage");
        return SH2_ERR_IO;
    }

    s_hal.open = true;
    return SH2_OK;
}

static void hal_close(sh2_Hal_t *self)
{
    (void)self;
    gpio_set_level((gpio_num_t)s_hal.pins.rstn, 0);
    s_hal.open = false;
}

/**
 * Montre les premiers paquets, puis un sur dix mille.
 *
 * Un en-tête SHTP valide, c'est : longueur sur 16 bits petit-boutiste (bit 15 =
 * continuation, masqué), puis le CANAL — 0 à 5 — puis un numéro de séquence.
 * Un canal supérieur à 5, ou une longueur qui ne bouge jamais, disent
 * immédiatement si l'on lit un vrai flux ou de la bouillie qui se trouve
 * ressembler à une longueur.
 */
static void dump_packet(unsigned total, unsigned len)
{
    const uint32_t n = s_counters.packets;
    if (n > 6u && (n % 10000u) != 0u) {
        return;
    }
    ESP_LOGI(TAG, "paquet %u : total=%u tampon=%u canal=%u seq=%u | %02x %02x %02x %02x %02x %02x %02x %02x",
             (unsigned)n, total, len, s_staging[2], s_staging[3], s_staging[0], s_staging[1],
             s_staging[2], s_staging[3], s_staging[4], s_staging[5], s_staging[6], s_staging[7]);
}

static int hal_read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us)
{
    (void)self;
    if (!s_hal.open) {
        return 0;
    }
    if (gpio_get_level((gpio_num_t)s_hal.pins.intn) != 0) {
        return 0;   /* rien à lire : ce n'est pas une erreur */
    }
    xSemaphoreTake(s_hal.intn, 0);
    s_counters.reads++;

    /* L'horodatage est pris ICI, au plus près de l'événement matériel. C'est
     * tout l'intérêt de mettre l'IMU sur le microcontrôleur plutôt que sur le
     * calculateur (§G.1) : la gigue est celle d'une interruption, pas celle de
     * l'ordonnanceur Linux. */
    if (t_us) {
        *t_us = (uint32_t)esp_timer_get_time();
    }

    /* --- Transaction 1 : l'en-tête seul, CS remonte à la fin --------------
     *
     * ⚠️ Dans s_staging, pas sur la pile : le DMA d'ESP-IDF veut une
     * destination alignée sur 32 bits et arrondit la longueur au multiple de 4
     * supérieur. Un `uint8_t header[4]` local n'offre ni l'un ni l'autre de
     * façon garantie. */
    if (spi_rx(s_staging, 4u) != ESP_OK) {
        return 0;
    }

    const unsigned total = (unsigned)((s_staging[0] | ((unsigned)s_staging[1] << 8)) & 0x7FFFu);
    if (total < 4u) {
        s_counters.empty_headers++;
        return 0;   /* en-tête vide : le composant n'avait rien à dire */
    }

    /* --- Le composant réasserte H_INTN pour annoncer qu'il va represente le
     *     paquet depuis son début. Sans cette attente, la seconde transaction
     *     part trop tôt et lit du remplissage. ------------------------------ */
    if (!wait_intn(INTN_WAIT_MS)) {
        s_counters.repeat_timeouts++;
        return 0;
    }

    /* --- Transaction 2 : le paquet ENTIER, en-tête relu ------------------- */
    const unsigned want = (total > sizeof(s_staging)) ? (unsigned)sizeof(s_staging) : total;
    if (spi_rx(s_staging, want) != ESP_OK) {
        return 0;
    }

    if (total > len) {
        /* Plus grand que ce que la pile peut recevoir. Le paquet a tout de même
         * été lu, donc consommé : ne pas le laisser en attente bloquerait
         * H_INTN et donc tout le flux. */
        s_counters.oversize++;
        ESP_LOGW(TAG, "paquet de %u octets pour un tampon de %u", total, len);
        return 0;
    }

    /* Canal 0, rapport 0x01 : le composant nous dit ce qu'il nous reproche.
     * C'est la seule voie par laquelle il le dit ; la pile SH-2 vendue ne
     * s'abonne pas au canal 0 et jetterait ce paquet en silence. */
    if (s_staging[2] == 0u && total >= 5u && s_staging[4] == 0x01u) {
        s_counters.shtp_errors++;
        if (s_counters.shtp_errors <= 8u || (s_counters.shtp_errors % 2000u) == 0u) {
            char codes[3 * 16 + 1] = {0};   /* liste vide : reste une chaine valide */
            int n = 0;
            for (unsigned i = 5u; i < total && i < 5u + 16u; ++i) {
                n += snprintf(codes + n, sizeof(codes) - (size_t)n, "%02x ", s_staging[i]);
            }
            ESP_LOGE(TAG, "liste d'erreurs SHTP #%u (%u octets) : %s",
                     (unsigned)s_counters.shtp_errors, total - 5u, codes);
        }
    }

    memcpy(pBuffer, s_staging, total);
    s_counters.packets++;
    dump_packet(total, len);
    return (int)total;
}

static int hal_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len)
{
    (void)self;
    if (!s_hal.open || len == 0u) {
        return 0;
    }

    /* Le composant annonce par H_INTN qu'il est prêt à échanger. On attend, on
     * écrit, et on ne touche à rien d'autre — surtout pas à PS0, qui est aussi
     * la broche de sélection de protocole. */
    if (!wait_intn(INTN_WAIT_MS)) {
        s_counters.wake_timeouts++;
        if (s_counters.wake_timeouts == 1u) {
            ESP_LOGW(TAG, "ecriture : H_INTN jamais bas (compteur en diagnostic)");
        }
        return 0;
    }

    if (spi_tx(pBuffer, len) != ESP_OK) {
        return 0;
    }
    s_counters.writes++;
    return (int)len;
}

static uint32_t hal_get_time_us(sh2_Hal_t *self)
{
    (void)self;
    return (uint32_t)esp_timer_get_time();
}

/* --------------------------------------------------------------------------
 *  Construction
 * ----------------------------------------------------------------------- */

sh2_Hal_t *rt_sh2_hal_init(const rt_sh2_hal_pins_t *pins, int spi_host, int clock_hz)
{
    memset(&s_hal, 0, sizeof(s_hal));
    s_hal.pins = *pins;

    s_hal.intn = xSemaphoreCreateBinary();
    if (s_hal.intn == NULL) {
        return NULL;
    }

    const gpio_config_t outs = {
        .pin_bit_mask = (1ULL << pins->rstn) | (1ULL << pins->ps0),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    if (gpio_config(&outs) != ESP_OK) {
        return NULL;
    }
    /* État sûr d'emblée : composant maintenu en reset, PS0 haut. */
    gpio_set_level((gpio_num_t)pins->rstn, 0);
    gpio_set_level((gpio_num_t)pins->ps0, 1);

    const gpio_config_t in = {
        .pin_bit_mask = (1ULL << pins->intn),
        .mode = GPIO_MODE_INPUT,
        /* ⚠️ Sur ESP32 classique, GPIO34-39 n'ont PAS de tirage interne : si
         * H_INTN atterrit sur l'une d'elles, il faut un 10 kΩ externe (§C14).
         * Le tirage demandé ici est donc une ceinture, pas une bretelle. */
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    if (gpio_config(&in) != ESP_OK) {
        return NULL;
    }

    /* Le service d'interruption GPIO peut déjà être installé par ailleurs :
     * ESP_ERR_INVALID_STATE n'est pas une erreur dans ce cas. */
    const esp_err_t isr_err = gpio_install_isr_service(0);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        return NULL;
    }
    if (gpio_isr_handler_add((gpio_num_t)pins->intn, intn_isr, NULL) != ESP_OK) {
        return NULL;
    }

    const spi_bus_config_t bus = {
        .mosi_io_num = pins->mosi,
        .miso_io_num = pins->miso,
        .sclk_io_num = pins->sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 1024,
    };
    esp_err_t err = spi_bus_initialize((spi_host_device_t)spi_host, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return NULL;
    }

    const spi_device_interface_config_t dev = {
        /* Mode 3 : CPOL = 1, CPHA = 1 ✅ datasheet. Se tromper de mode donne un
         * bus qui répond n'importe quoi sans jamais signaler d'erreur. */
        .mode = 3,
        .clock_speed_hz = clock_hz,
        .spics_io_num = pins->cs,
        .queue_size = 4,
        /* Pas de `cs_ena_pretrans` : la documentation ESP-IDF précise qu'il
         * n'agit que sur les transactions half-duplex. Ici on est en
         * full-duplex, il serait ignoré en silence — et un paramètre qui
         * semble donner un temps d'établissement de CS sans le donner est pire
         * que pas de paramètre du tout. `cs_ena_posttrans`, lui, s'applique. */
        .cs_ena_posttrans = 2,
    };
    if (spi_bus_add_device((spi_host_device_t)spi_host, &dev, &s_hal.spi) != ESP_OK) {
        return NULL;
    }

    s_hal.hal.open = hal_open;
    s_hal.hal.close = hal_close;
    s_hal.hal.read = hal_read;
    s_hal.hal.write = hal_write;
    s_hal.hal.getTimeUs = hal_get_time_us;

    ESP_LOGI(TAG, "spi%d a %d Hz, mode 3, cs=%d intn=%d rst=%d ps0=%d", spi_host,
             clock_hz, pins->cs, pins->intn, pins->rstn, pins->ps0);
    return &s_hal.hal;
}
