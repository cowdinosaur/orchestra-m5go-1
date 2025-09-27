#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "orchestra.h"
#include "songs.h"
#include "espnow_discovery.h"
#include "device_config.h"   // <-- for device_config_get_role(), ROLE_*
#include "config.h"          // Centralized configuration
#include "error_handler.h"   // Error handling macros
#include "audio.h"           // Audio interface
#include "rgb_led.h"         // RGB LED interface
#include "display.h"         // Display interface
#include "display_animations.h" // Animation interface
#include "espnow_comm.h"     // ESP-NOW interface

static const char *TAG = "ORCHESTRA";

// Module interfaces are now included via headers - no extern declarations needed

// Button configuration structure
typedef struct {
    const uint8_t *songs;
    uint8_t count;
    uint8_t *index;
    const char *name;
} button_config_t;

// Device / state
static uint8_t device_id = 0;
static bool is_playing = false;
static SemaphoreHandle_t orchestra_mutex;

// role cache
static device_role_t s_role = ROLE_UNKNOWN;
static bool s_is_conductor = false;

// button task handle so ISR can notify it
static TaskHandle_t s_btn_task_handle = NULL;

// Song rotation for buttons (used by conductor only)
static const uint8_t button_a_songs[] = {SONG_JUPITER_HYMN, SONG_CARNIVAL_THEME};
static const uint8_t button_b_songs[] = {SONG_CANON_IN_D, SONG_CARNIVAL_VAR1, SONG_MEDALLION_CALLS};
static const uint8_t button_c_songs[] = {SONG_BLUE_BELLS, SONG_TV_TIME};
static uint8_t button_a_index = 0;
static uint8_t button_b_index = 0;
static uint8_t button_c_index = 0;

// Button configurations array for unified handling
static button_config_t button_configs[3] = {
    [0] = { button_a_songs, sizeof(button_a_songs)/sizeof(button_a_songs[0]), &button_a_index, "A" },
    [1] = { button_b_songs, sizeof(button_b_songs)/sizeof(button_b_songs[0]), &button_b_index, "B" },
    [2] = { button_c_songs, sizeof(button_c_songs)/sizeof(button_c_songs[0]), &button_c_index, "C" }
};

// If you later want GPIO/NVS based IDs, wire them here
// (previously had a get_device_id helper that was unused; removed to avoid
// -Werror=unused-function build failures)


// -------- Buttons (conductor only) --------
static void IRAM_ATTR button_isr_handler(void *arg) {
    uint32_t btn = (uint32_t)arg; // 0=A,1=B,2=C
    BaseType_t hpw = pdFALSE;
    if (s_btn_task_handle) {
        xTaskNotifyFromISR(s_btn_task_handle, btn, eSetValueWithOverwrite, &hpw);
        if (hpw == pdTRUE) portYIELD_FROM_ISR();
    }
}

// Unified button handler to eliminate duplication
static void handle_button_press(uint8_t button_id) {
    if (!s_is_conductor || button_id >= 3) return;

    button_config_t *config = &button_configs[button_id];
    ESP_LOGI(TAG, "Button %s pressed", config->name);

    // Get next song in rotation
    uint8_t song_id = config->songs[*config->index];
    *config->index = (*config->index + 1) % config->count;

    // Validate song ID
    if (song_id >= total_songs) {
        ESP_LOGW(TAG, "Invalid song ID %u for button %s", song_id, config->name);
        return;
    }

    // Broadcast to all devices
    esp_err_t err = espnow_broadcast(MSG_SYNC_START, song_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to broadcast song start: %s", esp_err_to_name(err));
        return;
    }

    // Update local visuals
    const song_t *song = &songs[song_id];
    display_animations_start_playback(song->type);
}

static void button_task(void *pvParameters) {
    uint32_t btn;
    TickType_t last_press[3] = {0,0,0};
    const TickType_t debounce = pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS);

    while (1) {
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &btn, portMAX_DELAY) == pdTRUE) {
            TickType_t now = xTaskGetTickCount();
            if (btn < 3 && (now - last_press[btn] >= debounce)) {
                last_press[btn] = now;
                handle_button_press(btn);
            }
        }
    }
}

static void init_buttons(void) {
    // NOTE: GPIOs 37/38/39 are input-only and have NO internal pull-ups.
    // M5Stack board provides external resistors. Keep pull-ups disabled.
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BTN_A_GPIO) | (1ULL << BTN_B_GPIO) | (1ULL << BTN_C_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BTN_A_GPIO, button_isr_handler, (void*)0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BTN_B_GPIO, button_isr_handler, (void*)1));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BTN_C_GPIO, button_isr_handler, (void*)2));

    xTaskCreate(button_task, "button_task", BUTTON_TASK_STACK_SIZE, NULL, BUTTON_TASK_PRIORITY, &s_btn_task_handle);
    ESP_LOGI(TAG, "Buttons initialized (conductor)");
}

// ------------- Public API -------------
esp_err_t orchestra_init(void) {
    ESP_LOGI(TAG, "Initializing Orchestra…");

    orchestra_mutex = xSemaphoreCreateMutex();

    // Load role first; defaults to whatever your device_config picked
    s_role = device_config_get_role();
    s_is_conductor = (s_role == ROLE_CONDUCTOR);
    // Use role value as device_id to avoid collisions: ROLE_CONDUCTOR=0, ROLE_PART_1..4 = 1..4
    device_id = (uint8_t)s_role;
    ESP_LOGI(TAG, "Device role=%d (%s), id=%u",
             (int)s_role, device_config_get_role_name(s_role), device_id);

    // Subsystems
    esp_err_t ret = audio_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize audio: %s", esp_err_to_name(ret));
        return ret;
    }
    rgb_init();
    ret = display_init();  // This already calls display_animations_init internally
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_ERROR_CHECK(espnow_init(device_id));

    // Initialize and start discovery for peer tracking
    ESP_ERROR_CHECK(espnow_discovery_init());
    ESP_ERROR_CHECK(espnow_discovery_start());

    // Only the conductor owns buttons; performers ignore local inputs
    if (s_is_conductor) {
        init_buttons();
    }

    // Default volume from config
    ret = audio_set_volume(AUDIO_DEFAULT_VOLUME);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set default volume: %s", esp_err_to_name(ret));
    }

    // Idle visuals
    display_animations_start_idle();
    rgb_set_all_color(COLOR_IDLE);

    ESP_LOGI(TAG, "Orchestra initialized");
    return ESP_OK;
}

esp_err_t orchestra_play_song(uint8_t song_id) {
    if (song_id >= total_songs) {
        ESP_LOGW(TAG, "Invalid song ID: %u", song_id);
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(orchestra_mutex, portMAX_DELAY);

    if (is_playing) {
        audio_stop();
        is_playing = false;
    }

    const song_t *song = &songs[song_id];
    ESP_LOGI(TAG, "Play request: %s (type=%d) role=%d",
             song->name, song->type, (int)s_role);

    // Log what part(s) this device will play for clarity
    if (!s_is_conductor) {
        uint8_t part_bit = (1U << device_id);
        ESP_LOGI(TAG, "Device %u will play parts matching mask 0x%02X (song parts_mask=0x%02X)",
                 device_id, part_bit, song->parts_mask);
    }

    // LED color per song type
    uint32_t led_color = COLOR_IDLE;
    switch (song->type) {
        case SONG_TYPE_QUINTET: led_color = COLOR_QUINTET; break;
        case SONG_TYPE_DUET:    led_color = COLOR_DUET;    break;
        case SONG_TYPE_SOLO:    led_color = COLOR_SOLO;    break;
        default: break;
    }

    rgb_set_all_color(led_color);
    display_animations_start_playback(song->type);
    // Ensure animations know our role so they pick the right colors
    display_animations_update_beat(0.0f);

    bool should_play = false;

    if (s_is_conductor) {
        // Conductor can play for certain songs (TV Time solo, Canon in D duet)
        // Special handling for conductor audio playback
        if (song_id == SONG_TV_TIME) {
            // TV Time is a solo for the conductor
            should_play = true;
            ESP_LOGI(TAG, "Conductor: playing TV Time solo");
        } else if (song_id == SONG_CANON_IN_D) {
            // Canon in D: Conductor plays with Part 1 (ideally would alternate with Parts 2&4)
            should_play = true;
            ESP_LOGI(TAG, "Conductor: playing Canon in D duet");
        } else {
            // For other songs, conductor is visual-only
            should_play = false;
            ESP_LOGI(TAG, "Conductor: visual-only, no audio output.");
        }
    } else {
        // Performer: decide by song type / parts mask
        if (song->type == SONG_TYPE_QUINTET) {
            // All parts play
            should_play = true;
        } else {
            // SOLO and DUET respect the song's parts_mask. This lets the
            // conductor trigger specific parts on different devices.
            if (device_config_should_play_part(s_role, song->parts_mask)) {
                should_play = true;
            }
        }
    }

    if (should_play) {
        // Use role-aware playback so each performer produces a different part
        esp_err_t audio_ret = audio_play_song_for_role(song_id, (uint8_t)s_role);
        if (audio_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to play song: %s", esp_err_to_name(audio_ret));
            xSemaphoreGive(orchestra_mutex);
            return audio_ret;
        }
        is_playing = true;
    }

    ESP_LOGI(TAG, "Playback decision: should_play=%d (role=%d)", (int)should_play, (int)s_role);

    xSemaphoreGive(orchestra_mutex);
    return ESP_OK;
}

void orchestra_stop(void) {
    xSemaphoreTake(orchestra_mutex, portMAX_DELAY);

    // Performers stop audio; conductor had none
    if (!s_is_conductor) {
        audio_stop();
    }

    display_animations_stop();
    vTaskDelay(pdMS_TO_TICKS(IDLE_ANIMATION_DELAY_MS));
    display_animations_start_idle();
    rgb_set_all_color(COLOR_IDLE);
    is_playing = false;

    ESP_LOGI(TAG, "Stopped");

    xSemaphoreGive(orchestra_mutex);
}

esp_err_t orchestra_set_volume(float volume) {
    return audio_set_volume(volume);
}

// -------- Button handlers (conductor only) --------
// Legacy button handlers now just call the unified handler
void orchestra_handle_button_a(void) {
    handle_button_press(0);
}

void orchestra_handle_button_b(void) {
    handle_button_press(1);
}

void orchestra_handle_button_c(void) {
    handle_button_press(2);
}
