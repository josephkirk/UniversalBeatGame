// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "MovieSceneSection.h"
#include "Sections/MovieSceneEventSectionBase.h"
#include "EntitySystem/IMovieSceneEntityProvider.h"
#include "UniversalBeatTypes.h"
#include "MovieSceneNoteChannel.h"
#include "MovieSceneNoteChartSection.generated.h"

/**
 * Movie scene section that contains note chart data
 * Uses channel-based storage following event section patterns
 * Implements entity provider for frame-accurate note triggering
 */
UCLASS()
class UNIVERSALBEAT_API UMovieSceneNoteChartSection : public UMovieSceneSection, public IMovieSceneEntityProvider
{
	GENERATED_BODY()

public:
	UMovieSceneNoteChartSection(const FObjectInitializer& ObjectInitializer);

	/** Channel storing note keyframes (primary storage) */
	UPROPERTY()
	FMovieSceneNoteChannel NoteChannel;

	/** Legacy storage for backward compatibility - deprecated, migrated in PostLoad */
	UPROPERTY()
	TArray<FNoteInstance> Notes_DEPRECATED;

	/** Transient cache for runtime queries to avoid allocations during playback */
	UPROPERTY(Transient)
	TArray<FNoteInstance> RuntimeNotes;

	/** Snap grid resolution for note placement in editor */
	UPROPERTY(EditAnywhere, Category = "Note Chart")
	EMusicalNoteValue SnapGridResolution;

	// ~ IMovieSceneEntityProvider Interface
	virtual void ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity) override;
	virtual bool PopulateEvaluationFieldImpl(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder) override;
	// ~ End IMovieSceneEntityProvider Interface

	// ~ UObject Interface
	virtual void PostLoad() override;
	// ~ End UObject Interface

	// ~ UMovieSceneSection Interface
	virtual TOptional<TRange<FFrameNumber>> GetAutoSizeRange() const override;
	// ~ End UMovieSceneSection Interface

	/**
	 * Access underlying note channel for advanced operations
	 */
	FMovieSceneNoteChannel& GetNoteChannel() { return NoteChannel; }
	const FMovieSceneNoteChannel& GetNoteChannel() const { return NoteChannel; }

	/**
	 * Add a note to the section at the specified timestamp
	 * Adds to both channel and runtime notes array
	 */
	FKeyHandle AddNote(FFrameNumber Timestamp, UNoteDataAsset* NoteData);

	/**
	 * Remove a note using its key handle
	 */
	bool RemoveNote(FKeyHandle Handle);

	/**
	 * Remove all notes within a specific frame range
	 */
	int32 RemoveNotesInRange(FFrameNumber StartFrame, FFrameNumber EndFrame);

	/**
	 * Get all notes within a specific frame range
	 */
	void GetNotesInRange(FFrameNumber StartFrame, FFrameNumber EndFrame, TArray<FNoteInstance>& OutNotes) const;

	/**
	 * Get all notes with a specific gameplay tag
	 */
	void GetNotesByTag(FGameplayTag Tag, TArray<FNoteInstance>& OutNotes) const;

	/**
	 * Get the total number of notes in this section
	 */
	int32 GetNoteCount() const { return NoteChannel.GetNumKeys(); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};