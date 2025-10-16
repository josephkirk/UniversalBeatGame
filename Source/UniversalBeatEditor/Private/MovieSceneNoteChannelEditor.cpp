// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNoteChannelEditor.h"
#include "MovieSceneNoteChannel.h"
#include "SequencerChannelTraits.h"
#include "Styling/AppStyle.h"

namespace Sequencer
{

void DrawKeys(FMovieSceneNoteChannel* Channel, TArrayView<const FKeyHandle> InKeyHandles, const UMovieSceneSection* InOwner, TArrayView<FKeyDrawParams> OutKeyDrawParams)
{
	// Get channel data to retrieve note information
	TMovieSceneChannelData<const FNoteChannelValue> ChannelData = Channel->GetData();
	
	// Draw each key as a diamond marker
	for (int32 Index = 0; Index < InKeyHandles.Num(); ++Index)
	{
		FKeyDrawParams& Params = OutKeyDrawParams[Index];
		
		// Get the key index from handle using the channel's GetIndex method
		int32 KeyIndex = Channel->GetIndex(InKeyHandles[Index]);
		
		if (KeyIndex != INDEX_NONE && ChannelData.GetValues().IsValidIndex(KeyIndex))
		{
			const FNoteChannelValue& NoteValue = ChannelData.GetValues()[KeyIndex];
			
			// Set visual style for note keys
			Params.BorderBrush = FAppStyle::Get().GetBrush("Sequencer.KeyDiamond");
			Params.FillBrush = FAppStyle::Get().GetBrush("Sequencer.KeyDiamond");
			
			// Use different color if note has no data assigned
			if (NoteValue.NoteData)
			{
				Params.FillTint = FLinearColor(0.3f, 0.8f, 0.3f, 1.0f); // Green for valid notes
			}
			else
			{
				Params.FillTint = FLinearColor(0.8f, 0.3f, 0.3f, 1.0f); // Red for invalid notes
			}
		}
	}
}

} // namespace Sequencer

