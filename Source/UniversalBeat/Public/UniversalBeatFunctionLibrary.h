// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UniversalBeatTypes.h"
#include "UniversalBeatFunctionLibrary.generated.h"

/**
 * Blueprint function library for UniversalBeat note chart utilities
 */
UCLASS()
class UNIVERSALBEAT_API UUniversalBeatFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Convert musical note values to seconds based on BPM
	 * @param NoteValue The musical note value to convert (1/16, 1/8, 1/4, 1/2, Whole)
	 * @param BPM Beats per minute for the calculation
	 * @return Duration in seconds for the specified note value at the given BPM
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Timing", meta = (Tooltip = "Convert musical note values to seconds. Quarter note = 60/BPM seconds."))
	static float ConvertMusicalNoteToSeconds(EMusicalNoteValue NoteValue, float BPM);

	/**
	 * Get the fractional multiplier for a musical note value relative to a quarter note
	 * @param NoteValue The musical note value
	 * @return Multiplier (e.g., 1/16 = 0.25, 1/4 = 1.0, Whole = 4.0)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Timing", meta = (Tooltip = "Get fractional multiplier relative to quarter note."))
	static float GetNoteValueMultiplier(EMusicalNoteValue NoteValue);

	/**
	 * Calculate timing window in seconds for a note data asset
	 * @param PreTiming Pre-timing window as musical note value
	 * @param PostTiming Post-timing window as musical note value
	 * @param BPM Current beats per minute
	 * @param OutPreSeconds Output pre-timing window in seconds
	 * @param OutPostSeconds Output post-timing window in seconds
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Timing", meta = (Tooltip = "Calculate timing windows in seconds for note validation."))
	static void CalculateTimingWindows(EMusicalNoteValue PreTiming, EMusicalNoteValue PostTiming, float BPM, float& OutPreSeconds, float& OutPostSeconds);

	/**
	 * Get the number of ticks per broadcast for a given beat subdivision
	 * Internal subdivision is always 16 (sixteenth notes)
	 * @param Subdivision The beat subdivision level
	 * @return Number of ticks between broadcasts (0 for None, 16 for Whole, 8 for Half, 4 for Quarter, 2 for Eighth, 1 for Sixteenth)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Timing", meta = (Tooltip = "Get tick divisor for beat subdivision."))
	static int32 GetTicksForSubdivision(EBeatSubdivision Subdivision);

	/**
	 * Get the subdivision multiplier for timer interval calculation
	 * @param Subdivision The beat subdivision level
	 * @return Multiplier relative to whole beat (1 for None/Whole, 2 for Half, 4 for Quarter, 8 for Eighth, 16 for Sixteenth)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Timing", meta = (Tooltip = "Get subdivision multiplier for interval calculation."))
	static int32 GetSubdivisionMultiplier(EBeatSubdivision Subdivision);

	/**
	 * Check if a beat event falls on a specific subdivision
	 * 
	 * Important: The target subdivision must be equal to or coarser than the broadcast subdivision.
	 * - Broadcasting at Sixteenth: Can detect Whole, Half, Quarter, Eighth, Sixteenth
	 * - Broadcasting at Eighth: Can detect Whole, Half, Quarter, Eighth (NOT Sixteenth)
	 * - Broadcasting at Quarter: Can detect Whole, Half, Quarter (NOT Eighth or Sixteenth)
	 * 
	 * Examples when broadcasting at Sixteenth rate:
	 * - Whole: SubdivisionIndex == 0 (only index 0)
	 * - Half: SubdivisionIndex % 8 == 0 (indices 0, 8)
	 * - Quarter: SubdivisionIndex % 4 == 0 (indices 0, 4, 8, 12)
	 * - Eighth: SubdivisionIndex % 2 == 0 (indices 0, 2, 4, 6, 8, 10, 12, 14)
	 * - Sixteenth: Always true (every index: 0-15)
	 * 
	 * @param BeatEvent The beat event data to check (contains SubdivisionType and SubdivisionIndex)
	 * @param TargetSubdivision The subdivision to check against
	 * @return True if the beat event falls on the target subdivision, false if target is finer than broadcast rate
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Timing", meta = (Tooltip = "Check if beat event is on target subdivision. Target must be equal to or coarser than broadcast subdivision."))
	static bool IsNoteSubdivision(const FBeatEventData& BeatEvent, EBeatSubdivision TargetSubdivision);
};