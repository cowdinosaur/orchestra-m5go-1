// src/main.c — Role-aware entrypoint (conductor silent, performers react to ESP-NOW)

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

#include "espnow_comm.h"          // espnow_init(), espnow_broadcast()
#include "device_config.h"        // device_config_* , ROLE_*
#include "orchestra.h"            // orchestra_init(), orchestra_stop() (and your message hooks)
#include "display_animations.h"   // display_animations_* (idle blue / EQ during playback)
#include "songs.h"                // total_songs

static const char *TAG = "MAIN";

// If you pass -DDEVICE_ROLE=ROLE_PART_1 (etc.) in platformio.ini, it gets honored here.
// Otherwise stays ROLE_UNKNOWN and device_config_init() will pick method behavior.
#ifndef DEVICE_ROLE
#define DEVICE_ROLE ROLE_UNKNOWN
#endif

// M5Stack Core buttons (input-only ADC pins with external pull-ups)
#define BTN_LEFT_GPIO    39   // Button A
#define BTN_MID_GPIO     38   // Button B
#define BTN_RIGHT_GPIO   37   // Button C

static inline bool btn_read(int gpio) { return gpio_get_level(gpio) == 0; } // pressed = LOW

static void buttons_init(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BTN_LEFT_GPIO) |
                        (1ULL << BTN_MID_GPIO)  |
                        (1ULL << BTN_RIGHT_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0,            // no internal pull-ups on these ADC pins; board has externals
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

// Optional debug task to log raw button levels
static void button_debug_task(void *pv) {
    int last_left = 1, last_mid = 1, last_right = 1;
    while (1) {
        int left = gpio_get_level(BTN_LEFT_GPIO);
        int mid  = gpio_get_level(BTN_MID_GPIO);
        int right= gpio_get_level(BTN_RIGHT_GPIO);
        if (left != last_left || mid != last_mid || right != last_right) {
            ESP_LOGI(TAG, "BTN_RAW L=%d M=%d R=%d", left, mid, right);
            last_left = left; last_mid = mid; last_right = right;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    // NVS needed by device_config and esp-now peer storage
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "Booting Orchestra");

    // Initialize role framework (AUTO_ASSIGN or whatever your config uses)
    ESP_ERROR_CHECK(device_config_init(CONFIG_METHOD_AUTO_ASSIGN));

    // If a compile-time DEVICE_ROLE was provided, enforce it now
#if (DEVICE_ROLE != ROLE_UNKNOWN)
    device_config_set_role((device_role_t)DEVICE_ROLE);
#endif

    device_role_t role = device_config_get_role();
    ESP_LOGI(TAG, "Resolved device role: %s (%d)", device_config_get_role_name(role), (int)role);

    // Bring up the app subsystems (your implementation should start display + esp-now stacks)
    ESP_ERROR_CHECK(orchestra_init());

    // Animation engine is already initialized in orchestra_init() -> display_init()
    // Just start the idle animation
    display_animations_start_idle();

    // Initialize buttons for all devices (conductor and performers)
    buttons_init();
    xTaskCreate(button_debug_task, "btn_dbg", 2048, NULL, 5, NULL);

    // Conductor: silent control surface that broadcasts start/stop and song index
    if (role == ROLE_CONDUCTOR) {

        int  song_index = 0;
        bool playing    = false;
        bool prev_left = false, prev_mid = false, prev_right = false;

        ESP_LOGI(TAG, "Conductor ready. A=STOP, B=Start/Stop, C=Next song");

        while (1) {
            bool left  = btn_read(BTN_LEFT_GPIO);
            bool mid   = btn_read(BTN_MID_GPIO);
            bool right = btn_read(BTN_RIGHT_GPIO);

            // A (left): STOP
            if (left && !prev_left) {
                if (playing) {
                    ESP_LOGI(TAG, "Broadcast STOP");
                    espnow_broadcast(MSG_SYNC_STOP, 0);
                    playing = false;
                    display_animations_start_idle();
                } else {
                    ESP_LOGI(TAG, "Stop pressed but nothing playing");
                }
            }

            // B (middle): toggle start/stop
            if (mid && !prev_mid) {
                if (!playing) {
                    ESP_LOGI(TAG, "Broadcast START, song=%d", song_index);
                    espnow_broadcast(MSG_SYNC_START, (uint8_t)song_index);
                    playing = true;
                    // performers will show EQ; conductor stays in idle blue
                } else {
                    ESP_LOGI(TAG, "Broadcast STOP");
                    espnow_broadcast(MSG_SYNC_STOP, 0);
                    playing = false;
                    display_animations_start_idle();
                }
            }

            // C (right): next song (wrap)
            if (right && !prev_right) {
                song_index = (song_index + 1) % total_songs;
                ESP_LOGI(TAG, "Selected song %d", song_index);
                // If currently playing, restart new selection
                if (playing) {
                    espnow_broadcast(MSG_SYNC_STOP, 0);
                    vTaskDelay(pdMS_TO_TICKS(30));
                    ESP_LOGI(TAG, "Broadcast START, song=%d", song_index);
                    espnow_broadcast(MSG_SYNC_START, (uint8_t)song_index);
                }
            }

            prev_left  = left;
            prev_mid   = mid;
            prev_right = right;

            vTaskDelay(pdMS_TO_TICKS(15)); // ~66Hz scan
        }
    }

    // Performers: Button B plays test sound for speaker debugging
    ESP_LOGI(TAG, "Performer mode: B button plays test sound for speaker check");

    // Map each part to their solo song for testing
    uint8_t test_song = SONG_BLUE_BELLS;  // Default
    switch (role) {
        case ROLE_PART_1: test_song = SONG_BLUE_BELLS; break;
        case ROLE_PART_2: test_song = SONG_CARNIVAL_VAR1; break;
        case ROLE_PART_3: test_song = SONG_CARNIVAL_THEME; break;
        case ROLE_PART_4: test_song = SONG_MEDALLION_CALLS; break;
        default: break;
    }

    bool prev_mid = false;
    bool test_playing = false;

    while (1) {
        bool mid = btn_read(BTN_MID_GPIO);

        // B button: toggle test sound
        if (mid && !prev_mid) {
            if (!test_playing) {
                ESP_LOGI(TAG, "Part %d: Playing test song %d for speaker check", role, test_song);
                orchestra_play_song(test_song);
                test_playing = true;
            } else {
                ESP_LOGI(TAG, "Part %d: Stopping test song", role);
                orchestra_stop();
                test_playing = false;
            }
        }

        prev_mid = mid;
        vTaskDelay(pdMS_TO_TICKS(15)); // ~66Hz scan
    }
}
