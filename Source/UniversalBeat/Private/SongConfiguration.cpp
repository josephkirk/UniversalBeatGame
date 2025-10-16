// Copyright Epic Games, Inc. All Rights Reserved.

#include "SongConfiguration.h"
#include "LevelSequence.h"

USongConfiguration::USongConfiguration()
{
	// Set default values
	SongLabel = TEXT("Unnamed Song");
}

bool USongConfiguration::IsValid() const
{
	TArray<FString> Errors;
	ValidateConfiguration(Errors);
	return Errors.Num() == 0;
}

TArray<FString> USongConfiguration::GetValidationErrors() const
{
	TArray<FString> Errors;
	ValidateConfiguration(Errors);
	return Errors;
}

void USongConfiguration::ValidateConfiguration(TArray<FString>& OutErrors) const
{
	OutErrors.Empty();

	// Check basic properties
	if (SongLabel.IsEmpty())
	{
		OutErrors.Add(TEXT("Song label cannot be empty"));
	}

	if (!SongTag.IsValid())
	{
		OutErrors.Add(TEXT("Song tag is invalid or empty"));
	}
	else if (!SongTag.ToString().StartsWith(TEXT("Song.")))
	{
		OutErrors.Add(FString::Printf(TEXT("Song tag '%s' should start with 'Song.' for consistency"), *SongTag.ToString()));
	}

	// Check tracks
	if (Tracks.Num() == 0)
	{
		OutErrors.Add(TEXT("Song must have at least one track"));
		return; // No point checking individual tracks if there are none
	}

	for (int32 i = 0; i < Tracks.Num(); ++i)
	{
		const FNoteTrackEntry& Track = Tracks[i];
		const FString TrackPrefix = FString::Printf(TEXT("Track %d: "), i);

		if (Track.TrackSequence.IsNull())
		{
			OutErrors.Add(TrackPrefix + TEXT("Track sequence is not assigned"));
		}

		if (Track.DelayOffset < 0.0f)
		{
			OutErrors.Add(TrackPrefix + FString::Printf(TEXT("Delay offset cannot be negative (%.2f)"), Track.DelayOffset));
		}
	}

	// Check for duplicate sequences
	TSet<FSoftObjectPath> UniqueSequences;
	for (int32 i = 0; i < Tracks.Num(); ++i)
	{
		const FNoteTrackEntry& Track = Tracks[i];
		if (!Track.TrackSequence.IsNull())
		{
			const FSoftObjectPath SequencePath = Track.TrackSequence.ToSoftObjectPath();
			if (UniqueSequences.Contains(SequencePath))
			{
				OutErrors.Add(FString::Printf(TEXT("Track %d: Duplicate sequence reference (%s)"), i, *SequencePath.ToString()));
			}
			else
			{
				UniqueSequences.Add(SequencePath);
			}
		}
	}
}

#if WITH_EDITOR
void USongConfiguration::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property == nullptr)
	{
		return;
	}

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();

	// Validate song label
	if (PropertyName == GET_MEMBER_NAME_CHECKED(USongConfiguration, SongLabel))
	{
		if (SongLabel.IsEmpty())
		{
			SongLabel = TEXT("Unnamed Song");
			UE_LOG(LogTemp, Warning, TEXT("Song label cannot be empty. Reset to 'Unnamed Song'"));
		}
	}

	// Validate song tag
	if (PropertyName == GET_MEMBER_NAME_CHECKED(USongConfiguration, SongTag))
	{
		if (!SongTag.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Song '%s': Invalid or empty gameplay tag. This song cannot be played by tag."), *SongLabel);
		}
		else if (!SongTag.ToString().StartsWith(TEXT("Song.")))
		{
			UE_LOG(LogTemp, Warning, TEXT("Song '%s': Gameplay tag '%s' should start with 'Song.' for consistency"), *SongLabel, *SongTag.ToString());
		}
	}

	// Validate tracks array changes
	if (PropertyName == GET_MEMBER_NAME_CHECKED(USongConfiguration, Tracks))
	{
		// Clean up any tracks with negative delays
		for (FNoteTrackEntry& Track : Tracks)
		{
			if (Track.DelayOffset < 0.0f)
			{
				Track.DelayOffset = 0.0f;
				UE_LOG(LogTemp, Warning, TEXT("Song '%s': Track delay cannot be negative. Reset to 0.0"), *SongLabel);
			}
		}

		// Log validation status
		TArray<FString> Errors = GetValidationErrors();
		if (Errors.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("Song '%s' has %d validation error(s):"), *SongLabel, Errors.Num());
			for (const FString& Error : Errors)
			{
				UE_LOG(LogTemp, Warning, TEXT("  - %s"), *Error);
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Song '%s' configuration is valid"), *SongLabel);
		}
	}
}
#endif