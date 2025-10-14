// Copyright Epic Games, Inc. All Rights Reserved.

#include "UniversalBeat.h"

#define LOCTEXT_NAMESPACE "FUniversalBeatModule"

// Define logging category for UniversalBeat
DEFINE_LOG_CATEGORY_STATIC(LogUniversalBeat, Log, All);

void FUniversalBeatModule::StartupModule()
{
	// This code will execute after your module is loaded into memory
	UE_LOG(LogUniversalBeat, Log, TEXT("UniversalBeat module started"));
}

void FUniversalBeatModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module
	UE_LOG(LogUniversalBeat, Log, TEXT("UniversalBeat module shutdown"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUniversalBeatModule, UniversalBeat)
