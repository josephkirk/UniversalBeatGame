// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * Editor module for UniversalBeat plugin
 * Registers custom track editors for Sequencer
 */
class FUniversalBeatEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Handle to registered track editor */
	FDelegateHandle NoteChartTrackEditorHandle;
};
