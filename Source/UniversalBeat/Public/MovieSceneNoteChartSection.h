// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneSection.h"
#include "UniversalBeatTypes.h"
#include "MovieSceneNoteChartSection.generated.h"

/**
 * Movie scene section that contains note chart data
 * Stores note instances with timestamps and snap grid configuration
 */
UCLASS()
class UNIVERSALBEAT_API UMovieSceneNoteChartSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneNoteChartSection();

	/** Array of note instances in this section */
	UPROPERTY()
	TArray<FNoteInstance> Notes;

	/** Snap grid resolution for note placement in editor */
	UPROPERTY(EditAnywhere, Category = "Note Chart")
	EMusicalNoteValue SnapGridResolution;

	/**
	 * Add a note to the section at the specified timestamp
	 * Automatically sorts notes after addition
	 */
	void AddNote(FFrameNumber Timestamp, UNoteDataAsset* NoteData);

	/**
	 * Remove a note at the specified index
	 */
	void RemoveNote(int32 Index);

	/**
	 * Remove all notes in the section
	 */
	void ClearNotes();

	/**
	 * Sort notes by timestamp (ascending order)
	 */
	void SortNotes();

	/**
	 * Get all notes within a specific frame range
	 */
	TArray<FNoteInstance> GetNotesInRange(FFrameNumber StartFrame, FFrameNumber EndFrame) const;

	/**
	 * Get all notes with a specific gameplay tag
	 */
	TArray<FNoteInstance> GetNotesByTag(FGameplayTag Tag) const;

	/**
	 * Get the total number of notes in this section
	 */
	int32 GetNoteCount() const { return Notes.Num(); }

	/**
	 * Find the index of a note at a specific timestamp
	 * Returns -1 if not found
	 */
	int32 FindNoteIndex(FFrameNumber Timestamp) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};