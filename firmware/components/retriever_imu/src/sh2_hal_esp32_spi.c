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
 *  Protocole SPI du BNO08x, tel qu'il se déroule réellement :
 *
 *    lecture   H_INTN passe bas ──► CS bas ──► 4 octets d'en-tête SHTP
 *              longueur = (h0 | h1<<8) & 0x7FFF, en-tête compris
 *              ──► lire (longueur − 4) octets de plus ──► CS haut
 *
 *    écriture  WAKE (PS0) bas ──► attendre H_INTN ──► CS bas ──► écrire
 *              ──► CS haut ──► WAKE haut
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

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
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

typedef struct {
    sh2_Hal_t hal;              /* DOIT rester en premier : sh2 nous rend ce pointeur */
    spi_device_handle_t spi;
    SemaphoreHandle_t intn;
    rt_sh2_hal_pins_t pins;
    bool open;
} esp32_hal_t;

static esp32_hal_t s_hal;

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

static esp_err_t spi_rx(uint8_t *dst, size_t len, bool keep_cs)
{
    if (len == 0u) {
        return ESP_OK;
    }
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8u;
    t.rxlength = len * 8u;
    t.rx_buffer = dst;
    t.flags = keep_cs ? SPI_TRANS_CS_KEEP_ACTIVE : 0;
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
     * jusqu'après la première assertion de H_INTN. PS1 est tiré haut sur la
     * carte ; PS0 est piloté ici parce qu'il sert ensuite de WAKE. */
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

    /* L'horodatage est pris ICI, au plus près de l'événement matériel. C'est
     * tout l'intérêt de mettre l'IMU sur le microcontrôleur plutôt que sur le
     * calculateur (§G.1) : la gigue est celle d'une interruption, pas celle de
     * l'ordonnanceur Linux. */
    if (t_us) {
        *t_us = (uint32_t)esp_timer_get_time();
    }

    if (spi_device_acquire_bus(s_hal.spi, portMAX_DELAY) != ESP_OK) {
        return 0;
    }

    uint8_t header[4] = {0};
    int result = 0;

    if (spi_rx(header, sizeof(header), true) == ESP_OK) {
        const unsigned total = (unsigned)((header[0] | ((unsigned)header[1] << 8)) & 0x7FFFu);
        if (total >= 4u && total <= len) {
            memcpy(pBuffer, header, 4);
            if (spi_rx(pBuffer + 4, total - 4u, false) == ESP_OK) {
                result = (int)total;
            }
        } else if (total > len) {
            /* Paquet plus grand que le tampon fourni par sh2. On le jette
             * proprement : le laisser en attente bloquerait la ligne H_INTN. */
            ESP_LOGW(TAG, "paquet de %u octets pour un tampon de %u", total, len);
            uint8_t sink[64];
            unsigned left = total - 4u;
            while (left > 0u) {
                const unsigned n = left > sizeof(sink) ? (unsigned)sizeof(sink) : left;
                if (spi_rx(sink, n, left > n) != ESP_OK) {
                    break;
                }
                left -= n;
            }
        } else {
            /* total < 4 : en-tête vide, le composant n'avait rien à dire. */
            (void)spi_rx(header, 0, false);
        }
    }

    spi_device_release_bus(s_hal.spi);
    return result;
}

static int hal_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len)
{
    (void)self;
    if (!s_hal.open || len == 0u) {
        return 0;
    }

    /* Réveil : PS0/WAKE bas, puis on attend que le composant annonce qu'il est
     * prêt en assertant H_INTN. */
    gpio_set_level((gpio_num_t)s_hal.pins.ps0, 0);
    const bool ready = wait_intn(INTN_WAIT_MS);
    int written = 0;
    if (ready && spi_tx(pBuffer, len) == ESP_OK) {
        written = (int)len;
    }
    gpio_set_level((gpio_num_t)s_hal.pins.ps0, 1);

    if (!ready) {
        ESP_LOGW(TAG, "ecriture refusee : pas de reveil");
    }
    return written;
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
        .cs_ena_pretrans = 2,
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
