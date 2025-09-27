# Orchestra M5GO Song Implementation Fix

## Summary of Changes

Fixed the song implementation to match the original design specification. The previous implementation had all "solo" songs playing on ALL devices simultaneously, which was incorrect. Canon in D also had an invalid reference to PART_5 which doesn't exist.

## Key Issues Fixed

### 1. Solo Songs Playing on All Devices
**Previous Issue:** All solo songs had `parts_mask = ALL_PARTS`, causing them to play on all 5 devices simultaneously.

**Fix:** Each solo song now plays on its designated device only:
- **Blue Bells of Scotland** → Part 1 only (`parts_mask = PART_1`)
- **Carnival Theme** → Part 3 only (`parts_mask = PART_3`)
- **Carnival Variation 1** → Part 2 only (`parts_mask = PART_2`)
- **Medallion Calls** → Part 4 only (`parts_mask = PART_4`)
- **TV Time** → Conductor only (special handling added)

### 2. Invalid PART_5 Reference
**Previous Issue:** Canon in D referenced `PART_5` which doesn't exist. The system only has 5 devices:
- Conductor (device_id 0, role ROLE_CONDUCTOR)
- Part 1 (device_id 1, role ROLE_PART_1)
- Part 2 (device_id 2, role ROLE_PART_2)
- Part 3 (device_id 3, role ROLE_PART_3)
- Part 4 (device_id 4, role ROLE_PART_4)

**Fix:**
- Removed `PART_5` definition from songs.h
- Updated `ALL_PARTS` from 0x1F to 0x0F (only Parts 1-4)
- Canon in D now uses Parts 1, 2, and 4

### 3. Conductor Never Playing Audio
**Previous Issue:** The Conductor was hard-coded to never play audio, even for songs that required it.

**Fix:** Added special handling in orchestra.c to allow Conductor to play audio for:
- **TV Time** (Conductor solo)
- **Canon in D** (Conductor plays with Part 1)

## Current Song Assignments

| Song | Type | Who Plays |
|------|------|-----------|
| **Jupiter Hymn** | Quintet | All devices (Conductor visual-only) |
| **Canon in D** | Duet | Conductor + Part 1 + Part 2 + Part 4 |
| **Carnival Theme** | Solo | Part 3 only |
| **Carnival Variation 1** | Solo | Part 2 only |
| **Blue Bells** | Solo | Part 1 only |
| **Medallion Calls** | Solo | Part 4 only |
| **TV Time** | Solo | Conductor only |

## Limitations

### Canon in D Alternation Not Implemented
The original design specified that Canon in D should alternate between two pairs:
1. First section: Conductor (Part 0) + Part 1
2. Second section: Part 2 + Part 4

However, the current architecture uses a static `parts_mask` that cannot change during playback. Implementing true alternation would require:
- Adding timing metadata to songs
- Supporting dynamic part switching mid-song
- Synchronizing part changes across devices

**Current Compromise:** All four parts (Conductor, 1, 2, 4) play simultaneously throughout the song.

## Files Modified

1. **src/songs.c**
   - Updated all solo song `parts_mask` values to individual parts
   - Fixed Canon in D to remove PART_5
   - Updated comments to reflect correct assignments

2. **include/songs.h**
   - Removed `PART_5` definition
   - Updated `ALL_PARTS` to 0x0F
   - Added clarifying comments

3. **src/orchestra.c**
   - Modified conductor audio logic to allow playing for specific songs
   - Added special cases for TV Time and Canon in D

## Testing Recommendations

1. **Solo Songs:** Verify each solo song plays on only one device:
   - Start Blue Bells → Only Part 1 should produce sound
   - Start Carnival Theme → Only Part 3 should produce sound
   - Start TV Time → Only Conductor should produce sound

2. **Duet:** Verify Canon in D plays on correct devices:
   - Conductor, Part 1, Part 2, and Part 4 should all play
   - Part 3 should remain silent

3. **Visual Indicators:** All devices should still show appropriate LED colors:
   - Purple for solos (even on silent devices)
   - Yellow for Canon in D duet
   - Green for Jupiter Hymn quintet

## Future Improvements

1. **Implement Dynamic Part Switching:** Add infrastructure for changing parts_mask during playback to support true alternating duets.

2. **Add Conductor Part Definition:** Consider adding `PART_CONDUCTOR = 0x10` to allow explicit conductor participation in parts_mask.

3. **Song Metadata Enhancement:** Add timing information to songs to specify when parts should start/stop playing.