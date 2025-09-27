#ifndef CONFIG_H
#define CONFIG_H

// Hardware Configuration
// ======================

// M5Stack Core Button GPIO Pins (ADC input-only pins with external pull-ups)
#define BTN_A_GPIO          39
#define BTN_B_GPIO          38
#define BTN_C_GPIO          37

// Audio Configuration
#define AUDIO_SAMPLE_RATE   44100
#define AUDIO_DMA_BUF_COUNT 8
#define AUDIO_DMA_BUF_LEN   64
#define AUDIO_TICK_MS       10
#define AUDIO_DEFAULT_VOLUME 0.03f  // Reduced to 3% to prevent distortion
#define AUDIO_SPEAKER_GPIO  25

// Display Configuration
#define DISPLAY_WIDTH       320
#define DISPLAY_HEIGHT      240
#define DISPLAY_SPI_FREQ    40000000  // 40MHz

// RGB LED Configuration
#define RGB_LED_GPIO        15
#define RGB_LED_COUNT       10
#define RGB_RMT_CHANNEL     RMT_CHANNEL_0

// System Timing
// =============

// Button debounce time in milliseconds
#define BUTTON_DEBOUNCE_MS  200

// Task delays and timeouts
#define IDLE_ANIMATION_DELAY_MS     80
#define HEARTBEAT_INTERVAL_MS       1000
#define DISCOVERY_TIMEOUT_MS        5000
#define DISCOVERY_RETRY_INTERVAL_MS 2000

// Task Configuration
// ==================

// Task stack sizes (in words, not bytes)
#define BUTTON_TASK_STACK_SIZE      2048
#define PLAYBACK_TASK_STACK_SIZE    4096
#define ESPNOW_TASK_STACK_SIZE      3072
#define HEARTBEAT_TASK_STACK_SIZE   2048
#define DISCOVERY_TASK_STACK_SIZE   2048
#define ANIMATION_TASK_STACK_SIZE   3072

// Task priorities (higher number = higher priority)
#define BUTTON_TASK_PRIORITY        10
#define PLAYBACK_TASK_PRIORITY      5
#define ESPNOW_TASK_PRIORITY        7
#define HEARTBEAT_TASK_PRIORITY     3
#define DISCOVERY_TASK_PRIORITY     6
#define ANIMATION_TASK_PRIORITY     4

// Queue Configuration
// ===================

#define ESPNOW_QUEUE_SIZE           20
#define DISCOVERY_QUEUE_SIZE        10

// Network Configuration
// =====================

// ESP-NOW settings
#define ESPNOW_WIFI_CHANNEL         1
#define ESPNOW_MAX_PEERS            10
#define ESPNOW_SEND_RETRY_COUNT     3
#define ESPNOW_SEND_RETRY_DELAY_MS  10

// Clock synchronization
#define CLOCK_SYNC_INTERVAL_MS      5000
#define CLOCK_SYNC_MIN_SAMPLES      3
#define CLOCK_SYNC_MAX_OFFSET_US    1000000  // 1 second max clock difference

// Device Configuration
// ====================

// Maximum number of devices in the orchestra
#define MAX_ORCHESTRA_DEVICES       5

// Role assignment methods
#define CONFIG_METHOD_AUTO_ASSIGN   0
#define CONFIG_METHOD_MAC_TABLE     1
#define CONFIG_METHOD_GPIO          2
#define CONFIG_METHOD_NVS           3
#define CONFIG_METHOD_HARDCODED     4

// NVS namespace for storing device configuration
#define NVS_NAMESPACE               "orchestra"
#define NVS_KEY_DEVICE_ROLE         "role"
#define NVS_KEY_DEVICE_ID           "id"

// Logging Configuration
// =====================

#ifndef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#endif

// Color Definitions (24-bit RGB)
// ===============================

#define COLOR_IDLE          0x3333FF   // Blue - waiting state
#define COLOR_QUINTET       0x33FF33   // Green - all parts playing
#define COLOR_DUET          0xFFFF33   // Yellow - two parts playing
#define COLOR_SOLO          0xCC33CC   // Purple - single part
#define COLOR_ERROR         0xFF3333   // Red - error state
#define COLOR_DISCOVERY     0x33FFFF   // Cyan - discovery mode

// Animation Parameters
// ====================

#define ANIMATION_FPS               30
#define ANIMATION_SMOOTHING_FACTOR  0.2f
#define BEAT_PULSE_DURATION_MS      100
#define BEAT_PULSE_INTENSITY        0.8f

// Audio Processing
// ================

// Frequency limits for audio synthesis
#define AUDIO_MIN_FREQUENCY         50    // Hz
#define AUDIO_MAX_FREQUENCY         8000  // Hz

// Volume limits
#define AUDIO_MIN_VOLUME            0.0f
#define AUDIO_MAX_VOLUME            1.0f

// Note transform ratios for harmony generation
#define HARMONY_OCTAVE_DOWN_RATIO   0.5f
#define HARMONY_OCTAVE_UP_RATIO     2.0f
#define HARMONY_FIFTH_RATIO         1.5f
#define HARMONY_THIRD_RATIO         1.25f

// Safety Limits
// =============

#define MAX_SONG_NAME_LENGTH        32
#define MAX_NOTES_PER_SONG          1000
#define MAX_NOTE_DURATION_MS        10000
#define MIN_NOTE_DURATION_MS        10

// Debug Features
// ==============

// Uncomment to enable various debug features
// #define DEBUG_PRINT_MAC_ADDRESS
// #define DEBUG_PRINT_ESPNOW_MESSAGES
// #define DEBUG_PRINT_BUTTON_PRESSES
// #define DEBUG_PRINT_TIMING_INFO
// #define DEBUG_SIMULATE_PACKET_LOSS

#ifdef DEBUG_SIMULATE_PACKET_LOSS
#define PACKET_LOSS_PERCENTAGE     10  // Simulate 10% packet loss
#endif

#endif // CONFIG_H