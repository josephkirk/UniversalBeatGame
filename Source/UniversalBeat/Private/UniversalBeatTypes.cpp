// Copyright Epic Games, Inc. All Rights Reserved.

#include "UniversalBeatTypes.h"

float ConvertMusicalNoteToSeconds(EMusicalNoteValue NoteValue, float BPM)
{
	if (BPM <= 0.0f)
	{
		return 0.0f;
	}
	
	// One quarter note = 60 seconds / BPM
	const float QuarterNoteSeconds = 60.0f / BPM;
	
	switch (NoteValue)
	{
	case EMusicalNoteValue::Sixteenth:
		return QuarterNoteSeconds / 4.0f;
	case EMusicalNoteValue::Eighth:
		return QuarterNoteSeconds / 2.0f;
	case EMusicalNoteValue::Quarter:
		return QuarterNoteSeconds;
	case EMusicalNoteValue::Half:
		return QuarterNoteSeconds * 2.0f;
	case EMusicalNoteValue::Whole:
		return QuarterNoteSeconds * 4.0f;
	default:
		return QuarterNoteSeconds;
	}
}
