// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UniversalBeatTypes.h"
#include "SongConfiguration.generated.h"

/**
 * Data asset defining a song with multiple coordinated note tracks
 * Configures track sequences, delays, looping, and lifecycle events
 */
UCLASS(BlueprintType)
class UNIVERSALBEAT_API USongConfiguration : public UDataAsset
{
	GENERATED_BODY()

public:
	USongConfiguration();

	/** Human-readable label for the song (e.g., "Tutorial Song", "Level 1 Theme") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (Tooltip = "Display name for this song"))
	FString SongLabel;

	/** Gameplay tag identifying this song (e.g., "Song.Tutorial") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (Categories = "Song", Tooltip = "Unique gameplay tag identifier for song selection"))
	FGameplayTag SongTag;

	/** Array of note tracks that make up this song */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracks", meta = (Tooltip = "Note chart tracks with timing and looping configuration"))
	TArray<FNoteTrackEntry> Tracks;

	// Blueprint getter functions for const access
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Songs", meta = (Tooltip = "Get the display label for this song"))
	const FString& GetSongLabel() const { return SongLabel; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Songs", meta = (Tooltip = "Get the gameplay tag for this song"))
	const FGameplayTag& GetSongTag() const { return SongTag; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Songs", meta = (Tooltip = "Get the array of note tracks"))
	const TArray<FNoteTrackEntry>& GetTracks() const { return Tracks; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Songs", meta = (Tooltip = "Get the number of tracks in this song"))
	int32 GetTrackCount() const { return Tracks.Num(); }

	/**
	 * Validate the song configuration for completeness and consistency
	 * @return True if the song is valid and ready for playback
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Songs", meta = (Tooltip = "Check if song configuration is valid for playback"))
	bool IsValid() const;

	/**
	 * Get validation issues as human-readable text
	 * @return Array of validation error messages (empty if valid)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Songs", meta = (Tooltip = "Get list of configuration problems"))
	TArray<FString> GetValidationErrors() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Internal validation helper */
	void ValidateConfiguration(TArray<FString>& OutErrors) const;
};