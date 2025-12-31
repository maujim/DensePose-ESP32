/**
 * @file led_status.c
 * @brief WS2812 RGB LED status indicator implementation
 *
 * Uses RMT (Remote Control) peripheral for precise WS2812 timing.
 * The WS2812 requires 800kHz data with specific pulse widths.
 */

#include "led_status.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "led_status";

// Hardware configuration
#define LED_GPIO_PIN         21     // WS2812 data pin on ESP32-S3-Zero
#define RMT_LED_CHANNEL      0      // RMT channel to use

// WS2812 timing (nanoseconds) for 800kHz protocol
#define T0H_NS   350    // 0 bit high time
#define T0L_NS   800    // 0 bit low time
#define T1H_NS   700    // 1 bit high time
#define T1L_NS   600    // 1 bit low time
#define RESET_NS 50000  // Reset/latch time

// LED colors (GRB format for WS2812)
#define COLOR_RED     0x00FF00   // G=0, R=255, B=0
#define COLOR_GREEN   0xFF0000   // G=255, R=0, B=0
#define COLOR_BLUE    0x0000FF   // G=0, R=0, B=255
#define COLOR_OFF     0x000000

// RMT configuration
#define RMT_FREQ_HZ     80000000  // 80MHz RMT clock

// State
static rmt_channel_handle_t s_led_channel = NULL;
static rmt_encoder_handle_t s_led_encoder = NULL;
static led_status_t s_current_status = LED_STATUS_WIFI_DISCONNECTED;
static uint32_t s_csi_tick_count = 0;
static uint32_t s_last_blink_time = 0;

/**
 * @brief Simple WS2812 encoder using RMT copy encoder
 *
 * WS2812 protocol:
 * - Each bit: high pulse + low pulse
 * - 0 bit: 350ns high, 800ns low
 * - 1 bit: 700ns high, 600ns low
 * - 24 bits per LED (GRB), MSB first
 * - 50us+ reset/latch between frames
 */

// Convert RGB to GRB for WS2812
static uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
}

// Set LED color
static esp_err_t led_set_color(uint32_t grb_color)
{
    if (s_led_channel == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Convert color to RMT symbols
    // Each bit becomes 2 RMT items (high + low)
    rmt_symbol_word_t symbols[48];  // 24 bits * 2
    int symbol_idx = 0;

    // MSB first for each color component
    for (int i = 23; i >= 0; i--) {
        bool bit = (grb_color >> i) & 1;
        uint16_t t0h_ticks = (T0H_NS * RMT_FREQ_HZ) / 1000000000;
        uint16_t t0l_ticks = (T0L_NS * RMT_FREQ_HZ) / 1000000000;
        uint16_t t1h_ticks = (T1H_NS * RMT_FREQ_HZ) / 1000000000;
        uint16_t t1l_ticks = (T1L_NS * RMT_FREQ_HZ) / 1000000000;

        if (bit) {
            symbols[symbol_idx].duration0 = t1h_ticks;
            symbols[symbol_idx].level0 = 1;
            symbols[symbol_idx].duration1 = t1l_ticks;
            symbols[symbol_idx].level1 = 0;
        } else {
            symbols[symbol_idx].duration0 = t0h_ticks;
            symbols[symbol_idx].level0 = 1;
            symbols[symbol_idx].duration1 = t0l_ticks;
            symbols[symbol_idx].level1 = 0;
        }
        symbol_idx++;
    }

    // Transmit
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    return rmt_transmit(s_led_channel, symbols, sizeof(symbols), &tx_config);
}

esp_err_t led_status_init(void)
{
    ESP_LOGI(TAG, "Initializing WS2812 RGB LED on GPIO %d...", LED_GPIO_PIN);

    // RMT TX channel configuration
    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num = LED_GPIO_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_FREQ_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };

    esp_err_t ret = rmt_new_tx_channel(&tx_chan_config, &s_led_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Enable the channel
    ret = rmt_enable(s_led_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RMT channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initial color test
    led_set_color(COLOR_RED);
    vTaskDelay(pdMS_TO_TICKS(200));
    led_set_color(COLOR_GREEN);
    vTaskDelay(pdMS_TO_TICKS(200));
    led_set_color(COLOR_BLUE);
    vTaskDelay(pdMS_TO_TICKS(200));
    led_set_color(COLOR_OFF);

    ESP_LOGI(TAG, "RGB LED initialized");
    return ESP_OK;
}

esp_err_t led_status_set(led_status_t status)
{
    s_current_status = status;
    s_csi_tick_count = 0;
    return ESP_OK;
}

esp_err_t led_status_csi_tick(void)
{
    s_csi_tick_count++;
    return ESP_OK;
}

void led_status_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LED status task started");

    bool led_on = false;
    uint32_t last_state_change = xTaskGetTickCount();

    while (1) {
        uint32_t now = xTaskGetTickCount();
        uint32_t ms_since_last = (now - last_state_change) * portTICK_PERIOD_MS;

        switch (s_current_status) {
            case LED_STATUS_WIFI_DISCONNECTED:
                // Red slow blink (500ms on, 500ms off)
                if (ms_since_last >= 500) {
                    led_on = !led_on;
                    led_set_color(led_on ? COLOR_RED : COLOR_OFF);
                    last_state_change = now;
                }
                break;

            case LED_STATUS_WIFI_CONNECTED:
                // Blue slow pulse (200ms on, 800ms off)
                if (ms_since_last >= (led_on ? 200 : 800)) {
                    led_on = !led_on;
                    led_set_color(led_on ? COLOR_BLUE : COLOR_OFF);
                    last_state_change = now;
                }
                break;

            case LED_STATUS_CSI_ACTIVE:
                // Green blink on CSI activity
                // If we received CSI packets recently, blink briefly
                if (s_csi_tick_count > 0) {
                    led_set_color(COLOR_GREEN);
                    vTaskDelay(pdMS_TO_TICKS(20));  // Brief flash
                    led_set_color(COLOR_OFF);
                    s_csi_tick_count = 0;  // Reset counter
                }
                // Small idle delay
                vTaskDelay(pdMS_TO_TICKS(50));
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(20));  // 50Hz update rate
    }
}
