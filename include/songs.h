#ifndef SONGS_H
#define SONGS_H

#include <stdint.h>
#include "orchestra.h"

// Numeric Song IDs (used by main.c to pick songs)
#define SONG_JUPITER_HYMN       0
#define SONG_CANON_IN_D         1
#define SONG_CARNIVAL_THEME     2
#define SONG_CARNIVAL_VAR1      3
#define SONG_BLUE_BELLS         4
#define SONG_MEDALLION_CALLS    5
#define SONG_TV_TIME            6

// Note frequencies (Hz)
#define NOTE_C3  131
#define NOTE_D3  147
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_G3  196
#define NOTE_A3  220
#define NOTE_AS3 233 // Calculated (A-sharp 3 / B-flat 3)
#define NOTE_B3  247

#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_DS4 311 // Calculated (D-sharp 4 / E-flat 4)
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494

#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988

#define NOTE_C6  1047
#define NOTE_D6  1175
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_G6  1568
#define NOTE_A6  1760

#define REST     0

// Note duration helpers (in milliseconds)
#define WHOLE_NOTE      2000
#define HALF_NOTE       1000
#define QUARTER_NOTE    500
#define EIGHTH_NOTE     250
#define SIXTEENTH_NOTE  125

// Part masks for multi-part songs
#define PART_1     0x01  // Device role 1
#define PART_2     0x02  // Device role 2
#define PART_3     0x04  // Device role 3
#define PART_4     0x08  // Device role 4
// Note: Conductor (role 0) doesn't use parts_mask - needs special handling
#define ALL_PARTS  0x0F  // All performer parts (1-4)

// External song arrays (must be defined in songs.c)
extern const song_t songs[];
extern const uint8_t total_songs;

#endif // SONGS_H
