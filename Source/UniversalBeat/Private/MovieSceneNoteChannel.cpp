// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNoteChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieSceneFrameMigration.h"

void FMovieSceneNoteChannel::GetKeys(const TRange<FFrameNumber>& WithinRange, TArray<FFrameNumber>* OutKeyTimes, TArray<FKeyHandle>* OutKeyHandles)
{
	GetData().GetKeys(WithinRange, OutKeyTimes, OutKeyHandles);
}

void FMovieSceneNoteChannel::GetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes)
{
	GetData().GetKeyTimes(InHandles, OutKeyTimes);
}

void FMovieSceneNoteChannel::SetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<const FFrameNumber> InKeyTimes)
{
	GetData().SetKeyTimes(InHandles, InKeyTimes);
}

void FMovieSceneNoteChannel::DuplicateKeys(TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyHandle> OutNewHandles)
{
	GetData().DuplicateKeys(InHandles, OutNewHandles);
}

void FMovieSceneNoteChannel::DeleteKeys(TArrayView<const FKeyHandle> InHandles)
{
	GetData().DeleteKeys(InHandles);
}

void FMovieSceneNoteChannel::DeleteKeysFrom(FFrameNumber InTime, bool bDeleteKeysBefore)
{
	if (bDeleteKeysBefore)
	{
		// Delete all keys before specified time
		int32 FirstIndexToDelete = 0;
		int32 LastIndexToDelete = Algo::LowerBound(KeyTimes, InTime) - 1;
		
		if (LastIndexToDelete >= 0)
		{
			KeyTimes.RemoveAt(FirstIndexToDelete, LastIndexToDelete + 1);
			KeyValues.RemoveAt(FirstIndexToDelete, LastIndexToDelete + 1);
			KeyHandles.Reset();
		}
	}
	else
	{
		// Delete all keys from specified time onwards
		int32 FirstIndexToDelete = Algo::LowerBound(KeyTimes, InTime);
		
		if (FirstIndexToDelete < KeyTimes.Num())
		{
			int32 NumToDelete = KeyTimes.Num() - FirstIndexToDelete;
			KeyTimes.RemoveAt(FirstIndexToDelete, NumToDelete);
			KeyValues.RemoveAt(FirstIndexToDelete, NumToDelete);
			KeyHandles.Reset();
		}
	}
}

void FMovieSceneNoteChannel::ChangeFrameResolution(FFrameRate SourceRate, FFrameRate DestinationRate)
{
	check(KeyTimes.Num() == KeyValues.Num());
	
	for (FFrameNumber& Time : KeyTimes)
	{
		Time = ConvertFrameTime(FFrameTime(Time), SourceRate, DestinationRate).FloorToFrame();
	}
}

TRange<FFrameNumber> FMovieSceneNoteChannel::ComputeEffectiveRange() const
{
	if (KeyTimes.Num() > 0)
	{
		return TRange<FFrameNumber>(KeyTimes[0], KeyTimes.Last() + 1);
	}
	return TRange<FFrameNumber>::Empty();
}

int32 FMovieSceneNoteChannel::GetNumKeys() const
{
	return KeyTimes.Num();
}

void FMovieSceneNoteChannel::Reset()
{
	KeyTimes.Reset();
	KeyValues.Reset();
	KeyHandles.Reset();
}

void FMovieSceneNoteChannel::Offset(FFrameNumber DeltaPosition)
{
	for (FFrameNumber& Time : KeyTimes)
	{
		Time += DeltaPosition;
	}
}

FKeyHandle FMovieSceneNoteChannel::GetHandle(int32 Index)
{
	return GetData().GetHandle(Index);
}

int32 FMovieSceneNoteChannel::GetIndex(FKeyHandle Handle)
{
	return GetData().GetIndex(Handle);
}
