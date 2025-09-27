// src/display_animations.c
// Minimal, low-RAM display animations with role-colored equalizer
#include <string.h>
#include <math.h>
#include <stdio.h>
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
static char current_song_name[64] = "Orchestra M5GO";  // Default text
static uint8_t current_song_index = 0;  // Currently selected song

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

// Simple 5x7 font for scrolling text (basic ASCII characters)
// Each byte represents one column of the character (5 columns per char)
static const uint8_t font_5x7[][5] = {
    [' ' - 32] = {0x00, 0x00, 0x00, 0x00, 0x00},  // Space
    ['!' - 32] = {0x00, 0x00, 0x5F, 0x00, 0x00},  // !
    ['"' - 32] = {0x00, 0x07, 0x00, 0x07, 0x00},  // "
    ['#' - 32] = {0x14, 0x7F, 0x14, 0x7F, 0x14},  // #
    ['(' - 32] = {0x00, 0x1C, 0x22, 0x41, 0x00},  // (
    [')' - 32] = {0x00, 0x41, 0x22, 0x1C, 0x00},  // )
    ['-' - 32] = {0x08, 0x08, 0x08, 0x08, 0x08},  // -
    ['.' - 32] = {0x00, 0x60, 0x60, 0x00, 0x00},  // .
    ['0' - 32] = {0x3E, 0x51, 0x49, 0x45, 0x3E},  // 0
    ['1' - 32] = {0x00, 0x42, 0x7F, 0x40, 0x00},  // 1
    ['2' - 32] = {0x42, 0x61, 0x51, 0x49, 0x46},  // 2
    ['3' - 32] = {0x21, 0x41, 0x45, 0x4B, 0x31},  // 3
    ['4' - 32] = {0x18, 0x14, 0x12, 0x7F, 0x10},  // 4
    ['5' - 32] = {0x27, 0x45, 0x45, 0x45, 0x39},  // 5
    ['6' - 32] = {0x3C, 0x4A, 0x49, 0x49, 0x30},  // 6
    ['7' - 32] = {0x01, 0x71, 0x09, 0x05, 0x03},  // 7
    ['8' - 32] = {0x36, 0x49, 0x49, 0x49, 0x36},  // 8
    ['9' - 32] = {0x06, 0x49, 0x49, 0x29, 0x1E},  // 9
    ['A' - 32] = {0x7E, 0x11, 0x11, 0x11, 0x7E},  // A
    ['B' - 32] = {0x7F, 0x49, 0x49, 0x49, 0x36},  // B
    ['C' - 32] = {0x3E, 0x41, 0x41, 0x41, 0x22},  // C
    ['D' - 32] = {0x7F, 0x41, 0x41, 0x22, 0x1C},  // D
    ['E' - 32] = {0x7F, 0x49, 0x49, 0x49, 0x41},  // E
    ['F' - 32] = {0x7F, 0x09, 0x09, 0x09, 0x01},  // F
    ['G' - 32] = {0x3E, 0x41, 0x49, 0x49, 0x7A},  // G
    ['H' - 32] = {0x7F, 0x08, 0x08, 0x08, 0x7F},  // H
    ['I' - 32] = {0x00, 0x41, 0x7F, 0x41, 0x00},  // I
    ['J' - 32] = {0x20, 0x40, 0x41, 0x3F, 0x01},  // J
    ['K' - 32] = {0x7F, 0x08, 0x14, 0x22, 0x41},  // K
    ['L' - 32] = {0x7F, 0x40, 0x40, 0x40, 0x40},  // L
    ['M' - 32] = {0x7F, 0x02, 0x0C, 0x02, 0x7F},  // M
    ['N' - 32] = {0x7F, 0x04, 0x08, 0x10, 0x7F},  // N
    ['O' - 32] = {0x3E, 0x41, 0x41, 0x41, 0x3E},  // O
    ['P' - 32] = {0x7F, 0x09, 0x09, 0x09, 0x06},  // P
    ['Q' - 32] = {0x3E, 0x41, 0x51, 0x21, 0x5E},  // Q
    ['R' - 32] = {0x7F, 0x09, 0x19, 0x29, 0x46},  // R
    ['S' - 32] = {0x46, 0x49, 0x49, 0x49, 0x31},  // S
    ['T' - 32] = {0x01, 0x01, 0x7F, 0x01, 0x01},  // T
    ['U' - 32] = {0x3F, 0x40, 0x40, 0x40, 0x3F},  // U
    ['V' - 32] = {0x1F, 0x20, 0x40, 0x20, 0x1F},  // V
    ['W' - 32] = {0x3F, 0x40, 0x38, 0x40, 0x3F},  // W
    ['X' - 32] = {0x63, 0x14, 0x08, 0x14, 0x63},  // X
    ['Y' - 32] = {0x07, 0x08, 0x70, 0x08, 0x07},  // Y
    ['Z' - 32] = {0x61, 0x51, 0x49, 0x45, 0x43},  // Z
    ['a' - 32] = {0x20, 0x54, 0x54, 0x54, 0x78},  // a
    ['b' - 32] = {0x7F, 0x48, 0x44, 0x44, 0x38},  // b
    ['c' - 32] = {0x38, 0x44, 0x44, 0x44, 0x20},  // c
    ['d' - 32] = {0x38, 0x44, 0x44, 0x48, 0x7F},  // d
    ['e' - 32] = {0x38, 0x54, 0x54, 0x54, 0x18},  // e
    ['f' - 32] = {0x08, 0x7E, 0x09, 0x01, 0x02},  // f
    ['g' - 32] = {0x0C, 0x52, 0x52, 0x52, 0x3E},  // g
    ['h' - 32] = {0x7F, 0x08, 0x04, 0x04, 0x78},  // h
    ['i' - 32] = {0x00, 0x44, 0x7D, 0x40, 0x00},  // i
    ['j' - 32] = {0x20, 0x40, 0x44, 0x3D, 0x00},  // j
    ['k' - 32] = {0x7F, 0x10, 0x28, 0x44, 0x00},  // k
    ['l' - 32] = {0x00, 0x41, 0x7F, 0x40, 0x00},  // l
    ['m' - 32] = {0x7C, 0x04, 0x18, 0x04, 0x78},  // m
    ['n' - 32] = {0x7C, 0x08, 0x04, 0x04, 0x78},  // n
    ['o' - 32] = {0x38, 0x44, 0x44, 0x44, 0x38},  // o
    ['p' - 32] = {0x7C, 0x14, 0x14, 0x14, 0x08},  // p
    ['q' - 32] = {0x08, 0x14, 0x14, 0x18, 0x7C},  // q
    ['r' - 32] = {0x7C, 0x08, 0x04, 0x04, 0x08},  // r
    ['s' - 32] = {0x48, 0x54, 0x54, 0x54, 0x20},  // s
    ['t' - 32] = {0x04, 0x3F, 0x44, 0x40, 0x20},  // t
    ['u' - 32] = {0x3C, 0x40, 0x40, 0x20, 0x7C},  // u
    ['v' - 32] = {0x1C, 0x20, 0x40, 0x20, 0x1C},  // v
    ['w' - 32] = {0x3C, 0x40, 0x30, 0x40, 0x3C},  // w
    ['x' - 32] = {0x44, 0x28, 0x10, 0x28, 0x44},  // x
    ['y' - 32] = {0x0C, 0x50, 0x50, 0x50, 0x3C},  // y
    ['z' - 32] = {0x44, 0x64, 0x54, 0x4C, 0x44},  // z
};

// Render scrolling text for conductor during playback
static void render_scrolling_text(uint32_t frame, const char* text)
{
    display_begin_frame(DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // Text parameters
    int scale = 3;  // Scale factor for the font (3x = 21 pixels tall)
    int char_width = 6 * scale;  // 5 pixels + 1 space, scaled
    int char_height = 7 * scale;

    // Calculate text width
    int text_len = strlen(text);
    int text_width = text_len * char_width;

    // Three different Y positions for the three text lines
    int text_y_positions[3] = {
        DISPLAY_HEIGHT / 4 - char_height / 2,      // Top (quarter way down)
        DISPLAY_HEIGHT / 2 - char_height / 2,      // Middle
        3 * DISPLAY_HEIGHT / 4 - char_height / 2   // Bottom (three quarters down)
    };

    // Different phase offsets for each line (so they're not aligned)
    int phase_offsets[3] = {
        0,
        text_width / 3,
        2 * text_width / 3
    };

    for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
        // Fill scanline with black background
        for (int x = 0; x < DISPLAY_WIDTH; ++x) {
            scanline_buf[x] = 0;  // Black
        }

        // Check each of the three text positions
        for (int line = 0; line < 3; line++) {
            int text_y = text_y_positions[line];

            // Check if this scanline is within this text line
            if (y >= text_y && y < text_y + char_height) {
                int row = (y - text_y) / scale;  // Which row of the font (0-6)

                // Calculate scroll position with phase offset (4x speed, was 2)
                int scroll_offset = ((frame * 4) + phase_offsets[line]) % (text_width + DISPLAY_WIDTH + 100) - 50;

                // Draw each character
                for (int i = 0; i < text_len; i++) {
                    char c = text[i];
                    if (c < 32 || c > 'z') c = ' ';  // Ensure valid character

                    const uint8_t* char_data = font_5x7[c - 32];
                    int char_x = DISPLAY_WIDTH - scroll_offset + i * char_width;

                    // Draw each column of the character
                    for (int col = 0; col < 5; col++) {
                        uint8_t column_data = char_data[col];

                        // Check if this bit is set for the current row
                        if (column_data & (1 << row)) {
                            // Draw scaled pixels
                            for (int sx = 0; sx < scale; sx++) {
                                int x = char_x + col * scale + sx;
                                if (x >= 0 && x < DISPLAY_WIDTH) {
                                    // Different rainbow offset for each line
                                    float hue = fmodf((i * 30.0f + frame + line * 120.0f), 360.0f);
                                    float h = hue / 60.0f;
                                    float xh = 1.0f - fabsf(fmodf(h, 2.0f) - 1.0f);
                                    float r = 0, g = 0, b = 0;

                                    if (h < 1) { r = 1; g = xh; b = 0; }
                                    else if (h < 2) { r = xh; g = 1; b = 0; }
                                    else if (h < 3) { r = 0; g = 1; b = xh; }
                                    else if (h < 4) { r = 0; g = xh; b = 1; }
                                    else if (h < 5) { r = xh; g = 0; b = 1; }
                                    else { r = 1; g = 0; b = xh; }

                                    scanline_buf[x] = rgb565((uint8_t)(r * 255),
                                                            (uint8_t)(g * 255),
                                                            (uint8_t)(b * 255));
                                }
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

// Render network status with 5 circles showing connected devices
static void render_network_status(uint32_t frame)
{
    // Static variables to track changes
    static uint32_t last_check_frame = 0;
    static bool device_states[5] = {false, false, false, false, false};

    // Check device states every 10 frames (about 400ms at 25fps)
    bool check_states = (frame - last_check_frame >= 10);

    // Get device role to check if conductor
    device_role_t my_role = device_config_get_role();
    bool is_conductor = (my_role == ROLE_CONDUCTOR);

    // Circle positions in a pentagon pattern
    const int center_x = DISPLAY_WIDTH / 2;
    const int center_y = DISPLAY_HEIGHT / 2 + (is_conductor ? 10 : 0);  // Shift down if conductor to make room for selector
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

        // Draw song selector indicator for conductor only (at top of screen)
        if (is_conductor && y >= 10 && y < 25) {
            // Draw 7 small rectangles representing the 7 songs
            const int selector_width = 30;
            const int selector_height = 12;
            const int selector_spacing = 5;
            const int total_width = 7 * selector_width + 6 * selector_spacing;
            const int start_x = (DISPLAY_WIDTH - total_width) / 2;

            for (int song = 0; song < 7; song++) {
                int rect_x = start_x + song * (selector_width + selector_spacing);

                // Draw filled rectangle for selected song, outline for others
                bool is_selected = (song == current_song_index);

                // Draw the rectangle
                if (y == 10 || y == 24) {  // Top and bottom border
                    for (int x = rect_x; x < rect_x + selector_width && x < DISPLAY_WIDTH; x++) {
                        scanline_buf[x] = is_selected ? rgb565(255, 255, 0) : rgb565(60, 60, 60);
                    }
                } else {  // Middle part
                    // Left and right borders
                    if (rect_x < DISPLAY_WIDTH) {
                        scanline_buf[rect_x] = is_selected ? rgb565(255, 255, 0) : rgb565(60, 60, 60);
                    }
                    if (rect_x + selector_width - 1 < DISPLAY_WIDTH) {
                        scanline_buf[rect_x + selector_width - 1] = is_selected ? rgb565(255, 255, 0) : rgb565(60, 60, 60);
                    }

                    // Fill selected rectangle
                    if (is_selected) {
                        for (int x = rect_x + 1; x < rect_x + selector_width - 1 && x < DISPLAY_WIDTH; x++) {
                            // Animated gradient fill
                            uint8_t brightness = (uint8_t)(128 + 127 * sinf((frame + x) * 0.1f));
                            scanline_buf[x] = rgb565(brightness, brightness, 0);
                        }
                    }
                }
            }
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

    // Small delay to ensure display initialization is complete
    vTaskDelay(pdMS_TO_TICKS(200));

    bool prev_active = false;

    while (1) {
        bool active;
        device_role_t role;
        xSemaphoreTake(anim_mutex, portMAX_DELAY);
        active = anim_ctx.active;
        role = anim_ctx.device_role;
        xSemaphoreGive(anim_mutex);

        // Log state changes
        if (active != prev_active) {
            ESP_LOGI(TAG, "Animation state changed: active=%d, role=%d", active, (int)role);
            prev_active = active;
        }

        if (!active) {
            // Idle: Show network status for all devices for now
            render_network_status(frame);
            frame++;
            vTaskDelay(frame_dt);
        } else {
            // Playback: Conductor shows scrolling rainbow bar, others show equalizer
            if (role == ROLE_CONDUCTOR) {
                render_scrolling_text(frame, current_song_name);
            } else {
                render_equalizer_frame(frame);
            }
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
    device_role_t role = anim_ctx.device_role;
    xSemaphoreGive(anim_mutex);
    ESP_LOGI(TAG, "Animations: start playback (type=%d, role=%d, song='%s')",
             (int)song_type, (int)role, current_song_name);
}

void display_animations_stop(void)
{
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    anim_ctx.active = false;
    xSemaphoreGive(anim_mutex);
    ESP_LOGI(TAG, "Animations: stop");
}

void display_animations_set_song_name(const char* name)
{
    if (name) {
        strncpy(current_song_name, name, sizeof(current_song_name) - 1);
        current_song_name[sizeof(current_song_name) - 1] = '\0';
    }
}

void display_animations_set_song_index(uint8_t index)
{
    current_song_index = index;
}

void display_animations_update_beat(float intensity)
{
    if (intensity < 0.f) intensity = 0.f;
    if (intensity > 1.f) intensity = 1.f;
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    anim_ctx.beat_intensity = intensity;
    xSemaphoreGive(anim_mutex);
}
