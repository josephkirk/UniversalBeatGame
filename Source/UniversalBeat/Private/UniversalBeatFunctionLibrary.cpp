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

int32 UUniversalBeatFunctionLibrary::GetTicksForSubdivision(EBeatSubdivision Subdivision)
{
	// Map subdivision enum to tick divisor
	// Internal subdivision is always 16 (sixteenth notes)
	switch (Subdivision)
	{
	case EBeatSubdivision::None:
		return 0;  // No broadcasts
	case EBeatSubdivision::Whole:
		return 16;  // Every 16 ticks = whole beat
	case EBeatSubdivision::Half:
		return 8;   // Every 8 ticks = half beat
	case EBeatSubdivision::Quarter:
		return 4;   // Every 4 ticks = quarter beat
	case EBeatSubdivision::Eighth:
		return 2;   // Every 2 ticks = eighth beat
	case EBeatSubdivision::Sixteenth:
		return 1;   // Every tick = sixteenth beat
	default:
		return 0;
	}
}

int32 UUniversalBeatFunctionLibrary::GetSubdivisionMultiplier(EBeatSubdivision Subdivision)
{
	// Get subdivision multiplier for timer interval calculation
	switch (Subdivision)
	{
		case EBeatSubdivision::None:    return 1;
		case EBeatSubdivision::Whole:   return 1;
		case EBeatSubdivision::Half:    return 2;
		case EBeatSubdivision::Quarter: return 4;
		case EBeatSubdivision::Eighth:  return 8;
		case EBeatSubdivision::Sixteenth:  return 16;
		default: return 1;
	}
}
