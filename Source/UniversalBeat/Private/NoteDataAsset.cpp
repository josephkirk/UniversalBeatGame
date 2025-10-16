// Copyright Epic Games, Inc. All Rights Reserved.

#include "NoteDataAsset.h"

UNoteDataAsset::UNoteDataAsset()
{
	// Set default values
	Label = TEXT("Unnamed Note");
	PreTiming = EMusicalNoteValue::Eighth;
	PostTiming = EMusicalNoteValue::Quarter;
	InteractionType = ENoteInteractionType::Press;
}

#if WITH_EDITOR
void UNoteDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property == nullptr)
	{
		return;
	}

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();

	// Validate timing window constraints
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UNoteDataAsset, PreTiming) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UNoteDataAsset, PostTiming))
	{
		// Timing windows are already constrained by the enum definition (Sixteenth to Whole)
		// No additional validation needed as the enum itself provides the constraints
		
		// Optional: Log a warning if using very strict timing
		if (PreTiming == EMusicalNoteValue::Sixteenth && PostTiming == EMusicalNoteValue::Sixteenth)
		{
			UE_LOG(LogTemp, Warning, TEXT("Note '%s': Very strict timing windows (1/16 + 1/16) may be difficult for players"), *Label);
		}
	}

	// Validate gameplay tag
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UNoteDataAsset, NoteTag))
	{
		if (!NoteTag.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Note '%s': Invalid or empty gameplay tag. This note cannot be validated during gameplay."), *Label);
		}
		else if (!NoteTag.ToString().StartsWith(TEXT("Input.")))
		{
			UE_LOG(LogTemp, Warning, TEXT("Note '%s': Gameplay tag '%s' should start with 'Input.' for consistency"), *Label, *NoteTag.ToString());
		}
	}

	// Validate label
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UNoteDataAsset, Label))
	{
		if (Label.IsEmpty())
		{
			Label = TEXT("Unnamed Note");
			UE_LOG(LogTemp, Warning, TEXT("Note label cannot be empty. Reset to 'Unnamed Note'"));
		}
	}
}
#endif