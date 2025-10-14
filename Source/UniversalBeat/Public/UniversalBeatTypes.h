// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UniversalBeatTypes.generated.h"

/**
 * Beat subdivision types for event broadcasting
 */
UENUM(BlueprintType)
enum class EBeatSubdivision : uint8
{
	None    UMETA(DisplayName = "Full Beats Only"),
	Half    UMETA(DisplayName = "Half Beats"),
	Quarter UMETA(DisplayName = "Quarter Beats"),
	Eighth  UMETA(DisplayName = "Eighth Beats")
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
