#include "songs.h"

// Blue Bells of Scotland - Solo for Part 1 only
const note_t blue_bells_notes[] = {
    {NOTE_C5, QUARTER_NOTE}, {NOTE_F5, HALF_NOTE}, {NOTE_E5, QUARTER_NOTE},
    {NOTE_D5, QUARTER_NOTE}, {NOTE_C5, HALF_NOTE}, {NOTE_D5, QUARTER_NOTE},
    {NOTE_E5, EIGHTH_NOTE}, {NOTE_F5, EIGHTH_NOTE}, {NOTE_A4, QUARTER_NOTE},
    {REST, QUARTER_NOTE}, {NOTE_A4, QUARTER_NOTE}, {NOTE_AS4, QUARTER_NOTE},
    {NOTE_G4, QUARTER_NOTE}, {NOTE_F4, HALF_NOTE}, {NOTE_C5, QUARTER_NOTE},
    {NOTE_F5, HALF_NOTE}, {NOTE_E5, QUARTER_NOTE}, {NOTE_D5, QUARTER_NOTE},
    {NOTE_C5, HALF_NOTE}, {NOTE_D5, QUARTER_NOTE}, {NOTE_E5, EIGHTH_NOTE},
    {NOTE_F5, EIGHTH_NOTE}, {NOTE_A4, QUARTER_NOTE}, {REST, QUARTER_NOTE},
    {NOTE_A4, QUARTER_NOTE}, {NOTE_C5, SIXTEENTH_NOTE}, {NOTE_AS4, SIXTEENTH_NOTE},
    {NOTE_AS4, EIGHTH_NOTE}, {NOTE_G4, QUARTER_NOTE}, {NOTE_F4, HALF_NOTE}
};

// Carnival of Venice Theme - Solo for Part 3 only
const note_t carnival_theme_notes[] = {
    // Measures 1-3 (Slow, thematic start)
    {NOTE_AS3, QUARTER_NOTE},   // 233
    {NOTE_D4, QUARTER_NOTE},    // 294
    {NOTE_AS3, QUARTER_NOTE},   // 233
    {NOTE_A3, QUARTER_NOTE},    // 220
    {NOTE_G3, HALF_NOTE},       // 196
    {NOTE_A3, HALF_NOTE},       // 220
    {NOTE_AS3, WHOLE_NOTE},     // 233

    // Measures 4-6 (Ascending/Descending pattern)
    {NOTE_F4, HALF_NOTE},       // 349
    {NOTE_DS4, HALF_NOTE},      // 311
    {NOTE_AS3, HALF_NOTE},      // 233
    {NOTE_A3, HALF_NOTE},       // 220
    {NOTE_D4, WHOLE_NOTE},      // 294

    // Measures 7-9 (Repetition and rapid run setup)
    {NOTE_A3, HALF_NOTE},       // 220
    {NOTE_AS3, QUARTER_NOTE},   // 233
    {NOTE_A3, QUARTER_NOTE},    // 220
    {NOTE_G3, HALF_NOTE},       // 196
    {NOTE_DS4, QUARTER_NOTE},   // 311
    {NOTE_DS4, EIGHTH_NOTE},    // 311 (1/2 unit)
    {NOTE_F4, EIGHTH_NOTE},     // 349 (1/2 unit)
    {NOTE_AS4, WHOLE_NOTE},     // 466

    // Measures 10-12 (Ascending/Descending run)
    {NOTE_D4, HALF_NOTE},       // 294
    {NOTE_DS4, HALF_NOTE},      // 311
    {NOTE_G4, QUARTER_NOTE},    // 392
    {NOTE_A4, QUARTER_NOTE},    // 440
    {NOTE_AS4, QUARTER_NOTE},   // 466
    {NOTE_C5, QUARTER_NOTE},    // 523
    {NOTE_D5, QUARTER_NOTE},    // 587
    {NOTE_C5, QUARTER_NOTE},    // 523
    {NOTE_AS4, QUARTER_NOTE},   // 466
    {NOTE_F4, QUARTER_NOTE},    // 349
    {NOTE_D4, QUARTER_NOTE},    // 294
    {NOTE_F4, QUARTER_NOTE},    // 349
    {NOTE_AS4, HALF_NOTE}       // 466
};


// Carnival of Venice Variation 1 - Solo for Part 2 only
// Note: Lowered some high notes by an octave to prevent speaker clipping
const note_t carnival_var1_notes[] = {
    {NOTE_C5, SIXTEENTH_NOTE}, {NOTE_D5, SIXTEENTH_NOTE}, {NOTE_E5, SIXTEENTH_NOTE}, {NOTE_F5, SIXTEENTH_NOTE},
    {NOTE_G4, SIXTEENTH_NOTE}, {NOTE_A4, SIXTEENTH_NOTE}, {NOTE_F5, SIXTEENTH_NOTE}, {NOTE_E5, SIXTEENTH_NOTE},
    {NOTE_D5, EIGHTH_NOTE}, {NOTE_C5, EIGHTH_NOTE}, {NOTE_F5, QUARTER_NOTE},
    {NOTE_E5, SIXTEENTH_NOTE}, {NOTE_F5, SIXTEENTH_NOTE}, {NOTE_G4, SIXTEENTH_NOTE}, {NOTE_A4, SIXTEENTH_NOTE},
    {NOTE_F5, EIGHTH_NOTE}, {NOTE_E5, EIGHTH_NOTE}, {NOTE_D5, EIGHTH_NOTE},
    {NOTE_C5, QUARTER_NOTE}, {NOTE_AS4, EIGHTH_NOTE}, {NOTE_A4, EIGHTH_NOTE},
    {NOTE_G4, QUARTER_NOTE}, {NOTE_F4, HALF_NOTE}
};

// Medallion Calls - Solo for Part 4 only
const note_t medallion_calls_notes[] = {
    {NOTE_C5, QUARTER_NOTE}, {NOTE_F5, QUARTER_NOTE}, {NOTE_F5, EIGHTH_NOTE},
    {NOTE_F5, EIGHTH_NOTE}, {NOTE_E5, EIGHTH_NOTE}, {NOTE_D5, EIGHTH_NOTE},
    {NOTE_C5, QUARTER_NOTE}, {NOTE_D5, QUARTER_NOTE}, {NOTE_E5, QUARTER_NOTE},
    {NOTE_F5, HALF_NOTE}, {NOTE_G5, QUARTER_NOTE}, {NOTE_A5, QUARTER_NOTE},
    {NOTE_F5, QUARTER_NOTE}, {NOTE_E5, EIGHTH_NOTE}, {NOTE_D5, EIGHTH_NOTE},
    {NOTE_C5, HALF_NOTE}, {REST, QUARTER_NOTE}, {NOTE_C5, EIGHTH_NOTE},
    {NOTE_D5, EIGHTH_NOTE}, {NOTE_E5, EIGHTH_NOTE}, {NOTE_F5, EIGHTH_NOTE},
    {NOTE_G5, QUARTER_NOTE}, {NOTE_F5, QUARTER_NOTE}, {NOTE_E5, QUARTER_NOTE},
    {NOTE_D5, QUARTER_NOTE}, {NOTE_C5, WHOLE_NOTE}
};

// TV Time - Solo for Conductor (Part 0)
const note_t tv_time_notes[] = {
    {NOTE_E5, SIXTEENTH_NOTE}, {NOTE_D5, SIXTEENTH_NOTE}, {NOTE_C5, SIXTEENTH_NOTE}, {NOTE_B4, SIXTEENTH_NOTE},
    {NOTE_C5, EIGHTH_NOTE}, {NOTE_D5, EIGHTH_NOTE}, {NOTE_E5, EIGHTH_NOTE},
    {NOTE_G5, QUARTER_NOTE}, {NOTE_F5, EIGHTH_NOTE}, {NOTE_E5, EIGHTH_NOTE},
    {NOTE_D5, QUARTER_NOTE}, {NOTE_C5, QUARTER_NOTE}, {NOTE_E5, SIXTEENTH_NOTE},
    {NOTE_D5, SIXTEENTH_NOTE}, {NOTE_C5, SIXTEENTH_NOTE}, {NOTE_B4, SIXTEENTH_NOTE},
    {NOTE_A4, EIGHTH_NOTE}, {NOTE_B4, EIGHTH_NOTE}, {NOTE_C5, QUARTER_NOTE},
    {NOTE_D5, QUARTER_NOTE}, {NOTE_E5, HALF_NOTE}
};

// Jupiter Hymn - Quintet (simplified, all parts play same melody in harmony)
const note_t jupiter_hymn_notes[] = {
    {NOTE_D5, HALF_NOTE}, {NOTE_G5, HALF_NOTE}, {NOTE_F5, QUARTER_NOTE},
    {NOTE_E5, QUARTER_NOTE}, {NOTE_F5, HALF_NOTE}, {NOTE_D5, QUARTER_NOTE},
    {NOTE_E5, QUARTER_NOTE}, {NOTE_F5, QUARTER_NOTE}, {NOTE_G5, QUARTER_NOTE},
    {NOTE_E5, HALF_NOTE}, {NOTE_D5, HALF_NOTE}, {NOTE_C5, WHOLE_NOTE},
    {NOTE_D5, HALF_NOTE}, {NOTE_G5, HALF_NOTE}, {NOTE_F5, QUARTER_NOTE},
    {NOTE_E5, QUARTER_NOTE}, {NOTE_F5, HALF_NOTE}, {NOTE_D5, QUARTER_NOTE},
    {NOTE_E5, QUARTER_NOTE}, {NOTE_F5, QUARTER_NOTE}, {NOTE_G5, QUARTER_NOTE},
    {NOTE_A5, HALF_NOTE}, {NOTE_G5, HALF_NOTE}, {NOTE_F5, WHOLE_NOTE}
};

// Canon in D - Duet (Parts 1&0 together, Parts 2&4 together - but can't alternate dynamically)
const note_t canon_notes[] = {
    {NOTE_D5, QUARTER_NOTE}, {NOTE_A4, QUARTER_NOTE}, {NOTE_B4, QUARTER_NOTE},
    {NOTE_F4, QUARTER_NOTE}, {NOTE_G4, QUARTER_NOTE}, {NOTE_D4, QUARTER_NOTE},
    {NOTE_G4, QUARTER_NOTE}, {NOTE_A4, QUARTER_NOTE}, {NOTE_D5, QUARTER_NOTE},
    {NOTE_A4, QUARTER_NOTE}, {NOTE_B4, QUARTER_NOTE}, {NOTE_F4, QUARTER_NOTE},
    {NOTE_G4, QUARTER_NOTE}, {NOTE_D4, QUARTER_NOTE}, {NOTE_G4, QUARTER_NOTE},
    {NOTE_A4, QUARTER_NOTE}, {NOTE_B4, HALF_NOTE}, {NOTE_A4, HALF_NOTE},
    {NOTE_G4, HALF_NOTE}, {NOTE_F4, HALF_NOTE}, {NOTE_E4, WHOLE_NOTE},
    {NOTE_D4, WHOLE_NOTE}
};

// Song definitions
const song_t songs[] = {
    [SONG_JUPITER_HYMN] = {
        .name = "Jupiter Hymn",
        .type = SONG_TYPE_QUINTET,
        .notes = jupiter_hymn_notes,
        .note_count = sizeof(jupiter_hymn_notes) / sizeof(note_t),
        .parts_mask = ALL_PARTS
    },
    [SONG_CANON_IN_D] = {
        .name = "Canon in D",
        .type = SONG_TYPE_DUET,
        .notes = canon_notes,
        .note_count = sizeof(canon_notes) / sizeof(note_t),
        .parts_mask = PART_1 | PART_2 | PART_4  // Note: Should include Conductor but needs code changes
    },
    [SONG_CARNIVAL_THEME] = {
        .name = "Carnival Theme",
        .type = SONG_TYPE_SOLO,
        .notes = carnival_theme_notes,
        .note_count = sizeof(carnival_theme_notes) / sizeof(note_t),
        .parts_mask = PART_3  // Solo for Part 3 only
    },
    [SONG_CARNIVAL_VAR1] = {
        .name = "Carnival Variation",
        .type = SONG_TYPE_SOLO,
        .notes = carnival_var1_notes,
        .note_count = sizeof(carnival_var1_notes) / sizeof(note_t),
        .parts_mask = PART_2  // Solo for Part 2 only
    },
    [SONG_BLUE_BELLS] = {
        .name = "Blue Bells",
        .type = SONG_TYPE_SOLO,
        .notes = blue_bells_notes,
        .note_count = sizeof(blue_bells_notes) / sizeof(note_t),
        .parts_mask = PART_1  // Solo for Part 1 only
    },
    [SONG_MEDALLION_CALLS] = {
        .name = "Medallion Calls",
        .type = SONG_TYPE_SOLO,
        .notes = medallion_calls_notes,
        .note_count = sizeof(medallion_calls_notes) / sizeof(note_t),
        .parts_mask = PART_4  // Solo for Part 4 only
    },
    [SONG_TV_TIME] = {
        .name = "TV Time",
        .type = SONG_TYPE_SOLO,
        .notes = tv_time_notes,
        .note_count = sizeof(tv_time_notes) / sizeof(note_t),
        .parts_mask = 0  // Solo for Conductor only (needs special handling)
    }
};

const uint8_t total_songs = sizeof(songs) / sizeof(song_t);