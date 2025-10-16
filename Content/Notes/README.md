# Note Data Assets

This folder contains **Note Data Assets** that define the properties of note types used in rhythm gameplay.

## Creating a New Note Data Asset

1. **Right-click** in the Content Browser
2. Select **Miscellaneous** → **Data Asset**
3. Choose **NoteDataAsset** as the class
4. Name your asset (e.g., `DA_LeftArrow`, `DA_RightArrow`, `DA_Jump`)

## Configuring Properties

### Identity
- **Label**: Human-readable name (e.g., "Left Arrow", "Jump Button")
- **Note Tag**: Gameplay tag identifier (e.g., `Input.Left`, `Input.Jump`)
  - Must start with "Input." for consistency
  - Used to match player input with notes during gameplay

### Timing Windows
- **Pre Timing**: How early input is accepted (before the note timestamp)
  - **Sixteenth** (1/16): Very strict (0.125 beats at 120 BPM = ~62ms)
  - **Eighth** (1/8): Strict (0.25 beats = ~125ms) ← **Default**
  - **Quarter** (1/4): Moderate (0.5 beats = ~250ms)
  - **Half** (1/2): Lenient (1 beat = ~500ms)
  - **Whole**: Very lenient (2 beats = ~1000ms)

- **Post Timing**: How late input is accepted (after the note timestamp)
  - Same values as Pre Timing
  - **Quarter** (1/4) is the **default** for more forgiveness on late inputs

### Visual
- **Icon Texture**: Icon displayed in UI and sequencer
  - Optional but recommended for visual clarity

### Interaction
- **Interaction Type**: 
  - **Press**: Single button press (default, fully supported)
  - **Hold**: Hold button down (stored for future use)
  - **Release**: Release button (stored for future use)

## Example Configurations

### Easy Note (Forgiving timing)
- **Pre Timing**: Quarter (1/4)
- **Post Timing**: Half (1/2)
- **Use Case**: Tutorial levels, accessibility

### Standard Note (Balanced)
- **Pre Timing**: Eighth (1/8)
- **Post Timing**: Quarter (1/4)
- **Use Case**: Normal difficulty

### Hard Note (Strict timing)
- **Pre Timing**: Sixteenth (1/16)
- **Post Timing**: Eighth (1/8)
- **Use Case**: Expert difficulty, challenge modes

### Final Note (Extended grace period)
- **Pre Timing**: Quarter (1/4)
- **Post Timing**: Whole (1)
- **Use Case**: Song endings, important story beats

## Tips

1. **Timing Windows Scale with BPM**: 
   - Faster songs (higher BPM) = shorter timing windows in seconds
   - Slower songs (lower BPM) = longer timing windows in seconds
   - The musical fractions ensure consistency across tempos

2. **Visual Feedback**:
   - Assign unique icons to different note types
   - Use consistent colors/styles for similar difficulty levels

3. **Validation**:
   - Editor will warn if note tag is invalid or missing
   - Editor will warn for very strict timing (1/16 + 1/16)
   - All validation happens automatically in the Details panel

4. **Testing**:
   - Create data assets first
   - Test in-game with different BPM values
   - Adjust timing windows based on playtest feedback

## Note Types for Different Genres

### Rhythm Action (DDR-style)
- DA_LeftArrow, DA_RightArrow, DA_UpArrow, DA_DownArrow
- Standard timing for all

### Rhythm Platformer
- DA_Jump, DA_Dash, DA_Attack
- Jump: Eighth/Quarter (precise)
- Dash: Quarter/Quarter (moderate)
- Attack: Quarter/Half (lenient)

### Music Trainer
- DA_EasyBeat, DA_NormalBeat, DA_HardBeat
- Different timing strictness for difficulty progression

## See Also
- `Source/UniversalBeat/Public/NoteDataAsset.h` - Full API documentation
- `specs/002-implement-a-note/quickstart.md` - Complete workflow guide
- `specs/002-implement-a-note/data-model.md` - Technical details
