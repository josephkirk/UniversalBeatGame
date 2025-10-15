// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNoteChartTrack.h"
#include "MovieSceneNoteChartSection.h"

#define LOCTEXT_NAMESPACE "MovieSceneNoteChartTrack"

UMovieSceneNoteChartTrack::UMovieSceneNoteChartTrack()
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(200, 100, 200, 65); // Purple tint for note tracks
#endif
}

bool UMovieSceneNoteChartTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneNoteChartSection::StaticClass();
}

UMovieSceneSection* UMovieSceneNoteChartTrack::CreateNewSection()
{
	UMovieSceneNoteChartSection* NewSection = NewObject<UMovieSceneNoteChartSection>(this, NAME_None, RF_Transactional);
	return NewSection;
}

const TArray<UMovieSceneSection*>& UMovieSceneNoteChartTrack::GetAllSections() const
{
	return NoteChartSections;
}

void UMovieSceneNoteChartTrack::RemoveAllAnimationData()
{
	NoteChartSections.Empty();
}

bool UMovieSceneNoteChartTrack::HasSection(const UMovieSceneSection& Section) const
{
	return NoteChartSections.Contains(&Section);
}

void UMovieSceneNoteChartTrack::AddSection(UMovieSceneSection& Section)
{
	NoteChartSections.Add(&Section);
}

void UMovieSceneNoteChartTrack::RemoveSection(UMovieSceneSection& Section)
{
	NoteChartSections.Remove(&Section);
}

void UMovieSceneNoteChartTrack::RemoveSectionAt(int32 SectionIndex)
{
	if (NoteChartSections.IsValidIndex(SectionIndex))
	{
		NoteChartSections.RemoveAt(SectionIndex);
	}
}

bool UMovieSceneNoteChartTrack::IsEmpty() const
{
	return NoteChartSections.Num() == 0;
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneNoteChartTrack::GetDisplayName() const
{
	return LOCTEXT("TrackName", "Note Chart");
}
#endif

#undef LOCTEXT_NAMESPACE