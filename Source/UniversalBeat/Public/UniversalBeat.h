// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * UniversalBeat Module
 * 
 * Genre-agnostic rhythm game system providing beat tracking, timing checks,
 * and event-driven integration for Blueprint-based rhythm mechanics.
 */
class FUniversalBeatModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
