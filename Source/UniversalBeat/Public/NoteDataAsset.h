// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UniversalBeatTypes.h"
#include "NoteDataAsset.generated.h"

/**
 * Data asset defining the properties of a note type
 * Configures timing windows, visual representation, and interaction mechanics
 */
UCLASS(BlueprintType, Const)
class UNIVERSALBEAT_API UNoteDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UNoteDataAsset();

	/** Human-readable label for the note type (e.g., "Left Arrow", "Jump") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (Tooltip = "Display name for this note type"))
	FString Label;

	/** Gameplay tag identifying this note type (e.g., "Input.Left") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (Categories = "Input", Tooltip = "Unique gameplay tag identifier for input validation"))
	FGameplayTag NoteTag;

	/** How early input is accepted, measured in musical note fractions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (Tooltip = "Timing window before the note - larger values = more forgiving"))
	EMusicalNoteValue PreTiming;

	/** How late input is accepted, measured in musical note fractions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (Tooltip = "Timing window after the note - larger values = more forgiving"))
	EMusicalNoteValue PostTiming;

	/** Icon texture for visual identification in UI and sequence editor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual", meta = (Tooltip = "Icon displayed in UI and sequencer track"))
	TObjectPtr<UTexture2D> IconTexture;

	/** Interaction type: Press, Hold, or Release */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (Tooltip = "Type of input interaction required (Press validation only in v1)"))
	ENoteInteractionType InteractionType;

	// Blueprint getter functions for const access
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Notes", meta = (Tooltip = "Get the display label for this note type"))
	const FString& GetLabel() const { return Label; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Notes", meta = (Tooltip = "Get the gameplay tag for this note type"))
	const FGameplayTag& GetNoteTag() const { return NoteTag; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Notes", meta = (Tooltip = "Get the pre-timing window as musical note value"))
	EMusicalNoteValue GetPreTiming() const { return PreTiming; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Notes", meta = (Tooltip = "Get the post-timing window as musical note value"))
	EMusicalNoteValue GetPostTiming() const { return PostTiming; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Notes", meta = (Tooltip = "Get the icon texture for UI display"))
	UTexture2D* GetIconTexture() const { return IconTexture; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Notes", meta = (Tooltip = "Get the interaction type for this note"))
	ENoteInteractionType GetInteractionType() const { return InteractionType; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};