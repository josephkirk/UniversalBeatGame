// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Channels/MovieSceneChannel.h"
#include "Channels/MovieSceneChannelData.h"
#include "Channels/MovieSceneChannelTraits.h"
#include "MovieSceneSection.h"
#include "NoteDataAsset.h"
#include "MovieSceneNoteChannel.generated.h"

/**
 * Stores note-specific data for each keyframe in the channel
 */
USTRUCT()
struct UNIVERSALBEAT_API FNoteChannelValue
{
	GENERATED_BODY()

	/** Reference to note configuration data asset */
	UPROPERTY(EditAnywhere, Category = "Note")
	TObjectPtr<UNoteDataAsset> NoteData = nullptr;

	FNoteChannelValue() = default;
	
	explicit FNoteChannelValue(UNoteDataAsset* InNoteData)
		: NoteData(InNoteData)
	{}

	/** Comparison for sorting/deduplication */
	bool operator==(const FNoteChannelValue& Other) const
	{
		return NoteData == Other.NoteData;
	}

	bool operator!=(const FNoteChannelValue& Other) const
	{
		return !(*this == Other);
	}
};

/**
 * Channel structure for storing note keyframes with timestamps
 * Integrates with sequencer editor for visual keyframe editing
 */
USTRUCT()
struct UNIVERSALBEAT_API FMovieSceneNoteChannel : public FMovieSceneChannel
{
	GENERATED_BODY()

	typedef FNoteChannelValue CurveValueType;

	/**
	 * Access mutable interface for this channel's data
	 */
	FORCEINLINE TMovieSceneChannelData<FNoteChannelValue> GetData()
	{
		return TMovieSceneChannelData<FNoteChannelValue>(&KeyTimes, &KeyValues, this, &KeyHandles);
	}

	/**
	 * Access const interface for this channel's data
	 */
	FORCEINLINE TMovieSceneChannelData<const FNoteChannelValue> GetData() const
	{
		return TMovieSceneChannelData<const FNoteChannelValue>(&KeyTimes, &KeyValues);
	}

public:
	// ~ FMovieSceneChannel Interface
	virtual void GetKeys(const TRange<FFrameNumber>& WithinRange, TArray<FFrameNumber>* OutKeyTimes, TArray<FKeyHandle>* OutKeyHandles) override;
	virtual void GetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes) override;
	virtual void SetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<const FFrameNumber> InKeyTimes) override;
	virtual void DuplicateKeys(TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyHandle> OutNewHandles) override;
	virtual void DeleteKeys(TArrayView<const FKeyHandle> InHandles) override;
	virtual void DeleteKeysFrom(FFrameNumber InTime, bool bDeleteKeysBefore) override;
	virtual void ChangeFrameResolution(FFrameRate SourceRate, FFrameRate DestinationRate) override;
	virtual TRange<FFrameNumber> ComputeEffectiveRange() const override;
	virtual int32 GetNumKeys() const override;
	virtual void Reset() override;
	virtual void Offset(FFrameNumber DeltaPosition) override;
	// ~ End FMovieSceneChannel Interface

	/**
	 * Get handle for key at specified array index
	 */
	FKeyHandle GetHandle(int32 Index);

	/**
	 * Get array index for specified key handle
	 */
	virtual int32 GetIndex(FKeyHandle Handle) override;

private:
	/** Array of times for each key (frame numbers) */
	UPROPERTY(meta=(KeyTimes))
	TArray<FFrameNumber> KeyTimes;

	/** Array of values (note data) corresponding to each key time */
	UPROPERTY(meta=(KeyValues))
	TArray<FNoteChannelValue> KeyValues;

	/** Key handle map for editor operations (undo/redo, selection) - transient */
	FMovieSceneKeyHandleMap KeyHandles;
};

/**
 * Channel traits specialization for FMovieSceneNoteChannel
 * Notes don't interpolate or have default values
 */
template<>
struct TMovieSceneChannelTraits<FMovieSceneNoteChannel> : TMovieSceneChannelTraitsBase<FMovieSceneNoteChannel>
{
	enum { SupportsDefaults = false };
};

/**
 * Implement inline serialization support
 */
inline bool EvaluateChannel(const FMovieSceneNoteChannel* InChannel, FFrameTime InTime, FNoteChannelValue& OutValue)
{
	// Notes don't interpolate - exact frame match only
	return false;
}
