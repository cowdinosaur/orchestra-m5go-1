#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Audio module interface - provides audio playback functionality

// Initialize the audio subsystem
// Returns: ESP_OK on success, error code on failure
esp_err_t audio_init(void);

// Play a song by its ID
// Parameters:
//   song_id: Index of the song to play (0 to total_songs-1)
// Returns: ESP_OK on success, ESP_ERR_INVALID_ARG if song_id is invalid
esp_err_t audio_play_song(uint8_t song_id);

// Play a song with a specific role (for multi-part songs)
// Parameters:
//   song_id: Index of the song to play
//   role: Device role (ROLE_CONDUCTOR, ROLE_PART_1, etc.)
// Returns: ESP_OK on success, error code on failure
esp_err_t audio_play_song_for_role(uint8_t song_id, uint8_t role);

// Stop current audio playback
void audio_stop(void);

// Set the audio volume
// Parameters:
//   volume: Volume level (0.0 to 1.0, will be clamped to range)
// Returns: ESP_OK on success
esp_err_t audio_set_volume(float volume);

// Check if audio is currently playing
// Returns: true if playing, false otherwise
bool audio_is_playing(void);

#endif // AUDIO_H