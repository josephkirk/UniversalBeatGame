// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MovieSceneSection.h"
#include "UniversalBeatTypes.generated.h"

/**
 * Beat subdivision types for event broadcasting
 */
UENUM(BlueprintType)
enum class EBeatSubdivision : uint8
{
	None    UMETA(DisplayName = "No Broadcast Beat"),
	Whole    UMETA(DisplayName = "Full Beats Only"),
	Half    UMETA(DisplayName = "Half Beats"),
	Quarter UMETA(DisplayName = "Quarter Beats"),
	Eighth  UMETA(DisplayName = "Eighth Beats"),
	Sixteenth UMETA(DisplayName = "Sixteenth Beats")
};

/**
 * Result data from a beat timing accuracy check
 * Contains timing value (0.0-1.0), identifiers, and metadata
 */
USTRUCT(BlueprintType)
struct UNIVERSALBEAT_API FBeatTimingResult
{
	GENERATED_BODY()

	/** Input identifier (label name) - valid if tag is not used */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat")
	FName LabelName;

	/** Input identifier (gameplay tag) - valid if label is not used */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat")
	FGameplayTag InputTag;

	/** Timing accuracy value: 0.0 = mid-beat (worst), 1.0 = on-beat (perfect) */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat")
	float TimingValue = 0.0f;

	/** Absolute time when check was performed (from FPlatformTime::Seconds) */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat")
	double CheckTimestamp = 0.0;

	/** Current beat number since system started */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat")
	int32 BeatNumber = 0;

	FBeatTimingResult()
		: LabelName(NAME_None)
		, TimingValue(0.0f)
		, CheckTimestamp(0.0)
		, BeatNumber(0)
	{
	}
};

/**
 * Event data broadcast when a beat or beat subdivision occurs
 * Used for passive rhythm synchronization
 */
USTRUCT(BlueprintType)
struct UNIVERSALBEAT_API FBeatEventData
{
	GENERATED_BODY()

	/** Absolute beat number since system started */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat")
	int32 BeatNumber = 0;

	/** Subdivision index within current beat (0 = full beat, 1-3 = subdivisions) */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat")
	int32 SubdivisionIndex = 0;

	/** Active subdivision type */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat")
	EBeatSubdivision SubdivisionType = EBeatSubdivision::None;

	/** Absolute time of this beat event (from FPlatformTime::Seconds) */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat")
	double EventTimestamp = 0.0;

	FBeatEventData()
		: BeatNumber(0)
		, SubdivisionIndex(0)
		, SubdivisionType(EBeatSubdivision::None)
		, EventTimestamp(0.0)
	{
	}
};

// Forward declarations for note chart system
class UNoteDataAsset;

/**
 * Utility function to convert musical note values to seconds based on BPM
 * @param NoteValue The musical note value to convert
 * @param BPM Beats per minute for the calculation
 * @return Duration in seconds for the specified note value
 */
UNIVERSALBEAT_API float ConvertMusicalNoteToSeconds(EMusicalNoteValue NoteValue, float BPM);

/**
 * Musical note values for timing calculations
 */
UENUM(BlueprintType)
enum class EMusicalNoteValue : uint8
{
	Sixteenth	UMETA(DisplayName = "1/16th Note"),
	Eighth		UMETA(DisplayName = "1/8th Note"),
	Quarter		UMETA(DisplayName = "1/4 Note"),
	Half		UMETA(DisplayName = "1/2 Note"),
	Whole		UMETA(DisplayName = "Whole Note")
};

/**
 * Note interaction types for input validation
 */
UENUM(BlueprintType)
enum class ENoteInteractionType : uint8
{
	Press		UMETA(DisplayName = "Press"),
	Hold		UMETA(DisplayName = "Hold"),
	Release		UMETA(DisplayName = "Release")
};

/**
 * Timing direction indicators for input validation feedback
 */
UENUM(BlueprintType)
enum class ENoteTimingDirection : uint8
{
	Early		UMETA(DisplayName = "Early"),
	OnTime		UMETA(DisplayName = "On Time"),
	Late		UMETA(DisplayName = "Late")
};

/**
 * Result data from note timing validation
 * Contains accuracy, hit status, and timing feedback
 */
USTRUCT(BlueprintType)
struct UNIVERSALBEAT_API FNoteValidationResult
{
	GENERATED_BODY()

	/** Whether the input was within the valid timing window */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat|Notes")
	bool bHit = false;

	/** Accuracy value: 1.0 = perfect timing, 0.0 = edge of timing window */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat|Notes")
	float Accuracy = 0.0f;

	/** Whether input was early, on-time, or late */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat|Notes")
	ENoteTimingDirection TimingDirection = ENoteTimingDirection::OnTime;

	/** Time difference between input and note in seconds (negative = early, positive = late) */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat|Notes")
	float TimingOffset = 0.0f;

	/** Timestamp when input was received */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat|Notes")
	double InputTimestamp = 0.0;

	/** Timestamp of the target note */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat|Notes")
	double NoteTimestamp = 0.0;

	/** Gameplay tag of the note that was validated */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat|Notes")
	FGameplayTag NoteTag;

	/** Reference to the note data asset (if available) */
	UPROPERTY(BlueprintReadOnly, Category = "UniversalBeat|Notes")
	TObjectPtr<const UNoteDataAsset> NoteData = nullptr;

	FNoteValidationResult()
		: bHit(false)
		, Accuracy(0.0f)
		, TimingDirection(ENoteTimingDirection::OnTime)
		, TimingOffset(0.0f)
		, InputTimestamp(0.0)
		, NoteTimestamp(0.0)
	{
	}
};

/**
 * Instance of a note within a sequence at a specific timestamp
 */
USTRUCT(BlueprintType)
struct UNIVERSALBEAT_API FNoteInstance
{
	GENERATED_BODY()

	/** Frame number/timestamp within the sequence */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UniversalBeat|Notes")
	FFrameNumber Timestamp;

	/** Note data asset defining the note's properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UniversalBeat|Notes")
	TObjectPtr<UNoteDataAsset> NoteData = nullptr;

	FNoteInstance()
		: Timestamp(0)
	{
	}

	FNoteInstance(FFrameNumber InTimestamp, UNoteDataAsset* InNoteData)
		: Timestamp(InTimestamp)
		, NoteData(InNoteData)
	{
	}
};

/**
 * Entry for a track within a song configuration
 */
USTRUCT(BlueprintType)
struct UNIVERSALBEAT_API FNoteTrackEntry
{
	GENERATED_BODY()

	/** Level sequence containing the note chart track */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UniversalBeat|Song")
	TSoftObjectPtr<class ULevelSequence> TrackSequence;

	/** Delay in seconds before this track starts playing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UniversalBeat|Song", meta = (ClampMin = "0.0"))
	float DelayOffset = 0.0f;

	/** Whether this track should loop when it completes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UniversalBeat|Song")
	int LoopCount = 0;

	FNoteTrackEntry()
		: DelayOffset(0.0f)
		, LoopCount(0)
	{
	}
};
