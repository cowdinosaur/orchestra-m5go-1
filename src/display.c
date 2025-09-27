// src/display.c
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "device_config.h"
#include "driver/spi_master.h"

#include "orchestra.h"
#include "display_animations.h"

#define X_OFFSET  0
#define Y_OFFSET  0

#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240

// --- M5Stack Core (ILI9342C) pins ---
#define LCD_MOSI_PIN    23
#define LCD_CLK_PIN     18
#define LCD_CS_PIN      14
#define LCD_DC_PIN      27
#define LCD_RST_PIN     33
#define LCD_BL_PIN      32

#define DISPLAY_SANITY_TEST  0

static const char *TAG = "DISPLAY";
static spi_device_handle_t spi = NULL;

static bool s_spi_bus_inited = false;
static bool s_lcd_inited     = false;

// ─────────── Low-level LCD SPI helpers ───────────
static inline void lcd_write_cmd(uint8_t cmd) {
    spi_transaction_t t = (spi_transaction_t){0};
    t.flags      = SPI_TRANS_USE_TXDATA;
    t.length     = 8;
    t.tx_data[0] = cmd;
    gpio_set_level(LCD_DC_PIN, 0);
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));
}

static inline void lcd_write_data8(uint8_t data) {
    spi_transaction_t t = (spi_transaction_t){0};
    t.flags      = SPI_TRANS_USE_TXDATA;
    t.length     = 8;
    t.tx_data[0] = data;
    gpio_set_level(LCD_DC_PIN, 1);
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));
}

static void lcd_write_bytes(const uint8_t *buf, size_t len, bool is_data) {
    if (!len) return;
    spi_transaction_t t = (spi_transaction_t){0};
    t.length    = (uint32_t)len * 8;
    t.tx_buffer = buf;
    gpio_set_level(LCD_DC_PIN, is_data ? 1 : 0);
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));
}

static inline void lcd_write_data(const void *buf, size_t len_bytes) {
    lcd_write_bytes((const uint8_t *)buf, len_bytes, true);
}

static inline void lcd_write_data16(uint16_t v) {
    spi_transaction_t t = (spi_transaction_t){0};
    t.flags = SPI_TRANS_USE_TXDATA;
    t.length = 16;
    t.tx_data[0] = (uint8_t)(v >> 8);
    t.tx_data[1] = (uint8_t)(v & 0xFF);
    gpio_set_level(LCD_DC_PIN, 1);
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));
}

// Set drawing window by corners (inclusive)
static void lcd_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    x0 += X_OFFSET; y0 += Y_OFFSET; x1 += X_OFFSET; y1 += Y_OFFSET;
    lcd_write_cmd(0x2A); // CASET
    lcd_write_data16(x0); lcd_write_data16(x1);
    lcd_write_cmd(0x2B); // RASET
    lcd_write_data16(y0); lcd_write_data16(y1);
    lcd_write_cmd(0x2C); // RAMWR
}

// DMA-safe line buffer for push helpers
static uint8_t s_txbuf[DISPLAY_WIDTH * 2] __attribute__((aligned(4)));

static inline void lcd_push_rgb565(const uint16_t *pixels, size_t count) {
    if (!pixels || !count) return;
    if (count > DISPLAY_WIDTH) count = DISPLAY_WIDTH;
    for (size_t i = 0; i < count; ++i) {
        uint16_t p = pixels[i];
        s_txbuf[i * 2 + 0] = (uint8_t)(p >> 8);
        s_txbuf[i * 2 + 1] = (uint8_t)(p & 0xFF);
    }
    lcd_write_data(s_txbuf, count * 2);
}

// ------- Row-by-row push helpers for line-mode rendering -------
void display_begin_frame(uint16_t w, uint16_t h) {
    if (!spi) return;
    if (w > DISPLAY_WIDTH)  w = DISPLAY_WIDTH;
    if (h > DISPLAY_HEIGHT) h = DISPLAY_HEIGHT;
    lcd_set_addr_window(0, 0, (uint16_t)(w - 1), (uint16_t)(h - 1));
}

void display_push_row(uint16_t y, const uint16_t *row, uint16_t w) {
    (void)y;
    if (!spi || !row || !w) return;
    lcd_push_rgb565(row, w);
    if ((y & 7) == 0) vTaskDelay(0);
}

void display_end_frame(void) {
    vTaskDelay(0);
}

// Optional helpers
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3));
}

static void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT || !w || !h) return;
    if (x + w > DISPLAY_WIDTH)  w = DISPLAY_WIDTH  - x;
    if (y + h > DISPLAY_HEIGHT) h = DISPLAY_HEIGHT - y;

    lcd_set_addr_window(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));

    static uint16_t line[DISPLAY_WIDTH];
    for (uint16_t i = 0; i < w; ++i) line[i] = color;

    for (uint16_t row = 0; row < h; ++row) {
        lcd_push_rgb565(line, w);
    }
}

// ─────────── Hardware init ───────────
static void display_init_hardware(void) {
    // GPIOs
    gpio_config_t io_conf = (gpio_config_t){
        .pin_bit_mask = (1ULL << LCD_DC_PIN) | (1ULL << LCD_RST_PIN) | (1ULL << LCD_BL_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // SPI bus/device
    spi_bus_config_t buscfg = (spi_bus_config_t){
        .mosi_io_num = LCD_MOSI_PIN,
        .miso_io_num = -1,
        .sclk_io_num = LCD_CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2
    };
    spi_device_interface_config_t devcfg = (spi_device_interface_config_t){
        .clock_speed_hz = 26 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = LCD_CS_PIN,
        .queue_size = 1
    };

    if (!s_spi_bus_inited) {
        esp_err_t r = spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
        if (r == ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "SPI bus already initialized by another module, continuing");
            s_spi_bus_inited = true;
        } else {
            ESP_ERROR_CHECK(r);
            s_spi_bus_inited = true;
        }
    }

    if (spi == NULL) {
        ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfg, &spi));
    }

    // Panel reset + init sequence once
    if (!s_lcd_inited) {
        gpio_set_level(LCD_RST_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(LCD_RST_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(120));

        lcd_write_cmd(0x01); // SWRESET
        vTaskDelay(pdMS_TO_TICKS(120));

        lcd_write_cmd(0x11); // SLPOUT
        vTaskDelay(pdMS_TO_TICKS(120));

        lcd_write_cmd(0x3A); // COLMOD: 16bpp
        lcd_write_data8(0x55);

        lcd_write_cmd(0x21); // INVON - M5Stack display needs inversion ON
        lcd_write_cmd(0x36);
        lcd_write_data8(0x08); // MADCTL BGR

        lcd_write_cmd(0xB1); lcd_write_data8(0x00); lcd_write_data8(0x1B);
        lcd_write_cmd(0xB6); lcd_write_data8(0x0A); lcd_write_data8(0xA2);

        lcd_write_cmd(0x29); // DISPON
        vTaskDelay(pdMS_TO_TICKS(20));

        gpio_set_level(LCD_BL_PIN, 1);
        s_lcd_inited = true;
    }

    ESP_LOGI(TAG, "Display hardware initialized");
}

// Public bridge for full-frame push
void display_push_framebuffer(const uint16_t *fb, uint16_t w, uint16_t h) {
    if (!fb || !w || !h || !spi) return;
    if (w > DISPLAY_WIDTH)  w = DISPLAY_WIDTH;
    if (h > DISPLAY_HEIGHT) h = DISPLAY_HEIGHT;

    lcd_set_addr_window(0, 0, (uint16_t)(w - 1), (uint16_t)(h - 1));
    for (uint16_t y = 0; y < h; ++y) {
        const uint16_t *row = fb + (size_t)y * DISPLAY_WIDTH;
        lcd_push_rgb565(row, w);
    }
}

// Display init
esp_err_t display_init(void) {
    display_init_hardware();

    // Start the lightweight animations task and go to idle (blue)
    esp_err_t ret = display_animations_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init display animations: %s", esp_err_to_name(ret));
        return ret;
    }
    display_animations_start_idle();

#if DISPLAY_SANITY_TEST
    // Optionally cycle modes (debug only)
    // lcd_cycle_color_modes();
#endif

    // Paint a single blue frame immediately so something shows before the task runs
    lcd_fill_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, rgb565(0,0,200));

    ESP_LOGI(TAG, "Display system initialized");
    return ESP_OK;
}
