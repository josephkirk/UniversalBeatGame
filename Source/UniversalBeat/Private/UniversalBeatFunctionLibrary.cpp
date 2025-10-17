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

bool UUniversalBeatFunctionLibrary::IsNoteSubdivision(const FBeatEventData& BeatEvent, EBeatSubdivision TargetSubdivision)
{
	// Get the ticks per target subdivision
	int32 TicksPerSubdivision = GetTicksForSubdivision(TargetSubdivision);
	
	// If target is None or invalid, return false
	if (TicksPerSubdivision <= 0)
	{
		return false;
	}
	
	// Check if the target subdivision is coarser than or equal to the current broadcast subdivision
	// We can only detect subdivisions that are coarser than or equal to what's being broadcast
	int32 BroadcastTicks = GetTicksForSubdivision(BeatEvent.SubdivisionType);
	if (TicksPerSubdivision > BroadcastTicks)
	{
		// Target is finer than broadcast - cannot detect
		// E.g., asking for Eighth notes when only broadcasting Quarter notes
		return false;
	}
	
	// If target matches the broadcast subdivision, always true
	if (TargetSubdivision == BeatEvent.SubdivisionType)
	{
		return true;
	}
	
	// Check if the SubdivisionIndex aligns with the target subdivision
	// The SubdivisionIndex cycles based on the broadcast subdivision rate:
	// - Broadcasting at Sixteenth: SubdivisionIndex cycles 0-15
	// - Broadcasting at Eighth: SubdivisionIndex cycles 0-7
	// - Broadcasting at Quarter: SubdivisionIndex cycles 0-3
	// - Broadcasting at Half: SubdivisionIndex cycles 0-1
	// - Broadcasting at Whole: SubdivisionIndex is always 0
	//
	// We need to calculate how many broadcast ticks represent one target subdivision tick.
	// For example, if broadcasting at Eighth (2 ticks per 16th) and checking for Quarter (4 ticks per 16th):
	// - BroadcastTicks = 2, TicksPerSubdivision = 4
	// - TicksPerBroadcast = 4 / 2 = 2
	// - Quarter notes occur every 2 eighth-note events (indices 0, 2, 4, 6)
	
	int32 TicksPerBroadcast = TicksPerSubdivision / BroadcastTicks;
	
	// Special case: if target is coarser than broadcast by exactly the broadcast rate
	// E.g., checking for Whole (16) when broadcasting at Whole (16)
	if (TicksPerBroadcast == 0)
	{
		return BeatEvent.SubdivisionIndex == 0;
	}
	
	return (BeatEvent.SubdivisionIndex % TicksPerBroadcast) == 0;
}
