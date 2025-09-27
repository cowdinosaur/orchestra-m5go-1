# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Orchestra M5GO is an ESP-IDF/PlatformIO project that creates a synchronized multi-device music player using M5Stack Core (M5GO) devices. The system uses ESP-NOW for wireless synchronization to play multi-part orchestral pieces across 5 devices.

## Build and Development Commands

### Build the project
```bash
pio run
```

### Flash to a specific device role
```bash
# Flash as conductor (device 0)
pio run -e conductor -t upload

# Flash as part 1-4 performers
pio run -e part1 -t upload
pio run -e part2 -t upload
pio run -e part3 -t upload
pio run -e part4 -t upload
```

### Monitor serial output
```bash
pio device monitor
```

### Clean build files
```bash
pio run -t clean
```

### Erase flash (useful for clearing NVS storage)
```bash
pio run -t erase_flash
```

## Architecture

### Device Roles System
The project implements a distributed orchestra where each M5GO device has a specific role:
- **ROLE_CONDUCTOR** (Device 0): Controls song selection and broadcasts start/stop commands
- **ROLE_PART_1 to ROLE_PART_4** (Devices 1-4): Performers that respond to conductor commands

Role assignment methods (configured in `device_config.c`):
- MAC address table lookup
- GPIO pin configuration
- Auto-assignment via discovery
- Compile-time definition via `-DDEVICE_ROLE`

### Communication Architecture
- **ESP-NOW Protocol**: Broadcast-based messaging for real-time synchronization
- **Discovery System** (`espnow_discovery.c`): Automatic peer discovery and registration
- **Message Types**: SYNC_START, SYNC_STOP, SONG_SELECT, HEARTBEAT
- All devices maintain timestamps for synchronized playback

### Audio System
- Uses ESP32 internal DAC via I2S interface
- Audio output on GPIO 25 (M5Stack speaker)
- Note-based synthesis with frequency and duration
- Per-device part melodies for multi-part songs

### Display and Visual Feedback
- ILI9342C LCD controller via SPI
- Animations synchronized with music playback
- RGB LED (SK6812) on GPIO 15 for song type indication:
  - Blue: Idle
  - Green: Quintet
  - Yellow: Duet
  - Purple: Solo

### Song Organization
Songs support three types:
- **Solo**: All devices play the same melody
- **Duet**: Alternating parts between device pairs
- **Quintet**: Each device plays a unique part

Part assignment uses bit masks (e.g., `PART_1 | PART_4` for devices 1 and 4).

## Key Implementation Details

### Button Handling
- Conductor device handles physical button presses (GPIO 37, 38, 39)
- Button actions broadcast commands to all performers
- Debouncing implemented with state tracking

### Synchronization
- Timestamp-based synchronization using `esp_timer_get_time()`
- Messages include sender timestamp for latency compensation
- Heartbeat messages maintain connection status

### State Management
- Idle state with discovery broadcasts
- Playing state with synchronized audio/visual
- Automatic recovery from disconnections

## Testing Approach

1. Flash all 5 devices with appropriate roles
2. Power on devices and wait for blue idle LED
3. Press buttons on conductor to test:
   - Button A: Stop/Cycle songs
   - Button B: Start/Stop current song
   - Button C: Next song
4. Verify synchronization by observing LED colors and audio playback
5. Check serial monitor for ESP-NOW communication logs

## Common Development Tasks

### Adding a New Song
1. Define note arrays in `src/songs.c`
2. Add song structure with type, parts mask, and melodies
3. Update `total_songs` count
4. Test multi-device synchronization

### Modifying Device Roles
1. Update MAC table in `src/device_config.c` for permanent assignment
2. Or use GPIO pins for hardware-based configuration
3. Or set compile-time role in `platformio.ini` environments

### Debugging Communication
- Enable ESP_LOG level in `sdkconfig`
- Monitor ESP-NOW peer registration in serial output
- Check timestamp differences for sync issues