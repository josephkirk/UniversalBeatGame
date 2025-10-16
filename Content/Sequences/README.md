# Note Chart Sequences

This folder contains **Level Sequences** with **Note Chart Tracks** for rhythm gameplay.

## Creating a Note Chart Sequence

### 1. Create a Level Sequence
1. Right-click in Content Browser
2. Select **Cinematics** → **Level Sequence**
3. Name it (e.g., `SEQ_TestChart`, `SEQ_Level1`)

### 2. Add a Note Chart Track (Currently Manual - Editor Integration Pending)

**Note**: Full sequencer editor integration (drag-drop, visual editing) is planned for future updates. Currently, note charts are created programmatically or via data.

### 3. Configure the Sequence
- Set the sequence length to match your song duration
- Configure frame rate (typically 24fps or 30fps)
- Set playback range

## Working with Note Charts Programmatically

Until sequencer editor integration is complete, you can:

### Option A: Use Blueprint/C++ to Add Notes
```cpp
// Get or create note chart section
UMovieSceneNoteChartSection* Section = ...;

// Add notes programmatically
Section->AddNote(FFrameNumber(24), DA_LeftArrow);  // Note at 1 second (24fps)
Section->AddNote(FFrameNumber(48), DA_RightArrow); // Note at 2 seconds
Section->AddNote(FFrameNumber(72), DA_Jump);       // Note at 3 seconds
```

### Option B: Create Test Sequences
Create simple test sequences with a few notes to validate:
- Note data asset references work
- Timestamps are correct
- Snap grid resolution is configured
- Sequence playback triggers notes

## Snap Grid Configuration

The `SnapGridResolution` property controls how notes snap in the editor:

- **Sixteenth (1/16)**: Very precise, 4 snaps per quarter note
- **Eighth (1/8)**: Standard, 2 snaps per quarter note  
- **Quarter (1/4)**: One snap per beat
- **Half (1/2)**: Two beats per snap
- **Whole**: Four beats per snap

**Tip**: Match snap grid to your song's fastest note density.

## Sequence Properties

### Frame Rate
- **24 FPS**: Common for games, smoother at lower frame counts
- **30 FPS**: Higher precision, more frames per second
- **60 FPS**: Maximum precision, best for fast-paced rhythm games

### Duration
- Calculate based on song length
- Add a few seconds buffer at the end
- Example: 3-minute song = 180 seconds = 4320 frames at 24fps

## Integration with Song Configuration

Once you have note chart sequences, reference them in **Song Configuration** data assets:

1. Create a `USongConfiguration` in `Content/Songs/`
2. Add track entries with your sequences
3. Configure delays and looping as needed
4. Register the song in your game logic

See `Content/Songs/README.md` for details.

## Example Sequences

### SEQ_TestChart
- **Purpose**: Basic functionality testing
- **Duration**: 10 seconds
- **Notes**: 10-20 notes with mixed types
- **Use**: Verify note timing, input validation, visual feedback

### SEQ_Tutorial
- **Purpose**: Tutorial level
- **Duration**: 60 seconds
- **Notes**: Simple patterns, forgiving timing
- **Use**: Teach players the mechanics

### SEQ_Level1
- **Purpose**: First real challenge
- **Duration**: 120-180 seconds
- **Notes**: Progressive difficulty
- **Use**: Core gameplay experience

## Director Binding

To enable note tracking and validation, sequences need a **Note Chart Director**:

1. Open your sequence
2. In the Sequence Editor, find **Director Class** property
3. Set to `UNoteChartDirector`
4. The director will automatically track notes during playback

See `specs/002-implement-a-note/quickstart.md` for the complete workflow.

## Future Enhancements

Planned editor integration features:
- [ ] Drag-drop note placement in timeline
- [ ] Visual note icons in sequencer
- [ ] Snap-to-grid visualization
- [ ] Note selection and editing
- [ ] Copy/paste note patterns
- [ ] Quantization tools
- [ ] Note preview playback

## Technical Details

- **Section Class**: `UMovieSceneNoteChartSection`
- **Track Class**: `UMovieSceneNoteChartTrack`
- **Director Class**: `UNoteChartDirector` (implemented in Phase 5)
- **Frame Number**: Unreal's FFrameNumber for precise timing
- **Sorting**: Notes automatically sorted by timestamp

## See Also
- `Source/UniversalBeat/Public/MovieSceneNoteChartSection.h` - Section API
- `Source/UniversalBeat/Public/MovieSceneNoteChartTrack.h` - Track API
- `specs/002-implement-a-note/data-model.md` - Data structures
- `specs/002-implement-a-note/quickstart.md` - Complete workflow