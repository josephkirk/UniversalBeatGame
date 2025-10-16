// Copyright Epic Games, Inc. All Rights Reserved.

#include "UniversalBeatEditorModule.h"
#include "Modules/ModuleManager.h"
#include "ISequencerModule.h"
#include "NoteChartTrackEditor.h"
#include "MovieSceneNoteChannel.h"
#include "MovieSceneNoteChannelEditor.h"  // Must be before SequencerChannelInterface.h to declare DrawKeys
#include "SequencerChannelInterface.h"

#define LOCTEXT_NAMESPACE "FUniversalBeatEditorModule"

void FUniversalBeatEditorModule::StartupModule()
{
	// Register track editor with Sequencer
	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	
	// Register note channel interface for sequencer UI
	SequencerModule.RegisterChannelInterface<FMovieSceneNoteChannel>();
	
	// Register track editor
	NoteChartTrackEditorHandle = SequencerModule.RegisterTrackEditor(
		FOnCreateTrackEditor::CreateStatic(&FNoteChartTrackEditor::CreateTrackEditor)
	);
}

void FUniversalBeatEditorModule::ShutdownModule()
{
	// Unregister track editor
	if (ISequencerModule* SequencerModule = FModuleManager::Get().GetModulePtr<ISequencerModule>("Sequencer"))
	{
		SequencerModule->UnRegisterTrackEditor(NoteChartTrackEditorHandle);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUniversalBeatEditorModule, UniversalBeatEditor)
