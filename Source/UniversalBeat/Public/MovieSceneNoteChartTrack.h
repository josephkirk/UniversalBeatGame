// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneNameableTrack.h"
#include "MovieSceneNoteChartTrack.generated.h"

/**
 * Movie scene track for note charts
 * Manages note chart sections within a level sequence
 */
UCLASS()
class UNIVERSALBEAT_API UMovieSceneNoteChartTrack : public UMovieSceneNameableTrack
{
	GENERATED_BODY()

public:
	UMovieSceneNoteChartTrack();

	// UMovieSceneTrack interface
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual void RemoveAllAnimationData() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual void RemoveSectionAt(int32 SectionIndex) override;
	virtual bool IsEmpty() const override;
	
#if WITH_EDITORONLY_DATA
	virtual FText GetDisplayName() const override;
#endif

private:
	/** All note chart sections in this track */
	UPROPERTY()
	TArray<TObjectPtr<UMovieSceneSection>> NoteChartSections;
};