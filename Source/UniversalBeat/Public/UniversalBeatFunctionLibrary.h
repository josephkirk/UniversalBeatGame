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
};