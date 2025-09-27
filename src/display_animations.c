// src/display_animations.c
// Minimal, low-RAM display animations with role-colored equalizer
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"

#include "display_animations.h"
#include "device_config.h"
#include "espnow_discovery.h"

#ifndef DISPLAY_WIDTH
#define DISPLAY_WIDTH  320
#endif
#ifndef DISPLAY_HEIGHT
#define DISPLAY_HEIGHT 240
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *TAG = "DISPLAY_ANIM";

// Minimal animation context
static animation_context_t anim_ctx;
static SemaphoreHandle_t anim_mutex = NULL;

// One scanline buffer reused for pushes
static uint16_t scanline_buf[DISPLAY_WIDTH];

// Weak drawing hooks — implemented in display.c
__attribute__((weak)) void display_begin_frame(uint16_t w, uint16_t h) {(void)w;(void)h;}
__attribute__((weak)) void display_push_row(uint16_t y, const uint16_t *row, uint16_t w) {(void)y;(void)row;(void)w;}
__attribute__((weak)) void display_end_frame(void) {(void)0;}
__attribute__((weak)) void display_push_framebuffer(const uint16_t *fb, uint16_t w, uint16_t h) {(void)fb;(void)w;(void)h;}

// Cheap RGB565 helper
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3));
}

// Role color mapping (tuned to pop against dark bg during animation)
static inline void role_base_color(device_role_t role, uint8_t *r, uint8_t *g, uint8_t *b) {
    switch (role) {
        case ROLE_PART_1:    *r =  40; *g = 255; *b =  40; break;  // Green
        case ROLE_PART_2:    *r = 255; *g = 255; *b =  60; break;  // Yellow
        case ROLE_PART_3:    *r =  60; *g = 210; *b = 255; break;  // Cyan/Blue
        case ROLE_PART_4:    *r = 255; *g = 140; *b =  60; break;  // Orange
        case ROLE_CONDUCTOR: *r = 190; *g =  60; *b = 190; break;  // Purple
        default:             *r = 180; *g = 120; *b = 200; break;  // Fallback
    }
}

// Render one equalizer frame; color & intensity derive from (fixed) role + beat
static void render_equalizer_frame(uint32_t frame)
{
    const int bars = 12;
    const int gap = 2;
    const int bar_w = (DISPLAY_WIDTH - (bars + 1) * gap) / bars;

    // Snapshot shared state once
    float beat_f;
    device_role_t role;
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    beat_f = anim_ctx.beat_intensity;
    role   = anim_ctx.device_role;  // fixed at init
    xSemaphoreGive(anim_mutex);

    // Beat in 0..1000 fixed-point
    uint32_t base_i = (beat_f <= 0.0f) ? 0u : (beat_f >= 1.0f ? 1000u : (uint32_t)(beat_f * 1000.0f + 0.5f));

    // Base color per role
    uint8_t base_r, base_g, base_b;
    role_base_color(role, &base_r, &base_g, &base_b);

    display_begin_frame(DISPLAY_WIDTH, DISPLAY_HEIGHT);

    for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
        // Dark background for EQ (separate from idle blue)
        uint16_t bg = rgb565(10, 10, 30);
        for (int x = 0; x < DISPLAY_WIDTH; ++x) scanline_buf[x] = bg;

        for (int b = 0; b < bars; ++b) {
            // small variance for motion
            uint32_t variance_percent = (uint32_t)((b * 37 + (int)frame) % 101);
            uint32_t multiplier = 500 + (variance_percent * 150) / 100;   // 0.50 .. 0.65
            uint32_t level_percent = (base_i * multiplier) / 1000;        // 0 .. 650

            int height = (int)((level_percent * DISPLAY_HEIGHT) / 1000);
            int bx = gap + b * (bar_w + gap);
            int bar_top = DISPLAY_HEIGHT - height;

            if (y >= bar_top) {
                uint32_t tint_q = 850 + (300 * b) / (bars ? bars : 1);     // 0.85 .. 1.15 (scaled 1000)
                uint32_t temp   = 350 + (650 * level_percent) / 1000;      // 0.35 .. 1.00 (scaled 1000)

                uint32_t comp_r = (uint32_t)base_r * temp * tint_q / 1000000u;
                uint32_t comp_g = (uint32_t)base_g * temp * tint_q / 1000000u;
                uint32_t comp_b = (uint32_t)base_b * temp * tint_q / 1000000u;
                if (comp_r > 255) comp_r = 255;
                if (comp_g > 255) comp_g = 255;
                if (comp_b > 255) comp_b = 255;

                uint16_t col = rgb565((uint8_t)comp_r, (uint8_t)comp_g, (uint8_t)comp_b);
                for (int px = 0; px < bar_w; ++px) {
                    int x = bx + px;
                    if ((unsigned)x < DISPLAY_WIDTH) scanline_buf[x] = col;
                }
            }
        }

        display_push_row(y, scanline_buf, DISPLAY_WIDTH);
        if ((y & 7) == 0) vTaskDelay(0);
    }

    display_end_frame();
}

// Removed unused draw_char_at function - character drawing is now inline in render_network_status

// Render network status with 5 circles showing connected devices
static void render_network_status(uint32_t frame)
{
    // Static variables to track changes and reduce redraws
    static uint32_t last_full_redraw = 0;
    static bool device_states[5] = {false, false, false, false, false};
    static bool prev_device_states[5] = {false, false, false, false, false};

    // Only do full redraw every 2 seconds or on state change
    bool needs_full_redraw = (frame - last_full_redraw > 50);

    // Circle positions in a pentagon pattern
    const int center_x = DISPLAY_WIDTH / 2;
    const int center_y = DISPLAY_HEIGHT / 2;
    const int pattern_radius = 60;  // Distance from center to each circle
    const int circle_radius = 25;  // Fixed radius, no pulsing

    // Pentagon angles (72 degrees apart, starting from top)
    const float angles[5] = {
        -M_PI/2,                    // Top (Conductor)
        -M_PI/2 + 2*M_PI/5,        // Top-right (Part 1)
        -M_PI/2 + 4*M_PI/5,        // Bottom-right (Part 2)
        -M_PI/2 + 6*M_PI/5,        // Bottom-left (Part 3)
        -M_PI/2 + 8*M_PI/5         // Top-left (Part 4)
    };

    // Role labels
    const char role_labels[5] = {'C', '1', '2', '3', '4'};

    // Get device status
    device_role_t my_role = device_config_get_role();
    const peer_device_t* peers = espnow_discovery_get_peers();

    // Check current device states
    for (int i = 0; i < 5; i++) {
        prev_device_states[i] = device_states[i];
        if (i == (int)my_role) {
            device_states[i] = true;
        } else {
            device_states[i] = false;
            for (int p = 0; p < 5; p++) {
                if (peers[p].is_online && peers[p].role == (device_role_t)i) {
                    device_states[i] = true;
                    break;
                }
            }
        }
        // Check if any state changed
        if (device_states[i] != prev_device_states[i]) {
            needs_full_redraw = true;
        }
    }

    // Only redraw if needed
    if (!needs_full_redraw) return;

    last_full_redraw = frame;

    // Clear screen with dark blue background
    display_begin_frame(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    uint16_t bg_color = rgb565(10, 10, 30);  // Dark blue background

    for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
        // Fill scanline with background
        for (int x = 0; x < DISPLAY_WIDTH; ++x) {
            scanline_buf[x] = bg_color;
        }

        // Draw circles for this scanline
        for (int i = 0; i < 5; i++) {
            int cx = center_x + (int)(pattern_radius * cosf(angles[i]));
            int cy = center_y + (int)(pattern_radius * sinf(angles[i]));

            bool is_online = device_states[i];
            uint16_t circle_color;
            uint16_t text_color = rgb565(255, 255, 255);  // White text

            // Set colors based on device state
            if (i == (int)my_role) {
                // Our own device - solid green
                circle_color = rgb565(0, 200, 0);
            } else if (is_online) {
                // Connected peer - solid cyan
                circle_color = rgb565(0, 150, 150);
            } else {
                // Offline device - dark gray
                circle_color = rgb565(30, 30, 30);
                text_color = rgb565(100, 100, 100);  // Dimmer text for offline
            }

            // Check if this scanline intersects with the circle
            int dy = y - cy;
            if (abs(dy) <= circle_radius) {
                int dx = (int)sqrtf((float)(circle_radius * circle_radius - dy * dy));
                int x_start = cx - dx;
                int x_end = cx + dx;

                // Draw the circle segment for this scanline
                for (int x = x_start; x <= x_end; x++) {
                    if (x >= 0 && x < DISPLAY_WIDTH) {
                        scanline_buf[x] = circle_color;
                    }
                }

                // Draw role label in center of circle
                // Simple large character rendering (9x9 for visibility)
                if (abs(dy) <= 4) {  // Character height region
                    char label = role_labels[i];

                    // Define simple patterns for each character (9 pixels wide)
                    int char_offset = dy + 4;  // 0-8 range

                    if (label == 'C' && char_offset >= 1 && char_offset <= 7) {
                        // Draw 'C' - 7x7 within 9x9
                        const bool c_pattern[7][7] = {
                            {0,1,1,1,1,1,0},
                            {1,1,0,0,0,1,1},
                            {1,1,0,0,0,0,0},
                            {1,1,0,0,0,0,0},
                            {1,1,0,0,0,0,0},
                            {1,1,0,0,0,1,1},
                            {0,1,1,1,1,1,0}
                        };
                        int row = char_offset - 1;
                        for (int col = 0; col < 7; col++) {
                            int px = cx - 3 + col;
                            if (px >= x_start && px <= x_end && px >= 0 && px < DISPLAY_WIDTH) {
                                if (c_pattern[row][col]) {
                                    scanline_buf[px] = text_color;
                                }
                            }
                        }
                    }
                    else if (label >= '1' && label <= '4' && char_offset >= 1 && char_offset <= 7) {
                        // Simplified number rendering
                        int row = char_offset - 1;
                        if (label == '1') {
                            // Draw '1' - simple vertical line with top
                            if (row == 0 || row == 1) {  // Top
                                for (int px = cx - 1; px <= cx; px++) {
                                    if (px >= x_start && px <= x_end && px >= 0 && px < DISPLAY_WIDTH)
                                        scanline_buf[px] = text_color;
                                }
                            } else {  // Vertical line
                                if (cx >= x_start && cx <= x_end && cx >= 0 && cx < DISPLAY_WIDTH)
                                    scanline_buf[cx] = text_color;
                            }
                        }
                        else if (label == '2') {
                            // Draw '2' - top, middle, bottom bars with connections
                            if (row == 0 || row == 3 || row == 6) {  // Horizontal bars
                                for (int px = cx - 2; px <= cx + 2; px++) {
                                    if (px >= x_start && px <= x_end && px >= 0 && px < DISPLAY_WIDTH)
                                        scanline_buf[px] = text_color;
                                }
                            } else if (row < 3 && cx + 2 >= x_start && cx + 2 <= x_end) {
                                scanline_buf[cx + 2] = text_color;  // Top right
                            } else if (row > 3 && cx - 2 >= x_start && cx - 2 <= x_end) {
                                scanline_buf[cx - 2] = text_color;  // Bottom left
                            }
                        }
                        else if (label == '3') {
                            // Draw '3' - three horizontal bars with right side
                            if (row == 0 || row == 3 || row == 6) {  // Horizontal bars
                                for (int px = cx - 2; px <= cx + 2; px++) {
                                    if (px >= x_start && px <= x_end && px >= 0 && px < DISPLAY_WIDTH)
                                        scanline_buf[px] = text_color;
                                }
                            } else if (cx + 2 >= x_start && cx + 2 <= x_end) {
                                scanline_buf[cx + 2] = text_color;  // Right side
                            }
                        }
                        else if (label == '4') {
                            // Draw '4' - left vertical, horizontal middle, right vertical
                            if (row == 3) {  // Horizontal middle
                                for (int px = cx - 2; px <= cx + 2; px++) {
                                    if (px >= x_start && px <= x_end && px >= 0 && px < DISPLAY_WIDTH)
                                        scanline_buf[px] = text_color;
                                }
                            } else if (row < 3 && cx - 2 >= x_start && cx - 2 <= x_end) {
                                scanline_buf[cx - 2] = text_color;  // Left top
                            }
                            if (cx + 2 >= x_start && cx + 2 <= x_end && cx + 2 >= 0 && cx + 2 < DISPLAY_WIDTH) {
                                scanline_buf[cx + 2] = text_color;  // Right full
                            }
                        }
                    }
                }
            }
        }

        display_push_row(y, scanline_buf, DISPLAY_WIDTH);
        if ((y & 7) == 0) vTaskDelay(0);
    }

    display_end_frame();
}

static void animation_task(void *arg)
{
    (void)arg;
    uint32_t frame = 0;
    const TickType_t frame_dt = pdMS_TO_TICKS(40); // ~25 FPS

    while (1) {
        bool active;
        xSemaphoreTake(anim_mutex, portMAX_DELAY);
        active = anim_ctx.active;
        xSemaphoreGive(anim_mutex);

        if (!active) {
            // Idle: Show network status with 5 circles
            render_network_status(frame);
            frame++;
            vTaskDelay(frame_dt);
        } else {
            render_equalizer_frame(frame);
            frame++;
            vTaskDelay(frame_dt);
        }
    }
}

esp_err_t display_animations_init(void)
{
    memset(&anim_ctx, 0, sizeof(anim_ctx));
    anim_mutex = xSemaphoreCreateMutex();
    if (!anim_mutex) {
        ESP_LOGE(TAG, "Failed to create anim mutex");
        return ESP_ERR_NO_MEM;
    }

    // Capture device role ONCE
    anim_ctx.device_role    = device_config_get_role();
    anim_ctx.active         = false;            // start idle (blue)
    anim_ctx.song_type      = SONG_TYPE_SOLO;
    anim_ctx.beat_intensity = 0.0f;

    xTaskCreate(animation_task, "animation_task", 2048, NULL, 3, NULL);
    ESP_LOGI(TAG, "Display animations initialized; role=%d", (int)anim_ctx.device_role);
    return ESP_OK;
}

void display_animations_start_idle(void)
{
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    anim_ctx.active = false;
    anim_ctx.beat_intensity = 0.0f;
    xSemaphoreGive(anim_mutex);
    ESP_LOGI(TAG, "Animations: start idle (blue)");
}

void display_animations_start_playback(song_type_t song_type)
{
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    anim_ctx.active = true;
    anim_ctx.song_type = song_type;
    anim_ctx.beat_intensity = 0.0f;
    xSemaphoreGive(anim_mutex);
    ESP_LOGI(TAG, "Animations: start playback (type=%d)", (int)song_type);
}

void display_animations_stop(void)
{
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    anim_ctx.active = false;
    xSemaphoreGive(anim_mutex);
    ESP_LOGI(TAG, "Animations: stop");
}

void display_animations_update_beat(float intensity)
{
    if (intensity < 0.f) intensity = 0.f;
    if (intensity > 1.f) intensity = 1.f;
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    anim_ctx.beat_intensity = intensity;
    xSemaphoreGive(anim_mutex);
}
