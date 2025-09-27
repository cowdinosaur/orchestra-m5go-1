# Orchestra M5GO Display Animations

## Overview

The Orchestra M5GO features dynamic visual animations that enhance the musical experience. The display system provides both idle-time animations to keep the project engaging and synchronized playback animations that respond to the music.

## Animation States

### 1. Idle Animation - Network Status Display

**Current Implementation:**
- **5 Pulsing Circles** showing ESP-NOW network status
  - Arranged in pentagon pattern
  - Each circle represents one device in the orchestra
  - Circle colors indicate status:
    - **Green (pulsing)**: Your own device
    - **Cyan (pulsing)**: Connected peer device
    - **Dark red (static)**: Device slot offline/not detected
  - Smooth sine-wave pulsing animation at ~25 FPS
  - Dark blue background (RGB: 0, 0, 50)

**Visual Feedback:**
- When powered up, your device shows as a green pulsing circle
- As other devices connect via ESP-NOW, they appear as cyan pulsing circles
- Full network shown when all 5 circles are pulsing (1 green + 4 cyan)
- Instantly see which devices are online/offline in the orchestra

### 2. Playback Animation (During Music)

**Current Implementation:**
- **Equalizer Bars Only**
  - 12 vertical bars with varying heights
  - Heights respond to beat intensity (0.0 to 1.0)
  - Role-based coloring:
    - Conductor: Base blue
    - Part 1-4: Unique colors per role
  - Bar colors tinted based on intensity
  - Updates at ~25 FPS

**Planned but Not Yet Implemented:**

#### **Solo Songs** (Purple LED)
- Spiral Animation - Rainbow spiral from center
- Particle System - Gravity-affected particles
- Fireworks - Burst effects

#### **Duet Songs** (Yellow LED)
- Synchronized Circles - Two orbiting circles

#### **Quintet Songs** (Green LED)
- Five Synchronized Circles - Pentagonal formation
- Particle Burst - Intense particle effects

## Visual Effects

### Color Coordination
- Animation colors match LED indicators:
  - Blue: Idle/Ready state
  - Purple: Solo performance
  - Yellow: Duet performance
  - Green: Quintet performance

### Particle Physics
- Gravity simulation for realistic motion
- Velocity-based trailing effects
- Life-based fading for smooth transitions
- Collision boundaries at screen edges

### Synchronization Features
- Frame-based animation timing
- Beat intensity parameter for music-reactive effects
- Device role awareness for customized animations
- Smooth transitions between animation states

## Technical Details

### Performance
- 30 FPS target framerate
- Double-buffered rendering (when fully implemented)
- DMA-optimized memory allocation
- Efficient pixel drawing algorithms

### Display Specifications
- Resolution: 320x240 pixels
- Color: 16-bit RGB565
- Controller: ILI9342C
- Interface: SPI

### Animation Techniques

#### HSV Color Space
- Used for smooth color transitions
- Enables rainbow effects
- Natural hue cycling

#### Mathematical Functions
- Sine waves for smooth oscillations
- Circular motion using trigonometry
- Pseudo-random generation for variety
- Physics simulation for particles

## Customization

### Per-Device Animations
Each device can have slightly different animation parameters based on its role:
- Conductor: Lead animation patterns
- Part 1-4: Offset phases for variety
- Creates cohesive but varied visual experience

### Future Enhancements

Potential additions to the animation system:

1. **Audio-Reactive Animations**
   - Real-time FFT analysis
   - Frequency-based visualizations
   - Beat detection for synchronized effects

2. **Network-Synchronized Effects**
   - Animations that span across all 5 screens
   - Coordinated color waves
   - Message-passing visual effects

3. **User Customization**
   - Selectable animation themes
   - Adjustable colors and speeds
   - Custom image uploads

4. **Advanced Effects**
   - 3D rendering capabilities
   - Shader-like effects
   - Video playback support

## Usage Tips

### Best Viewing Conditions
- Dim lighting enhances LED effects
- Arrange M5GOs in arc for best visual impact
- Keep screens at eye level

### Battery Optimization
- Animations automatically dim after extended idle
- Lower brightness options available
- Sleep mode after inactivity

### Troubleshooting

**No animations showing:**
- Check display initialization in serial output
- Verify SPI connections
- Ensure framebuffer allocation succeeded

**Choppy animations:**
- Reduce particle count
- Optimize animation complexity
- Check CPU usage

**Wrong colors:**
- Verify RGB565 conversion
- Check color byte order
- Ensure proper bit shifting

## Animation Flow

```
Power On
    ↓
Network Status Display (5 Circles)
    - Your device: Green pulsing circle
    - Discovering peers via ESP-NOW
    ↓
Other Devices Connect
    - Each peer appears as cyan pulsing circle
    - Real-time network visualization
    ↓
Button Press → Song Selected
    ↓
Equalizer Animation (all song types)
    - Role-based colors
    - Beat-responsive bars
    ↓
Song Ends
    ↓
Return to Network Status Display
```

This rich animation system ensures the Orchestra M5GO remains visually engaging whether actively playing music or sitting idle, creating an immersive audio-visual experience.