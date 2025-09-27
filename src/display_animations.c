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

// Render one equalizer frame with colorful rainbow bars on black background
static void render_equalizer_frame(uint32_t frame)
{
    const int bars = 12;
    const int gap = 3;
    const int bar_w = (DISPLAY_WIDTH - (bars + 1) * gap) / bars;

    // Snapshot shared state once
    float beat_f;
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    beat_f = anim_ctx.beat_intensity;
    xSemaphoreGive(anim_mutex);

    // Use beat intensity and add baseline activity (20-80% height range)
    float base_activity = 0.2f + sinf(frame * 0.05f) * 0.1f;  // Gentle wave
    float effective_beat = base_activity + beat_f * 0.6f;
    if (effective_beat > 1.0f) effective_beat = 1.0f;

    display_begin_frame(DISPLAY_WIDTH, DISPLAY_HEIGHT);

    for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
        // Pure black background for maximum contrast
        for (int x = 0; x < DISPLAY_WIDTH; ++x) scanline_buf[x] = 0;

        for (int b = 0; b < bars; ++b) {
            // Create dynamic movement with multiple sine waves
            float phase1 = sinf((frame * 0.1f) + (b * 0.8f));
            float phase2 = cosf((frame * 0.07f) + (b * 1.2f));
            float phase3 = sinf((frame * 0.13f) - (b * 0.5f));

            // Combine waves for organic movement (30-90% of screen height)
            float height_factor = 0.3f + (phase1 * 0.2f + phase2 * 0.15f + phase3 * 0.15f) * 0.5f + effective_beat * 0.3f;
            if (height_factor > 0.9f) height_factor = 0.9f;
            if (height_factor < 0.1f) height_factor = 0.1f;

            int height = (int)(height_factor * DISPLAY_HEIGHT);
            int bx = gap + b * (bar_w + gap);
            int bar_top = DISPLAY_HEIGHT - height;

            if (y >= bar_top) {
                // Rainbow colors based on bar position
                float hue = (float)b / (float)bars * 360.0f + frame * 2.0f;  // Rotate colors

                // Convert HSV to RGB for vibrant colors
                float h = fmodf(hue, 360.0f) / 60.0f;
                float x = 1.0f - fabsf(fmodf(h, 2.0f) - 1.0f);
                float r = 0, g = 0, b = 0;

                if (h < 1) { r = 1; g = x; b = 0; }
                else if (h < 2) { r = x; g = 1; b = 0; }
                else if (h < 3) { r = 0; g = 1; b = x; }
                else if (h < 4) { r = 0; g = x; b = 1; }
                else if (h < 5) { r = x; g = 0; b = 1; }
                else { r = 1; g = 0; b = x; }

                // Add gradient effect - brighter at bottom
                float brightness = 0.6f + ((float)(y - bar_top) / (float)height) * 0.4f;

                uint8_t red = (uint8_t)(r * brightness * 255);
                uint8_t green = (uint8_t)(g * brightness * 255);
                uint8_t blue = (uint8_t)(b * brightness * 255);

                uint16_t col = rgb565(red, green, blue);
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
    // Static variables to track changes
    static uint32_t last_check_frame = 0;
    static bool device_states[5] = {false, false, false, false, false};

    // Check device states every 10 frames (about 400ms at 25fps)
    bool check_states = (frame - last_check_frame >= 10);

    // Circle positions in a pentagon pattern
    const int center_x = DISPLAY_WIDTH / 2;
    const int center_y = DISPLAY_HEIGHT / 2;
    const int pattern_radius = 60;  // Distance from center to each circle
    const int circle_radius = 25;  // Fixed radius

    // Slowly rotating pentagon (one rotation per ~10 seconds)
    float rotation = frame * 0.01f;

    // Pentagon angles (72 degrees apart, starting from top)
    const float base_angles[5] = {
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

    // Only check states when needed
    if (check_states) {
        last_check_frame = frame;
        const peer_device_t* peers = espnow_discovery_get_peers();

        // Check current device states
        for (int i = 0; i < 5; i++) {
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
        }
    }

    // Always redraw for animation (pulsing effect)

    // Clear screen with pure black background
    display_begin_frame(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    uint16_t bg_color = 0;  // Pure black background

    // Calculate rotated positions for all circles
    int circle_x[5], circle_y[5];
    for (int i = 0; i < 5; i++) {
        float angle = base_angles[i] + rotation;
        circle_x[i] = center_x + (int)(pattern_radius * cosf(angle));
        circle_y[i] = center_y + (int)(pattern_radius * sinf(angle));
    }

    for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
        // Fill scanline with background
        for (int x = 0; x < DISPLAY_WIDTH; ++x) {
            scanline_buf[x] = bg_color;
        }

        // Draw connection lines between connected devices (before circles)
        for (int i = 0; i < 5; i++) {
            if (!device_states[i]) continue;
            for (int j = i + 1; j < 5; j++) {
                if (!device_states[j]) continue;

                // Simple line drawing between connected devices
                int x1 = circle_x[i], y1 = circle_y[i];
                int x2 = circle_x[j], y2 = circle_y[j];

                // Check if this scanline intersects with the line
                if ((y >= y1 && y <= y2) || (y >= y2 && y <= y1)) {
                    // Linear interpolation for the line
                    if (y2 != y1) {
                        int x = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
                        // Draw a 2-pixel wide line for visibility
                        if (x >= 0 && x < DISPLAY_WIDTH) {
                            scanline_buf[x] = rgb565(0, 40, 40);  // Dim cyan line
                            if (x + 1 < DISPLAY_WIDTH) scanline_buf[x + 1] = rgb565(0, 40, 40);
                        }
                    }
                }
            }
        }

        // Draw circles for this scanline
        for (int i = 0; i < 5; i++) {
            int cx = circle_x[i];
            int cy = circle_y[i];

            bool is_online = device_states[i];
            uint16_t circle_color;
            uint16_t text_color = rgb565(255, 255, 255);  // White text

            // Set colors based on device state (no pulsing for now to fix rendering)
            if (i == (int)my_role) {
                // Our own device - solid purple
                circle_color = rgb565(150, 0, 150);  // Purple
            } else if (is_online) {
                // Connected peer - solid cyan
                circle_color = rgb565(0, 120, 120);
            } else {
                // Offline device - dark gray
                circle_color = rgb565(20, 20, 20);
                text_color = rgb565(80, 80, 80);  // Dimmer text for offline
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
