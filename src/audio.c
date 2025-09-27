// src/audio.c
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "songs.h"
#include "device_config.h"
#include "display_animations.h"
#include "config.h"
#include "error_handler.h"

static const char *TAG = "AUDIO";

// ----------------------
// Audio configuration
// ----------------------
#define SAMPLE_RATE       AUDIO_SAMPLE_RATE
#define DMA_BUF_COUNT     AUDIO_DMA_BUF_COUNT
#define DMA_BUF_LEN       AUDIO_DMA_BUF_LEN
#define SAMPLES_PER_TICK  (SAMPLE_RATE * AUDIO_TICK_MS / 1000)

// ----------------------
// Audio state
// ----------------------
static bool          audio_playing = false;
static float         volume = AUDIO_DEFAULT_VOLUME;  // 0..1
static TaskHandle_t  playback_task_handle = NULL;

// ----------------------
// Animation helpers
// ----------------------
static inline float clamp01(float x){ return x<0.f?0.f:(x>1.f?1.f:x); }

static float pulse_intensity_for_note(uint16_t freq, uint16_t dur_ms) {
    if (freq == 0) return 0.0f; // rest
    float nf = (float)freq / 1000.0f;        // ~0.2..2.0 typical
    float nd = 220.0f / (float)(dur_ms + 50);// shorter -> stronger
    float base = 0.25f + 0.55f * clamp01(nf);
    float shaped = base * clamp01(nd);
    return clamp01(shaped + 0.10f);          // ~0.1..0.9
}

// ----------------------
// I2S setup
// ----------------------
static esp_err_t audio_init_i2s(void) {
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = true,
    };
    CHECK_ERROR_RETURN(i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL),
                       TAG, "Failed to install I2S driver");
    CHECK_ERROR_RETURN(i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN),
                       TAG, "Failed to set DAC mode");
    CHECK_ERROR_RETURN(i2s_set_pin(I2S_NUM_0, NULL),
                       TAG, "Failed to set I2S pins");

    ESP_LOGI(TAG, "I2S audio initialized (%d Hz, %d-sample tick)", SAMPLE_RATE, SAMPLES_PER_TICK);
    return ESP_OK;
}

// ----------------------
// Tick renderer
// ----------------------
// This generates exactly SAMPLES_PER_TICK samples each tick at 'freq' Hz,
// preserving 'phase' across ticks to keep waveform continuous.
static void render_tick(int16_t *buf, uint16_t freq, float *phase_io) {
    float phase = *phase_io;
    float step  = (freq == 0) ? 0.f : (2.0f * (float)M_PI * (float)freq / (float)SAMPLE_RATE);

    if (freq == 0) {
        // Silence (rest)
        memset(buf, 0, SAMPLES_PER_TICK * sizeof(int16_t));
    } else {
        for (size_t i = 0; i < SAMPLES_PER_TICK; ++i) {
            float s = sinf(phase) * volume;
            // scale to int16
            buf[i] = (int16_t)(s * 32767.0f);
            phase += step;
            if (phase >= 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
        }
    }
    *phase_io = phase;
}

// ----------------------
// Role part selection / transform
// ----------------------
static void select_melody_for_role(const song_t *song, uint8_t role,
                                   const note_t **out_notes, uint16_t *out_count)
{
    // Prefer explicit part if present
    if (role >= ROLE_PART_1 && role <= ROLE_PART_4) {
        int part_idx = (int)role; // ROLE_PART_1 = 1
        if (song->parts[part_idx].notes && song->parts[part_idx].note_count) {
            *out_notes = song->parts[part_idx].notes;
            *out_count = song->parts[part_idx].note_count;
            return;
        }
    }
    // Fallback to lead
    *out_notes = song->notes;
    *out_count = song->note_count;
}

// Simple transform when we fell back to the lead melody (to create harmonies)
static uint16_t transform_freq_for_role(uint16_t base_freq, uint8_t role) {
    if (base_freq == 0) return 0;
    switch (role) {
        case ROLE_PART_1: return base_freq;                 // lead
        case ROLE_PART_2: {                                  // down an octave (if too low, revert)
            uint16_t f = (uint16_t)(base_freq * HARMONY_OCTAVE_DOWN_RATIO);
            return (f < AUDIO_MIN_FREQUENCY) ? base_freq : f;
        }
        case ROLE_PART_3: return (uint16_t)(base_freq * HARMONY_OCTAVE_UP_RATIO); // up an octave
        case ROLE_PART_4: return (uint16_t)(base_freq * HARMONY_FIFTH_RATIO); // perfect fifth
        default:          return base_freq;
    }
}

// ----------------------
// Playback task (tick-based)
// ----------------------
typedef struct {
    uint8_t song_id;
    uint8_t role;
} play_role_param_t;

static void playback_task(void *pv) {
    play_role_param_t *p = (play_role_param_t *)pv;
    uint8_t song_id = p->song_id;
    uint8_t role    = p->role;
    free(p);

    if (song_id >= total_songs) {
        ESP_LOGE(TAG, "Invalid song ID: %u", (unsigned)song_id);
        playback_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    const song_t *song = &songs[song_id];
    const note_t *mel  = NULL;
    uint16_t count     = 0;

    select_melody_for_role(song, role, &mel, &count);
    bool using_lead = (mel == song->notes);

    ESP_LOGI(TAG, "Starting tick playback: '%s' (notes=%u, role=%u)", song->name, (unsigned)count, (unsigned)role);

    audio_playing = true;

    // Start equalizer animation
    display_animations_start_playback(SONG_TYPE_SOLO);

    // One tick buffer (tiny RAM)
    static int16_t tick_buf[SAMPLES_PER_TICK];
    float phase = 0.0f;

    for (uint16_t i = 0; i < count && audio_playing; ++i) {
        uint16_t freq = mel[i].frequency;
        uint16_t dur  = mel[i].duration_ms;

        if (using_lead) {
            freq = transform_freq_for_role(freq, role);
        }

        // Pulse stronger exactly on note edge
        display_animations_update_beat(pulse_intensity_for_note(freq, dur));

        // Convert ms -> ticks (round to nearest, min 1 for any non-zero)
        uint32_t ticks = (dur + (AUDIO_TICK_MS / 2)) / AUDIO_TICK_MS;
        if (freq != 0 && ticks == 0) ticks = 1;

        // Render this note in fixed-sized ticks
        for (uint32_t t = 0; t < ticks && audio_playing; ++t) {
            render_tick(tick_buf, freq, &phase);

            size_t bytes_written = 0;
            // I2S pacing is the wall clock (blocking until DMA has room)
            ESP_ERROR_CHECK(i2s_write(I2S_NUM_0, tick_buf,
                                      SAMPLES_PER_TICK * sizeof(int16_t),
                                      &bytes_written, portMAX_DELAY));

            // very small decay so bars don't stick at peak between ticks of the same note
            // (keeps pulse feel without needing extra RAM)
            if (freq != 0) {
                display_animations_update_beat(0.25f);
            } else {
                display_animations_update_beat(0.0f);
            }
        }

        // Optional tiny gap shaping (prevents harsh transitions), keep zero to disable
        // phase is kept continuous; we don't reset it at note boundaries to avoid clicks
    }

    // Song finished or stopped
    audio_playing = false;
    display_animations_stop();
    display_animations_start_idle();

    ESP_LOGI(TAG, "Playback finished: '%s' (role=%u)", song->name, (unsigned)role);
    playback_task_handle = NULL;
    vTaskDelete(NULL);
}

// ----------------------
// Public API
// ----------------------
esp_err_t audio_init(void) {
    esp_err_t ret = audio_init_i2s();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Audio system initialized successfully");
    return ESP_OK;
}

void audio_stop(void) {
    audio_playing = false;
    if (playback_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (playback_task_handle != NULL) {
            vTaskDelete(playback_task_handle);
            playback_task_handle = NULL;
        }
    }
    display_animations_stop();
    display_animations_start_idle();
    ESP_LOGI(TAG, "Audio stopped");
}

esp_err_t audio_play_song(uint8_t song_id) {
    // Validate input
    if (song_id >= total_songs) {
        ESP_LOGW(TAG, "Invalid song ID: %u", song_id);
        return ESP_ERR_INVALID_ARG;
    }

    audio_stop();

    play_role_param_t *p = (play_role_param_t *)malloc(sizeof(*p));
    if (!p) {
        ESP_LOGE(TAG, "alloc play param failed");
        return ESP_ERR_NO_MEM;
    }
    p->song_id = song_id;
    p->role    = (uint8_t)device_config_get_role();

    // Higher prio than UI; modest stack is enough (tick buffer is static)
    BaseType_t ret = xTaskCreate(playback_task, "playback_tick",
                                 PLAYBACK_TASK_STACK_SIZE, p,
                                 PLAYBACK_TASK_PRIORITY, &playback_task_handle);
    if (ret != pdPASS) {
        free(p);
        ESP_LOGE(TAG, "Failed to create playback task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t audio_play_song_for_role(uint8_t song_id, uint8_t role) {
    // Validate input
    if (song_id >= total_songs) {
        ESP_LOGW(TAG, "Invalid song ID: %u", song_id);
        return ESP_ERR_INVALID_ARG;
    }

    audio_stop();

    play_role_param_t *p = (play_role_param_t *)malloc(sizeof(*p));
    if (!p) {
        ESP_LOGE(TAG, "alloc play param failed");
        return ESP_ERR_NO_MEM;
    }
    p->song_id = song_id;
    p->role    = role;

    BaseType_t ret = xTaskCreate(playback_task, "playback_tick",
                                 PLAYBACK_TASK_STACK_SIZE, p,
                                 PLAYBACK_TASK_PRIORITY, &playback_task_handle);
    if (ret != pdPASS) {
        free(p);
        ESP_LOGE(TAG, "Failed to create playback task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t audio_set_volume(float vol) {
    // Clamp volume to valid range
    if (vol < AUDIO_MIN_VOLUME) vol = AUDIO_MIN_VOLUME;
    if (vol > AUDIO_MAX_VOLUME) vol = AUDIO_MAX_VOLUME;

    volume = vol;
    ESP_LOGI(TAG, "Volume set to %.2f", volume);
    return ESP_OK;
}

bool audio_is_playing(void) { return audio_playing; }
