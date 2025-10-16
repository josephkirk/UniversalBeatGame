// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneNoteChannel.h"
#include "Channels/MovieSceneChannelHandle.h"
#include "MovieSceneClipboard.h"

struct FKeyHandle;
struct FKeyDrawParams;
class UMovieSceneSection;

namespace MovieSceneClipboard
{
	/** Clipboard type name for note channel values */
	template<> inline FName GetKeyTypeName<FNoteChannelValue>()
	{
		static FName TypeName("FNoteChannelValue");
		return TypeName;
	}
}

namespace Sequencer
{
	/** Key drawing for note channel - displays notes as diamond markers */
	void DrawKeys(FMovieSceneNoteChannel* Channel, TArrayView<const FKeyHandle> InKeyHandles, const UMovieSceneSection* InOwner, TArrayView<FKeyDrawParams> OutKeyDrawParams);
}
