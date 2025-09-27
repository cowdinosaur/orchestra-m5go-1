# Orchestra M5GO Button Controls

## Overview

The Orchestra M5GO system has **two different button control implementations** that conflict with each other:
1. **Simple sequential control** in `main.c` (currently active)
2. **Song-specific control** in `orchestra.c` (currently inactive/overridden)

## Current Active Implementation (from main.c)

### CONDUCTOR Device Only
The conductor device is the ONLY device that responds to button presses. Performers (Parts 1-4) completely ignore button inputs.

#### Button A (Left) - STOP
- **Function**: Stops any currently playing song
- **When Playing**: Broadcasts MSG_SYNC_STOP to all devices, stops playback
- **When Idle**: Does nothing (logs "Stop pressed but nothing playing")

#### Button B (Middle) - START/STOP Toggle
- **When Idle**: Starts playing the currently selected song index
  - Broadcasts MSG_SYNC_START with song_index
  - Cycles through ALL songs sequentially (0-6)
- **When Playing**: Stops playback
  - Broadcasts MSG_SYNC_STOP to all devices

#### Button C (Right) - NEXT SONG
- **Function**: Advances to the next song in the list
- **Behavior**: Cycles through songs 0→1→2→3→4→5→6→0 (wraps around)
- **When Playing**: Automatically restarts with the new song
  - Sends STOP, waits 30ms, sends START with new song
- **When Idle**: Just updates the song selection for next start

### Song List (Sequential Order):
0. **Jupiter Hymn** - Quintet piece from Holst's The Planets
1. **Canon in D** - Duet by Pachelbel
2. **Carnival of Venice Theme** - Solo piece
3. **Carnival of Venice Variation 1** - Solo piece
4. **Blue Bells of Scotland** - Traditional solo
5. **The Medallion Calls** - Solo from Pirates of the Caribbean
6. **It's TV Time!** - Solo from Deltarune

### PERFORMER Devices (Parts 1-4)
- **Button A**: No function
- **Button B**: **Speaker Test Mode** (for debugging)
  - Press to play the device's designated solo song
  - Press again to stop playback
  - Part 1 plays: Blue Bells of Scotland
  - Part 2 plays: Carnival Variation 1
  - Part 3 plays: Carnival Theme
  - Part 4 plays: Medallion Calls
- **Button C**: No function
- Performers primarily respond to ESP-NOW messages from the conductor

## Inactive Implementation (from orchestra.c)

**NOTE**: This implementation is currently OVERRIDDEN by main.c but the code still exists.

If this were active, the conductor buttons would work differently:

### Button A - Cycles between:
1. Jupiter Hymn (Quintet)
2. Carnival Theme (Solo)

### Button B - Cycles between:
1. Canon in D (Duet)
2. Carnival Variation 1 (Solo)
3. Medallion Calls (Solo)

### Button C - Cycles between:
1. Blue Bells (Solo)
2. TV Time (Solo)

## Important Notes

### Control Flow Issues
1. **Conflicting Implementations**: There are TWO separate button handling systems:
   - `main.c` uses a simple while loop with sequential song selection
   - `orchestra.c` has button handlers that map specific songs to each button

2. **main.c Takes Precedence**: The main.c implementation runs in an infinite loop on the conductor, preventing orchestra.c handlers from ever being called

3. **orchestra_handle_button_*() Functions**: These exist in orchestra.c but are NEVER called in the current implementation

### How to Switch Implementations

To use the orchestra.c button mapping instead of sequential:
1. Remove the button handling while loop from main.c (lines 94-144)
2. The interrupt-based handlers in orchestra.c will then take over
3. Each button will cycle through its specific songs instead of sequential selection

### Current Behavior Summary

| Device | Button A | Button B | Button C |
|--------|----------|----------|----------|
| **Conductor** | Stop playback | Start/Stop toggle | Next song (0-6) |
| **Part 1** | No function | Test: Blue Bells | No function |
| **Part 2** | No function | Test: Carnival Var1 | No function |
| **Part 3** | No function | Test: Carnival Theme | No function |
| **Part 4** | No function | Test: Medallion Calls | No function |

### Design Philosophy
- **Centralized Control**: Only the conductor can control playback
- **Synchronized Performance**: All performers follow conductor's commands via ESP-NOW
- **No Local Control**: Performers cannot independently start/stop songs