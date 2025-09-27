#ifndef DISPLAY_ANIMATIONS_H
#define DISPLAY_ANIMATIONS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "orchestra.h"

// M5Stack display dimensions
#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 240

// Animation types
typedef enum {
    ANIM_IDLE_STARS,
    ANIM_IDLE_WAVE,
    ANIM_IDLE_RAINBOW,
    ANIM_PLAY_EQUALIZER,
    ANIM_PLAY_CIRCLES,
    ANIM_PLAY_PARTICLES,
    ANIM_PLAY_WAVE_SYNC,
    ANIM_PLAY_SPIRAL,
    ANIM_PLAY_FIREWORKS
} animation_type_t;

// Particle structure for particle effects
typedef struct {
    float x, y;
    float vx, vy;
    uint16_t color;
    uint8_t life;
    bool active;
} particle_t;

// Star structure for starfield
typedef struct {
    float x, y, z;
    float speed;
} star_t;

// Animation context
typedef struct {
    animation_type_t type;
    uint32_t frame;
    uint32_t start_time;
    bool active;
    song_type_t song_type;
    float beat_intensity;
    uint8_t device_role;
} animation_context_t;

// Function declarations
esp_err_t display_animations_init(void);
void display_animations_start_idle(void);
void display_animations_start_playback(song_type_t song_type);
void display_animations_stop(void);
void display_animations_set_song_name(const char* name);
void display_animations_update_beat(float intensity);
void display_set_brightness(uint8_t brightness);

// Individual animation functions
void anim_draw_starfield(uint32_t frame);
void anim_draw_wave_pattern(uint32_t frame, uint16_t color);
void anim_draw_rainbow_cycle(uint32_t frame);
void anim_draw_equalizer(uint32_t frame, float intensity);
void anim_draw_circles_sync(uint32_t frame, song_type_t type);
void anim_draw_particles(uint32_t frame, uint16_t color);
void anim_draw_spiral(uint32_t frame, uint16_t color);
void anim_draw_fireworks(uint32_t frame);

#endif // DISPLAY_ANIMATIONS_H