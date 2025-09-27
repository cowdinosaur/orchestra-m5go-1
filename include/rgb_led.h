#ifndef RGB_LED_H
#define RGB_LED_H

#include <stdint.h>
#include "esp_err.h"

// RGB LED module interface - controls the SK6812 RGB LED strip

// Initialize the RGB LED subsystem
// Returns: ESP_OK on success, error code on failure
void rgb_init(void);

// Set all LEDs to the same color
// Parameters:
//   color: 24-bit RGB color value (0xRRGGBB)
void rgb_set_all_color(uint32_t color);

// Set a single LED to a specific color
// Parameters:
//   index: LED index (0 to RGB_LED_COUNT-1)
//   color: 24-bit RGB color value
// Returns: ESP_OK on success, ESP_ERR_INVALID_ARG if index is invalid
esp_err_t rgb_set_pixel(uint8_t index, uint32_t color);

// Create a breathing effect with the specified color
// Parameters:
//   color: 24-bit RGB color value
//   duration_ms: Duration of one breath cycle in milliseconds
// Returns: ESP_OK on success
esp_err_t rgb_breathing_effect(uint32_t color, uint32_t duration_ms);

// Stop any ongoing LED effects
void rgb_stop_effects(void);

// Update the LED display (must be called after setting colors)
// Returns: ESP_OK on success, error code on failure
esp_err_t rgb_update(void);

#endif // RGB_LED_H