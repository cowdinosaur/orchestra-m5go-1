# Orchestra M5GO Display Animations

## Overview

The Orchestra M5GO features dynamic visual animations that enhance the musical experience. The display system provides both idle-time animations to keep the project engaging and synchronized playback animations that respond to the music.

## Animation States

### 1. Idle Animation - Network Status Display

**Current Implementation:**
- **5 Rotating Circles** showing ESP-NOW network status
  - Arranged in pentagon pattern (72° apart)
  - **Slowly rotating constellation** - entire pattern rotates (one full rotation per ~10 seconds)
  - Each circle represents one device in the orchestra
  - Circle colors indicate status:
    - **Purple (solid)**: Your own device
    - **Cyan (solid)**: Connected peer device
    - **Dark gray**: Device slot offline/not detected
  - **Connection lines**: Dim cyan lines connect all online devices creating a network constellation
  - Pure black background for high contrast
  - Each circle labeled with role (C, 1, 2, 3, 4)
  - State checks every 400ms for responsive updates

**Visual Features:**
- **Dynamic rotation**: The pentagon slowly rotates, creating engaging movement
- **Network visualization**: Connection lines show the mesh network between online devices
- **Role identification**: White text labels inside circles (dimmed for offline devices)
- When powered up, your device shows as a purple circle
- As other devices connect via ESP-NOW, they appear as cyan circles with connection lines
- Full network shown when all 5 circles are connected with lines
- Dark/black background matches the equalizer for visual consistency

### 2. Playback Animation (During Music)

**Current Implementation varies by device role:**

#### **Conductor Device - Scrolling Song Title**
- **Rainbow scrolling text** displaying the current song name
- Text features:
  - 3x scaled 5x7 bitmap font (21 pixels tall)
  - Horizontally scrolling from right to left
  - Rainbow gradient colors that shift dynamically
  - Centered vertically on screen
  - Pure black background
  - Smooth scrolling at 2 pixels per frame
- Provides clear visual indication of what's playing
- Conductor acts as the "display board" for the orchestra

#### **Performer Devices (Parts 1-4) - Dynamic Equalizer**
- **Dynamic Rainbow Equalizer**
  - 12 vertical bars with organic movement
  - **Color Scheme:**
    - Pure black background for maximum contrast
    - Rainbow gradient across bars (red → orange → yellow → green → blue → purple)
    - Colors rotate continuously for fluid motion
    - Vertical gradient within each bar (darker at top, brighter at bottom)
  - **Movement Dynamics:**
    - Three overlapping sine waves create natural, fluid motion
    - Bars range from 30% to 90% of screen height
    - Baseline wave activity ensures constant movement even without beat
    - Beat intensity adds up to 30% additional height
    - Each bar moves independently with phase offsets
  - **Performance:** Updates at ~25 FPS
  - **Visual Impact:** High contrast colorful bars on black background

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
Network Status Display (5 Rotating Circles)
    - Your device: Purple circle
    - Pentagon pattern slowly rotating
    - Discovering peers via ESP-NOW
    ↓
Other Devices Connect
    - Each peer appears as cyan circle
    - Connection lines drawn between online devices
    - Creates animated network constellation
    ↓
Button Press → Song Selected
    ↓
Rainbow Equalizer Animation (all song types)
    - 12 colorful bars on black background
    - Dynamic wave movements
    - Beat-responsive heights
    ↓
Song Ends
    ↓
Return to Network Status Display
```

This rich animation system ensures the Orchestra M5GO remains visually engaging whether actively playing music or sitting idle, creating an immersive audio-visual experience.