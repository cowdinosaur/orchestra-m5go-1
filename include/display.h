#pragma once

#include "esp_err.h"
#include "orchestra.h"   // for song_type_t if needed

// Display module interface

// Initialize the display subsystem
esp_err_t display_init(void);

// Show idle state
void display_idle(void);

// Start animation for a specific song type
void display_start_animation(song_type_t type);

// Stop current animation
void display_stop_animation(void);
