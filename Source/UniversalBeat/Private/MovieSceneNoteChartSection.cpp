// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNoteChartSection.h"
#include "NoteDataAsset.h"

UMovieSceneNoteChartSection::UMovieSceneNoteChartSection()
{
	// Set default snap grid to 1/16th note
	SnapGridResolution = EMusicalNoteValue::Sixteenth;
	
	// Set default section range (0 to 10 seconds at 24fps)
	SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(240)));
	
	// Enable infinite section by default
	bSupportsInfiniteRange = true;
}

void UMovieSceneNoteChartSection::AddNote(FFrameNumber Timestamp, UNoteDataAsset* NoteData)
{
	if (!NoteData)
	{
		UE_LOG(LogTemp, Warning, TEXT("MovieSceneNoteChartSection::AddNote: Null NoteData provided"));
		return;
	}

	FNoteInstance NewNote(Timestamp, NoteData);
	Notes.Add(NewNote);
	
	// Sort after adding
	SortNotes();
	
	// Expand section range if needed
	TRange<FFrameNumber> CurrentRange = GetRange();
	if (!CurrentRange.Contains(Timestamp))
	{
		FFrameNumber NewStart = FMath::Min(CurrentRange.GetLowerBoundValue(), Timestamp);
		FFrameNumber NewEnd = FMath::Max(CurrentRange.GetUpperBoundValue(), Timestamp + 1);
		SetRange(TRange<FFrameNumber>(NewStart, NewEnd));
	}
}

void UMovieSceneNoteChartSection::RemoveNote(int32 Index)
{
	if (Notes.IsValidIndex(Index))
	{
		Notes.RemoveAt(Index);
	}
}

void UMovieSceneNoteChartSection::ClearNotes()
{
	Notes.Empty();
}

void UMovieSceneNoteChartSection::SortNotes()
{
	Notes.Sort([](const FNoteInstance& A, const FNoteInstance& B)
	{
		return A.Timestamp < B.Timestamp;
	});
}

TArray<FNoteInstance> UMovieSceneNoteChartSection::GetNotesInRange(FFrameNumber StartFrame, FFrameNumber EndFrame) const
{
	TArray<FNoteInstance> Result;
	
	for (const FNoteInstance& Note : Notes)
	{
		if (Note.Timestamp >= StartFrame && Note.Timestamp <= EndFrame)
		{
			Result.Add(Note);
		}
	}
	
	return Result;
}

TArray<FNoteInstance> UMovieSceneNoteChartSection::GetNotesByTag(FGameplayTag Tag) const
{
	TArray<FNoteInstance> Result;
	
	if (!Tag.IsValid())
	{
		return Result;
	}
	
	for (const FNoteInstance& Note : Notes)
	{
		if (Note.NoteData && Note.NoteData->GetNoteTag() == Tag)
		{
			Result.Add(Note);
		}
	}
	
	return Result;
}

int32 UMovieSceneNoteChartSection::FindNoteIndex(FFrameNumber Timestamp) const
{
	for (int32 i = 0; i < Notes.Num(); ++i)
	{
		if (Notes[i].Timestamp == Timestamp)
		{
			return i;
		}
	}
	return -1;
}

#if WITH_EDITOR
void UMovieSceneNoteChartSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (PropertyChangedEvent.Property == nullptr)
	{
		return;
	}
	
	const FName PropertyName = PropertyChangedEvent.Property->GetFName();
	
	// If notes array changed, ensure they're sorted
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMovieSceneNoteChartSection, Notes))
	{
		SortNotes();
		
		UE_LOG(LogTemp, Log, TEXT("MovieSceneNoteChartSection: Notes updated, count = %d"), Notes.Num());
	}
	
	// If snap grid changed, log it
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMovieSceneNoteChartSection, SnapGridResolution))
	{
		UE_LOG(LogTemp, Log, TEXT("MovieSceneNoteChartSection: Snap grid resolution changed"));
	}
}
#endif