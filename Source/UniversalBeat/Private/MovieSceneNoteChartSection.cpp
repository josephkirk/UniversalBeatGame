// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNoteChartSection.h"
#include "NoteDataAsset.h"
#include "UniversalBeatSubsystem.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "EntitySystem/MovieSceneEntityBuilder.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "Evaluation/MovieSceneEvaluationField.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"

UMovieSceneNoteChartSection::UMovieSceneNoteChartSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set default snap grid to 1/16th note
	SnapGridResolution = EMusicalNoteValue::Sixteenth;
	
	// Set default section range (0 to 10 seconds at 24fps)
	SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(240)));
	
	// Enable infinite section by default
	bSupportsInfiniteRange = true;

	// Register note channel with channel proxy
	FMovieSceneChannelProxyData Channels;
#if WITH_EDITOR
	FMovieSceneChannelMetaData NoteMetaData;
	NoteMetaData.SetIdentifiers("Notes", NSLOCTEXT("MovieSceneNoteChartSection", "NotesText", "Notes"));
	Channels.Add(NoteChannel, NoteMetaData);
#else
	Channels.Add(NoteChannel);
#endif
	
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
}

void UMovieSceneNoteChartSection::PostLoad()
{
	Super::PostLoad();
	
	// Migrate deprecated notes array to channel
	if (Notes_DEPRECATED.Num() > 0)
	{
		TMovieSceneChannelData<FNoteChannelValue> ChannelData = NoteChannel.GetData();
		
		for (const FNoteInstance& Note : Notes_DEPRECATED)
		{
			FNoteChannelValue Value(Note.NoteData);
			ChannelData.AddKey(Note.Timestamp, MoveTemp(Value));
		}
		
		// Clear deprecated array after migration
		Notes_DEPRECATED.Empty();
		MarkPackageDirty();
		
		UE_LOG(LogTemp, Log, TEXT("MovieSceneNoteChartSection: Migrated %d notes from deprecated array to channel"), NoteChannel.GetNumKeys());
	}
}

bool UMovieSceneNoteChartSection::PopulateEvaluationFieldImpl(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder)
{
	// Get channel data to access key indices
	TMovieSceneChannelData<const FNoteChannelValue> ChannelData = NoteChannel.GetData();
	TArrayView<const FFrameNumber> AllKeyTimes = ChannelData.GetTimes();
	TArrayView<const FNoteChannelValue> AllKeyValues = ChannelData.GetValues();
	
	if (AllKeyTimes.Num() == 0)
	{
		return false;
	}
	
	// Create one-shot entity for each note in the effective range
	const int32 MetaDataIndex = OutFieldBuilder->AddMetaData(InMetaData);
	bool bAddedAnyEntities = false;
	
	for (int32 KeyIndex = 0; KeyIndex < AllKeyTimes.Num(); ++KeyIndex)
	{
		const FFrameNumber NoteTime = AllKeyTimes[KeyIndex];
		
		// Only add entities within the effective range
		if (EffectiveRange.Contains(NoteTime))
		{
			// Pass actual key index so ImportEntityImpl can retrieve the correct note
			OutFieldBuilder->AddOneShotEntity(TRange<FFrameNumber>(NoteTime), this, KeyIndex, MetaDataIndex);
			bAddedAnyEntities = true;
		}
	}
	
	return bAddedAnyEntities;
}

void UMovieSceneNoteChartSection::ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity)
{
	// Entity index corresponds to key index in channel
	const int32 KeyIndex = Params.EntityID;
	
	// Retrieve note data from channel
	TMovieSceneChannelData<const FNoteChannelValue> ChannelData = NoteChannel.GetData();
	TArrayView<const FNoteChannelValue> KeyValues = ChannelData.GetValues();
	TArrayView<const FFrameNumber> KeyTimes = ChannelData.GetTimes();
	
	if (!KeyValues.IsValidIndex(KeyIndex) || !KeyTimes.IsValidIndex(KeyIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("MovieSceneNoteChartSection::ImportEntityImpl: Invalid key index %d"), KeyIndex);
		return;
	}
	
	const FNoteChannelValue& NoteValue = KeyValues[KeyIndex];
	const FFrameNumber NoteTime = KeyTimes[KeyIndex];
	
	if (!NoteValue.NoteData)
	{
		UE_LOG(LogTemp, Warning, TEXT("MovieSceneNoteChartSection::ImportEntityImpl: Note at index %d has no assigned NoteDataAsset"), KeyIndex);
		return;
	}
	
	// Add note to runtime notes array (existing AddNote behavior)
	// The UniversalBeatSubsystem will query this array
	FNoteInstance RuntimeNote(NoteTime, NoteValue.NoteData);
	RuntimeNotes.Add(RuntimeNote);
	
	// Broadcast OnNoteBeat event to subsystem
	// Get world from the EntityLinker instead of GetWorld() which doesn't work in this context
	if (EntityLinker)
	{
		UWorld* World = EntityLinker->GetWorld();
		if (World && World->GetGameInstance())
		{
			
			if (UUniversalBeatSubsystem* Subsystem = World->GetSubsystem<UUniversalBeatSubsystem>())
			{
				Subsystem->OnNoteBeat.Broadcast(RuntimeNote);
					
				if (Subsystem->IsDebugLoggingEnabled())
				{
					UE_LOG(LogTemp, Log, TEXT("MovieSceneNoteChartSection::ImportEntityImpl: Broadcasted OnNoteBeat for note at frame %d"), NoteTime.Value);
				}
			}
		}
	}
	
	UE_LOG(LogTemp, Verbose, TEXT("MovieSceneNoteChartSection::ImportEntityImpl: Note triggered at frame %d with data %s"), 
		NoteTime.Value, *NoteValue.NoteData->GetName());
}

TOptional<TRange<FFrameNumber>> UMovieSceneNoteChartSection::GetAutoSizeRange() const
{
	TRange<FFrameNumber> EffectiveRange = NoteChannel.ComputeEffectiveRange();
	
	if (!EffectiveRange.IsEmpty())
	{
		return EffectiveRange;
	}
	
	return TOptional<TRange<FFrameNumber>>();
}

FKeyHandle UMovieSceneNoteChartSection::AddNote(FFrameNumber Timestamp, UNoteDataAsset* NoteData)
{
	if (!NoteData)
	{
		UE_LOG(LogTemp, Warning, TEXT("MovieSceneNoteChartSection::AddNote: Null NoteData provided"));
		return FKeyHandle::Invalid();
	}

	// Add to channel
	TMovieSceneChannelData<FNoteChannelValue> ChannelData = NoteChannel.GetData();
	FNoteChannelValue Value(NoteData);
	int32 KeyIndex = ChannelData.AddKey(Timestamp, MoveTemp(Value));
	FKeyHandle Handle = ChannelData.GetHandle(KeyIndex);
	
	// Also add to runtime notes for immediate queries
	FNoteInstance RuntimeNote(Timestamp, NoteData);
	RuntimeNotes.Add(RuntimeNote);
	
	// Expand section range if needed
	TRange<FFrameNumber> CurrentRange = GetRange();
	if (!CurrentRange.Contains(Timestamp))
	{
		FFrameNumber NewStart = FMath::Min(CurrentRange.GetLowerBoundValue(), Timestamp);
		FFrameNumber NewEnd = FMath::Max(CurrentRange.GetUpperBoundValue(), Timestamp + 1);
		SetRange(TRange<FFrameNumber>(NewStart, NewEnd));
	}
	
	return Handle;
}

bool UMovieSceneNoteChartSection::RemoveNote(FKeyHandle Handle)
{
	TMovieSceneChannelData<FNoteChannelValue> ChannelData = NoteChannel.GetData();
	
	// Find and remove from channel
	int32 Index = ChannelData.GetIndex(Handle);
	if (Index != INDEX_NONE)
	{
		ChannelData.RemoveKey(Index);
		
		// Rebuild runtime notes
		RuntimeNotes.Empty();
		return true;
	}
	
	return false;
}

int32 UMovieSceneNoteChartSection::RemoveNotesInRange(FFrameNumber StartFrame, FFrameNumber EndFrame)
{
	TMovieSceneChannelData<FNoteChannelValue> ChannelData = NoteChannel.GetData();
	TArrayView<const FFrameNumber> KeyTimes = ChannelData.GetTimes();
	
	TArray<FKeyHandle> HandlesToDelete;
	
	for (int32 Idx = 0; Idx < KeyTimes.Num(); ++Idx)
	{
		if (KeyTimes[Idx] >= StartFrame && KeyTimes[Idx] <= EndFrame)
		{
			HandlesToDelete.Add(ChannelData.GetHandle(Idx));
		}
	}
	
	int32 NumRemoved = HandlesToDelete.Num();
	
	for (FKeyHandle Handle : HandlesToDelete)
	{
		RemoveNote(Handle);
	}
	
	return NumRemoved;
}

void UMovieSceneNoteChartSection::GetNotesInRange(FFrameNumber StartFrame, FFrameNumber EndFrame, TArray<FNoteInstance>& OutNotes) const
{
	OutNotes.Empty();
	
	TMovieSceneChannelData<const FNoteChannelValue> ChannelData = NoteChannel.GetData();
	TArrayView<const FFrameNumber> KeyTimes = ChannelData.GetTimes();
	TArrayView<const FNoteChannelValue> KeyValues = ChannelData.GetValues();
	
	for (int32 Idx = 0; Idx < KeyTimes.Num(); ++Idx)
	{
		if (KeyTimes[Idx] >= StartFrame && KeyTimes[Idx] <= EndFrame)
		{
			if (KeyValues[Idx].NoteData)
			{
				OutNotes.Add(FNoteInstance(KeyTimes[Idx], KeyValues[Idx].NoteData));
			}
		}
	}
}

void UMovieSceneNoteChartSection::GetNotesByTag(FGameplayTag Tag, TArray<FNoteInstance>& OutNotes) const
{
	OutNotes.Empty();
	
	if (!Tag.IsValid())
	{
		return;
	}
	
	TMovieSceneChannelData<const FNoteChannelValue> ChannelData = NoteChannel.GetData();
	TArrayView<const FFrameNumber> KeyTimes = ChannelData.GetTimes();
	TArrayView<const FNoteChannelValue> KeyValues = ChannelData.GetValues();
	
	for (int32 Idx = 0; Idx < KeyTimes.Num(); ++Idx)
	{
		if (KeyValues[Idx].NoteData && KeyValues[Idx].NoteData->GetNoteTag() == Tag)
		{
			OutNotes.Add(FNoteInstance(KeyTimes[Idx], KeyValues[Idx].NoteData));
		}
	}
}

#if WITH_EDITOR
void UMovieSceneNoteChartSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (PropertyChangedEvent.Property == nullptr)
	{
		return;
	}
	
	const FName PropertyName = PropertyChangedEvent.Property->GetFName();
	
	// If note channel changed, log it
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMovieSceneNoteChartSection, NoteChannel))
	{
		UE_LOG(LogTemp, Log, TEXT("MovieSceneNoteChartSection: Note channel updated, count = %d"), NoteChannel.GetNumKeys());
	}
	
	// If snap grid changed, log it
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMovieSceneNoteChartSection, SnapGridResolution))
	{
		UE_LOG(LogTemp, Log, TEXT("MovieSceneNoteChartSection: Snap grid resolution changed"));
	}
}
#endif