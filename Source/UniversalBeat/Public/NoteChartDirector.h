// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceDirector.h"
#include "UniversalBeatTypes.h"
#include "GameplayTagContainer.h"
#include "NoteChartDirector.generated.h"

class UMovieSceneNoteChartSection;
class UNoteDataAsset;

/**
 * Sequence director for note chart playback tracking
 * Manages note progression, timing windows, and validation coordination
 */
UCLASS(Blueprintable)
class UNIVERSALBEAT_API UNoteChartDirector : public ULevelSequenceDirector
{
	GENERATED_BODY()

public:
	UNoteChartDirector();

	/**
	 * Initialize the director - call this after the sequence begins playing to load notes
	 * This should be called from Blueprint (e.g., BeginPlay or OnPlay event)
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Director")
	void InitializeNoteChart();

	/**
	 * Get the next note with the matching tag within its timing window
	 * @param NoteTag Gameplay tag to search for
	 * @param CurrentTime Current playback time in seconds
	 * @param OutNote Output parameter - the matching note instance if found
	 * @return True if a matching note was found, false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Director")
	bool GetNextNoteForTag(FGameplayTag NoteTag, float CurrentTime, FNoteInstance& OutNote);

	/**
	 * Mark a note as consumed (successfully validated)
	 * Prevents re-validation of the same note
	 * @param Note The note instance to mark as consumed
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Director")
	void MarkNoteConsumed(const FNoteInstance& Note);

	/**
	 * Check if a note has been consumed
	 * @param Note The note instance to check
	 * @return True if note was already consumed
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Director")
	bool IsNoteConsumed(const FNoteInstance& Note) const;

	/**
	 * Reset consumed notes (for looping sequences)
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Director")
	void ResetConsumedNotes();

	/**
	 * Get all notes from all note chart tracks in the sequence
	 * @return Sorted array of all note instances
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Director")
	TArray<FNoteInstance> GetAllNotes() const;

	/**
	 * Get the total number of notes in the sequence
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Director")
	int32 GetTotalNoteCount() const;

	/**
	 * Get the current BPM for timing calculations
	 * Retrieved from UniversalBeatSubsystem
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Director")
	float GetCurrentBPM() const;

	// Blueprint implementable events for game logic

	/**
	 * Called when a note is successfully hit
	 * @param Result Validation result with accuracy and timing info
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UniversalBeat|Director Events", meta = (DisplayName = "On Note Hit"))
	void OnNoteHit(const FNoteValidationResult& Result);

	/**
	 * Called when a note is missed (timing window expired)
	 * @param Note The note instance that was missed
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UniversalBeat|Director Events", meta = (DisplayName = "On Note Missed"))
	void OnNoteMissed(const FNoteInstance& Note);

protected:
	/** Cached sorted notes from all note chart sections */
	UPROPERTY()
	TArray<FNoteInstance> CachedNotesSorted;

	/** Set of consumed note timestamps (for fast lookup) */
	UPROPERTY()
	TSet<int32> ConsumedNoteTimestamps;

	/** Current note index for sequential playback tracking */
	UPROPERTY()
	int32 CurrentNoteIndex;

	/** Last update time for miss detection */
	UPROPERTY()
	float LastUpdateTime;

	/**
	 * Initialize note tracking from sequence
	 * Called in OnCreated()
	 */
	void InitializeNoteTracking();

	/**
	 * Load notes from all note chart sections in the sequence
	 */
	void LoadNotesFromSequence();

	/**
	 * Update miss detection (checks if notes have expired)
	 * Should be called regularly during playback
	 */
	UFUNCTION()
	void UpdateMissDetection(float CurrentTime);

	/**
	 * Convert frame number to seconds using sequence frame rate
	 */
	float FrameToSeconds(FFrameNumber Frame) const;

	/**
	 * Convert seconds to frame number using sequence frame rate
	 */
	FFrameNumber SecondsToFrame(float Seconds) const;
};