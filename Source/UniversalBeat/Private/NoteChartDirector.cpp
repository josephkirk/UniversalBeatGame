// Copyright Epic Games, Inc. All Rights Reserved.

#include "NoteChartDirector.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneNoteChartTrack.h"
#include "MovieSceneNoteChartSection.h"
#include "NoteDataAsset.h"
#include "UniversalBeatSubsystem.h"
#include "UniversalBeatFunctionLibrary.h"
#include "Engine/World.h"

UNoteChartDirector::UNoteChartDirector()
{
	CurrentNoteIndex = 0;
	LastUpdateTime = 0.0f;
}

void UNoteChartDirector::InitializeNoteChart()
{
	// Initialize note tracking from the sequence
	InitializeNoteTracking();
	
	UE_LOG(LogTemp, Log, TEXT("NoteChartDirector::InitializeNoteChart - Loaded %d notes"), CachedNotesSorted.Num());
}

void UNoteChartDirector::InitializeNoteTracking()
{
	// Clear existing data
	CachedNotesSorted.Empty();
	ConsumedNoteTimestamps.Empty();
	CurrentNoteIndex = 0;
	LastUpdateTime = 0.0f;
	
	// Load all notes from sequence
	LoadNotesFromSequence();
	
	// Sort notes by timestamp
	CachedNotesSorted.Sort([](const FNoteInstance& A, const FNoteInstance& B)
	{
		return A.Timestamp < B.Timestamp;
	});
}

void UNoteChartDirector::LoadNotesFromSequence()
{
	// Get the level sequence from the player property (UE 5.6 API)
	if (!Player)
	{
		UE_LOG(LogTemp, Warning, TEXT("NoteChartDirector::LoadNotesFromSequence - No sequence player"));
		return;
	}
	
	// Get the level sequence
	ULevelSequence* Sequence = Cast<ULevelSequence>(Player->GetSequence());
	if (!Sequence)
	{
		UE_LOG(LogTemp, Warning, TEXT("NoteChartDirector::LoadNotesFromSequence - No level sequence"));
		return;
	}
	
	// Get the movie scene
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		UE_LOG(LogTemp, Warning, TEXT("NoteChartDirector::LoadNotesFromSequence - No movie scene"));
		return;
	}
	
	// Find all note chart tracks (GetTracks() in UE 5.6, was GetMasterTracks() in older versions)
	const TArray<UMovieSceneTrack*>& AllTracks = MovieScene->GetTracks();
	for (UMovieSceneTrack* Track : AllTracks)
	{
		UMovieSceneNoteChartTrack* NoteTrack = Cast<UMovieSceneNoteChartTrack>(Track);
		if (!NoteTrack)
		{
			continue;
		}
		
		// Get all sections from this track
		const TArray<UMovieSceneSection*>& Sections = NoteTrack->GetAllSections();
		for (UMovieSceneSection* Section : Sections)
		{
			UMovieSceneNoteChartSection* NoteSection = Cast<UMovieSceneNoteChartSection>(Section);
			if (NoteSection)
			{
				// Add all notes from this section
				CachedNotesSorted.Append(NoteSection->Notes);
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("NoteChartDirector::LoadNotesFromSequence - Found %d note chart tracks with %d total notes"), 
		AllTracks.Num(), CachedNotesSorted.Num());
}

bool UNoteChartDirector::GetNextNoteForTag(FGameplayTag NoteTag, float CurrentTime, FNoteInstance& OutNote)
{
	if (!NoteTag.IsValid())
	{
		return false;
	}
	
	// Get BPM from subsystem
	float BPM = GetCurrentBPM();
	if (BPM <= 0.0f)
	{
		return false;
	}
	
	// Binary search for notes around current time
	// Start from current index to optimize sequential playback
	for (int32 i = CurrentNoteIndex; i < CachedNotesSorted.Num(); ++i)
	{
		const FNoteInstance& Note = CachedNotesSorted[i];
		
		// Skip if already consumed
		if (ConsumedNoteTimestamps.Contains(Note.Timestamp.Value))
		{
			continue;
		}
		
		// Skip if wrong tag
		if (!Note.NoteData || Note.NoteData->GetNoteTag() != NoteTag)
		{
			continue;
		}
		
		// Convert note timestamp to seconds
		float NoteTimeSeconds = FrameToSeconds(Note.Timestamp);
		
		// Calculate timing windows
		float PreTimingSeconds = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(
			Note.NoteData->GetPreTiming(), BPM);
		float PostTimingSeconds = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(
			Note.NoteData->GetPostTiming(), BPM);
		
		float WindowStart = NoteTimeSeconds - PreTimingSeconds;
		float WindowEnd = NoteTimeSeconds + PostTimingSeconds;
		
		// Check if current time is within timing window
		if (CurrentTime >= WindowStart && CurrentTime <= WindowEnd)
		{
			// Update current index for next search
			CurrentNoteIndex = i;
			OutNote = Note;
			return true;
		}
		
		// If we've passed the window, this note is missed
		if (CurrentTime > WindowEnd)
		{
			// TODO: Trigger miss event
			continue;
		}
		
		// If we haven't reached the window yet, stop searching
		if (CurrentTime < WindowStart)
		{
			break;
		}
	}
	
	return false;
}

void UNoteChartDirector::MarkNoteConsumed(const FNoteInstance& Note)
{
	ConsumedNoteTimestamps.Add(Note.Timestamp.Value);
}

bool UNoteChartDirector::IsNoteConsumed(const FNoteInstance& Note) const
{
	return ConsumedNoteTimestamps.Contains(Note.Timestamp.Value);
}

void UNoteChartDirector::ResetConsumedNotes()
{
	ConsumedNoteTimestamps.Empty();
	CurrentNoteIndex = 0;
	LastUpdateTime = 0.0f;
	
	UE_LOG(LogTemp, Log, TEXT("NoteChartDirector::ResetConsumedNotes - Reset for loop/restart"));
}

TArray<FNoteInstance> UNoteChartDirector::GetAllNotes() const
{
	return CachedNotesSorted;
}

int32 UNoteChartDirector::GetTotalNoteCount() const
{
	return CachedNotesSorted.Num();
}

float UNoteChartDirector::GetCurrentBPM() const
{
	// Get BPM from UniversalBeatSubsystem
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UUniversalBeatSubsystem* BeatSubsystem = GameInstance->GetSubsystem<UUniversalBeatSubsystem>())
			{
				return BeatSubsystem->GetBPM();
			}
		}
	}
	
	return 120.0f; // Default fallback BPM
}

void UNoteChartDirector::UpdateMissDetection(float CurrentTime)
{
	// TODO: Implement in Phase 6
	// Check if any unconsumed notes have expired their post-timing window
	// Call OnNoteMissed for each missed note
	
	LastUpdateTime = CurrentTime;
}

float UNoteChartDirector::FrameToSeconds(FFrameNumber Frame) const
{
	// Use Player property directly (UE 5.6 API)
	if (!Player)
	{
		return 0.0f;
	}
	
	ULevelSequence* Sequence = Cast<ULevelSequence>(Player->GetSequence());
	if (!Sequence || !Sequence->GetMovieScene())
	{
		return 0.0f;
	}
	
	FFrameRate FrameRate = Sequence->GetMovieScene()->GetDisplayRate();
	return FrameRate.AsSeconds(Frame);
}

FFrameNumber UNoteChartDirector::SecondsToFrame(float Seconds) const
{
	// Use Player property directly (UE 5.6 API)
	if (!Player)
	{
		return FFrameNumber(0);
	}
	
	ULevelSequence* Sequence = Cast<ULevelSequence>(Player->GetSequence());
	if (!Sequence || !Sequence->GetMovieScene())
	{
		return FFrameNumber(0);
	}
	
	FFrameRate FrameRate = Sequence->GetMovieScene()->GetDisplayRate();
	return FrameRate.AsFrameNumber(Seconds);
}