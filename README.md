# UniversalBeat Rhythm Game System Plugin

**Version**: 1.0.0  
**Engine**: Unreal Engine 5.6+  
**Platform**: Cross-platform compatible (Windows primary)

## Overview

UniversalBeat is a genre-agnostic rhythm game system plugin that provides precise beat tracking and timing accuracy evaluation for player inputs. Designers can configure BPM, check input timing via Blueprint, and bind to event dispatchers for rhythm-based feedback systems.

## Features

- ✅ **BPM-based Beat Tracking** - Set tempo, system tracks beats automatically
- ✅ **Timing Accuracy Checks** - Evaluate input timing (0.0-1.0 scale)
- ✅ **Event-Driven Integration** - React to timing checks and beat events
- ✅ **Player Calibration** - Compensate for hardware latency
- ✅ **Beat Broadcasting** - Sync effects to rhythm with subdivisions
- ✅ **Time Dilation Support** - Choose real-time or dilated-time tracking
- ✅ **Frame-Independent** - Sub-millisecond precision regardless of frame rate
- ✅ **Blueprint-Friendly** - All features accessible without C++

## Installation

1. Copy the `UniversalBeat` folder to your project's `Plugins/` directory
2. Open your project in Unreal Editor
3. Go to **Edit → Plugins**
4. Find "UniversalBeat Rhythm System" in the list
5. Enable the plugin
6. Restart the editor when prompted

## Quick Start (5 Minutes)

### Step 1: Set BPM

```
Event Begin Play → Get Game Instance → Get Subsystem (UniversalBeat) → Set BPM (120)
```

### Step 2: Check Input Timing

```
Input Action → Get Subsystem → Check Beat Timing By Label ("Jump") → Returns 0.0-1.0
```

### Step 3: React to Timing
Bind to OnBeatCheck event and get the value of input timing value
```
Branch: Value >= 0.9 → "Perfect!"
Branch: Value >= 0.7 → "Great!"
Branch: Value >= 0.5 → "Good!"
Else → "Miss!"
```

## Example Assets

Check `Content/Blueprints/Examples/` for:
- `BP_RhythmGameExample` - Complete rhythm mechanic implementation
- `WBP_TimingFeedback` - UI widget for timing display
- `WBP_CalibrationUI` - Player calibration interface

## Target Performance

- **Timing Check**: <0.1ms per check
- **Beat Broadcasting**: <0.1ms per frame
- **Target Frame Rate**: 60+ FPS (stable operation at 30+ FPS)
- **Memory Footprint**: ~200 bytes (negligible)

## License

MIT License

Copyright (c) 2025 AFW Project Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---

**Created with UniversalBeat** 🎵 | Powered by Unreal Engine 5.6
