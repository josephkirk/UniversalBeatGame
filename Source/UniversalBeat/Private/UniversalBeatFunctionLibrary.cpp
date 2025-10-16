// Copyright Epic Games, Inc. All Rights Reserved.

#include "UniversalBeatFunctionLibrary.h"

float UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue NoteValue, float BPM)
{
	return ::ConvertMusicalNoteToSeconds(NoteValue, BPM);
}

float UUniversalBeatFunctionLibrary::GetNoteValueMultiplier(EMusicalNoteValue NoteValue)
{
	switch (NoteValue)
	{
	case EMusicalNoteValue::Sixteenth:
		return 0.25f;
	case EMusicalNoteValue::Eighth:
		return 0.5f;
	case EMusicalNoteValue::Quarter:
		return 1.0f;
	case EMusicalNoteValue::Half:
		return 2.0f;
	case EMusicalNoteValue::Whole:
		return 4.0f;
	default:
		return 1.0f;
	}
}

void UUniversalBeatFunctionLibrary::CalculateTimingWindows(EMusicalNoteValue PreTiming, EMusicalNoteValue PostTiming, float BPM, float& OutPreSeconds, float& OutPostSeconds)
{
	OutPreSeconds = ConvertMusicalNoteToSeconds(PreTiming, BPM);
	OutPostSeconds = ConvertMusicalNoteToSeconds(PostTiming, BPM);
}