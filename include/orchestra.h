#ifndef ORCHESTRA_H
#define ORCHESTRA_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Song types
typedef enum {
    SONG_TYPE_SOLO = 0,
    SONG_TYPE_DUET,
    SONG_TYPE_QUINTET
} song_type_t;

// LED Colors for different song types (24-bit RGB)
#define COLOR_IDLE      0x3333FF   // Blue
#define COLOR_QUINTET   0x33FF33   // Green
#define COLOR_DUET      0xFFFF33   // Yellow
#define COLOR_SOLO      0xCC33CC   // Purple

// Note structure
typedef struct {
    uint16_t frequency;
    uint16_t duration_ms;
} note_t;

// Part assignment for multi-part songs
typedef struct {
    const note_t* notes;
    uint16_t note_count;
} part_melody_t;

// Song structure
typedef struct {
    const char* name;
    song_type_t type;
    const note_t* notes;          // Default/solo melody
    uint16_t note_count;
    uint8_t parts_mask;           // Bit mask for which parts play
    part_melody_t parts[5];       // Individual part melodies for multi-part songs
} song_t;

// ESP-NOW message types
typedef enum {
    MSG_SYNC_START = 0,
    MSG_SYNC_STOP,
    MSG_SONG_SELECT,
    MSG_HEARTBEAT
} msg_type_t;

// ESP-NOW message structure
typedef struct {
    msg_type_t type;
    uint8_t song_id;
    uint64_t timestamp; // microseconds since boot (esp_timer_get_time())
    uint8_t sender_id;
} espnow_msg_t;

// Function declarations with proper error handling
esp_err_t orchestra_init(void);
esp_err_t orchestra_play_song(uint8_t song_id);
void orchestra_stop(void);
esp_err_t orchestra_set_volume(float volume);
void orchestra_handle_button_a(void);
void orchestra_handle_button_b(void);
void orchestra_handle_button_c(void);

#endif // ORCHESTRA_H