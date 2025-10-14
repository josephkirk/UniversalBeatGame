// Copyright Epic Games, Inc. All Rights Reserved.

#include "UniversalBeatSubsystem.h"
#include "TimerManager.h"
#include "Engine/World.h"

// Logging category
DEFINE_LOG_CATEGORY_STATIC(LogUniversalBeat, Log, All);

void UUniversalBeatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// T016: Set default values
	CurrentBPM = 120.0f;
	StartTime = FPlatformTime::Seconds();
	CalibrationOffsetMs = 0.0f;
	bRespectTimeDilation = false;
	bBeatBroadcastingEnabled = false;
	bDebugLoggingEnabled = false;
	CurrentSubdivision = EBeatSubdivision::None;
	bCurveFallbackWarningLogged = false;

	// Load default timing curve asset (will be created in T010)
	// TimingCurve = LoadObject<UCurveFloat>(nullptr, TEXT("/UniversalBeat/Curves/DefaultTimingCurve"));
	
	// T018: Start periodic drift compensation timer (runs continuously)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DriftCompensationTimer,
			this,
			&UUniversalBeatSubsystem::RealignBeatTracking,
			1.0f,
			true  // Loop indefinitely
		);
	}

	UE_LOG(LogUniversalBeat, Log, TEXT("UniversalBeatSubsystem initialized - BPM: %.1f, StartTime: %.3f"), CurrentBPM, StartTime);
}

void UUniversalBeatSubsystem::Deinitialize()
{
	// T017: Clean up timers
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DriftCompensationTimer);
		World->GetTimerManager().ClearTimer(BeatBroadcastTimer);
		World->GetTimerManager().ClearTimer(CalibrationTimer);
	}
	
	Super::Deinitialize();
	UE_LOG(LogUniversalBeat, Log, TEXT("UniversalBeatSubsystem deinitialized"));
}

// ====================================================================
// 1. BPM Configuration
// ====================================================================

void UUniversalBeatSubsystem::SetBPM(float NewBPM)
{
	// T011: Validate BPM and reset to default 120 if invalid
	if (NewBPM <= 0.0f)
	{
		UE_LOG(LogUniversalBeat, Error, TEXT("Invalid BPM value %.2f rejected, resetting to default 120"), NewBPM);
		CurrentBPM = 120.0f;
		StartTime = FPlatformTime::Seconds();
		return;
	}
	
	// Validate practical range (warning only, not enforced)
	if (NewBPM < 20.0f || NewBPM > 400.0f)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("BPM %.2f is outside practical range [20-400]"), NewBPM);
	}
	
	// Validate typical range (warning only)
	if (NewBPM < 60.0f || NewBPM > 240.0f)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("BPM %.2f is outside typical range [60-240]"), NewBPM);
	}
	
	CurrentBPM = NewBPM;
	StartTime = FPlatformTime::Seconds();  // Reset start time to prevent phase discontinuity
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("BPM changed to %.2f, StartTime reset to %.3f"), CurrentBPM, StartTime);
	}
}

float UUniversalBeatSubsystem::GetBPM() const
{
	// TODO: Implement in Phase 3 (T012)
	return CurrentBPM;
}

float UUniversalBeatSubsystem::GetSecondsPerBeat() const
{
	// T013: Calculate seconds per beat from BPM
	return 60.0f / CurrentBPM;
}

void UUniversalBeatSubsystem::SetRespectTimeDilation(bool bRespect)
{
	// T014: Set time dilation mode
	bRespectTimeDilation = bRespect;
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("Time dilation mode: %s"), 
			bRespect ? TEXT("Respecting (beats slow with dilation)") : TEXT("Real-time (beats ignore dilation)"));
	}
}

bool UUniversalBeatSubsystem::GetRespectTimeDilation() const
{
	// T014: Get time dilation mode
	return bRespectTimeDilation;
}

// ====================================================================
// 2. Timing Checks
// ====================================================================

float UUniversalBeatSubsystem::CheckBeatTimingByLabel(FName LabelName)
{
	// T021: Check timing with label identifier
	return CheckBeatTimingInternal(LabelName, FGameplayTag());
}

float UUniversalBeatSubsystem::CheckBeatTimingByTag(FGameplayTag InputTag)
{
	// T022: Check timing with tag identifier
	return CheckBeatTimingInternal(NAME_None, InputTag);
}

void UUniversalBeatSubsystem::SetTimingCurve(UCurveFloat* NewCurve)
{
	// T023: Set timing curve asset (null is valid - uses fallback)
	TimingCurve = NewCurve;
	
	// Reset fallback warning flag when curve changes
	bCurveFallbackWarningLogged = false;
	
	if (bDebugLoggingEnabled)
	{
		if (NewCurve)
		{
			UE_LOG(LogUniversalBeat, Log, TEXT("Timing curve set to: %s"), *NewCurve->GetName());
		}
		else
		{
			UE_LOG(LogUniversalBeat, Log, TEXT("Timing curve cleared - will use linear fallback"));
		}
	}
}

UCurveFloat* UUniversalBeatSubsystem::GetTimingCurve() const
{
	// T023: Get current timing curve asset
	return TimingCurve;
}

// ====================================================================
// 3. Calibration
// ====================================================================

void UUniversalBeatSubsystem::SetCalibrationOffset(float OffsetMs)
{
	// T030: Validate and set calibration offset (FR-019a/b)
	float ClampedOffset = FMath::Clamp(OffsetMs, -200.0f, 200.0f);
	
	if (ClampedOffset != OffsetMs)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("Calibration offset %.2fms out of range [-200, +200], clamped to %.2fms"), OffsetMs, ClampedOffset);
	}
	
	if (FMath::Abs(ClampedOffset) > 100.0f)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("Calibration offset %.2fms is large (>100ms), may indicate configuration issue"), ClampedOffset);
	}
	
	CalibrationOffsetMs = ClampedOffset;
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("Calibration offset set to %.2fms"), CalibrationOffsetMs);
	}
}

float UUniversalBeatSubsystem::GetCalibrationOffset() const
{
	// T030: Get current calibration offset
	return CalibrationOffsetMs;
}

void UUniversalBeatSubsystem::RunCalibrationSequence(int32 NumPrompts)
{
	// T032: Simplified calibration sequence
	// For full implementation, would need timer-based prompt presentation
	// This version provides the basic structure
	
	int32 ClampedPrompts = FMath::Clamp(NumPrompts, 5, 20);
	
	CalibrationTotalPrompts = ClampedPrompts;
	CalibrationPromptsRemaining = ClampedPrompts;
	CalibrationOffsets.Empty();
	
	UE_LOG(LogUniversalBeat, Log, TEXT("Calibration sequence started - %d prompts"), ClampedPrompts);
	
	// In a full implementation, this would:
	// 1. Present visual/audio prompts at beat intervals
	// 2. Collect player input timing via timing checks
	// 3. Calculate average offset from collected data
	// 4. Broadcast OnCalibrationComplete with result
	
	// For now, complete immediately with placeholder
	CompleteCalibrationSequence();
}

// ====================================================================
// 4. Beat Broadcasting
// ====================================================================

void UUniversalBeatSubsystem::EnableBeatBroadcasting(EBeatSubdivision Subdivision)
{
	// T034: Enable beat broadcasting with subdivisions
	bBeatBroadcastingEnabled = true;
	CurrentSubdivision = Subdivision;
	
	if (!GetWorld())
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("Cannot enable beat broadcasting - no valid world"));
		return;
	}
	
	// Calculate timer interval based on subdivision
	float SubdivisionMultiplier = GetSubdivisionMultiplier(Subdivision);
	float SecondsPerBeat = 60.0f / CurrentBPM;
	float TimerInterval = SecondsPerBeat / SubdivisionMultiplier;
	
	// Start timer
	GetWorld()->GetTimerManager().SetTimer(
		BeatBroadcastTimer,
		this,
		&UUniversalBeatSubsystem::BroadcastBeatEvent,
		TimerInterval,
		true  // Loop
	);
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("Beat broadcasting enabled - Subdivision:%d Interval:%.3fs"), 
			(int32)Subdivision, TimerInterval);
	}
}

void UUniversalBeatSubsystem::DisableBeatBroadcasting()
{
	// T035: Disable beat broadcasting
	bBeatBroadcastingEnabled = false;
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeatBroadcastTimer);
	}
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("Beat broadcasting disabled"));
	}
}

bool UUniversalBeatSubsystem::IsBeatBroadcastingEnabled() const
{
	// T036: Query beat broadcasting state
	return bBeatBroadcastingEnabled;
}

// ====================================================================
// 5. Debug & Utility
// ====================================================================

void UUniversalBeatSubsystem::EnableDebugLogging(bool bEnabled)
{
	// T041: Enable/disable debug logging
	bDebugLoggingEnabled = bEnabled;
	UE_LOG(LogUniversalBeat, Log, TEXT("Debug logging %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

bool UUniversalBeatSubsystem::IsDebugLoggingEnabled() const
{
	// T041: Query debug logging state
	return bDebugLoggingEnabled;
}

int32 UUniversalBeatSubsystem::GetCurrentBeatNumber() const
{
	// T024: Calculate beat number from elapsed time
	if (!GetWorld())
	{
		return 0;
	}
	
	double CurrentTime = bRespectTimeDilation 
		? GetWorld()->GetTimeSeconds() 
		: GetWorld()->GetRealTimeSeconds();
	
	double CalibrationOffsetSeconds = CalibrationOffsetMs / 1000.0;
	double AdjustedTime = CurrentTime + CalibrationOffsetSeconds;
	
	double SecondsPerBeat = 60.0 / CurrentBPM;
	double ElapsedBeats = (AdjustedTime - StartTime) / SecondsPerBeat;
	
	return FMath::FloorToInt(ElapsedBeats);
}

float UUniversalBeatSubsystem::GetCurrentBeatPhase() const
{
	// T025: Return calculated beat phase
	return CalculateBeatPhase();
}

// ====================================================================
// Internal Helper Functions
// ====================================================================

float UUniversalBeatSubsystem::CalculateBeatPhase() const
{
	// T015: Core beat tracking logic with time dilation and calibration support
	// T019: Enhanced pause handling (FR-004b/c)
	if (!GetWorld())
	{
		return 0.0f;
	}
	
	// Check for pause state if respecting time dilation
	bool bCurrentlyPaused = bRespectTimeDilation && IsPausedState();
	
	if (bCurrentlyPaused)
	{
		// FR-004c: Beat tracking pauses, but timing checks still function
		// Return cached phase from when pause started
		if (!bIsPaused)
		{
			// Just entered pause - cache current state
			bIsPaused = true;
			CachedPauseTime = GetWorld()->GetTimeSeconds();
			// Calculate and cache the phase at pause time
			double CurrentTime = GetWorld()->GetRealTimeSeconds();
			double CalibrationOffsetSeconds = CalibrationOffsetMs / 1000.0;
			double AdjustedTime = CurrentTime + CalibrationOffsetSeconds;
			double SecondsPerBeat = 60.0 / CurrentBPM;
			double ElapsedBeats = (AdjustedTime - StartTime) / SecondsPerBeat;
			CachedPausePhase = FMath::Frac(ElapsedBeats);
		}
		// Return frozen phase
		return CachedPausePhase;
	}
	else if (bIsPaused)
	{
		// Just exited pause - reset flag
		bIsPaused = false;
	}
	
	// Get current time based on time dilation setting
	double CurrentTime;
	if (bRespectTimeDilation)
	{
		CurrentTime = GetWorld()->GetTimeSeconds();  // Dilated time
	}
	else
	{
		CurrentTime = GetWorld()->GetRealTimeSeconds();  // Real-time (ignores dilation)
	}
	
	// Apply calibration offset
	double CalibrationOffsetSeconds = CalibrationOffsetMs / 1000.0;
	double AdjustedTime = CurrentTime + CalibrationOffsetSeconds;
	
	// Calculate elapsed beats since start
	double SecondsPerBeat = 60.0 / CurrentBPM;
	double ElapsedBeats = (AdjustedTime - StartTime) / SecondsPerBeat;
	
	// Calculate beat phase (0.0 to 1.0 within current beat)
	float BeatPhase = FMath::Frac(ElapsedBeats);
	
	return BeatPhase;
}

float UUniversalBeatSubsystem::EvaluateTimingCurve(float BeatPhase)
{
	// T020: Curve evaluation with validation and fallback
	float RawValue;
	
	if (TimingCurve && TimingCurve->FloatCurve.GetNumKeys() > 0)
	{
		// Valid curve - evaluate it
		RawValue = TimingCurve->GetFloatValue(BeatPhase);
	}
	else
	{
		// Invalid/null curve - use linear fallback (FR-007a)
		if (!bCurveFallbackWarningLogged)
		{
			UE_LOG(LogUniversalBeat, Warning, TEXT("TimingCurve is null or invalid, using linear fallback"));
			bCurveFallbackWarningLogged = true;
		}
		
		// Linear fallback: Triangle wave
		// 0.0→0.5 = rise from 0.0 to 1.0
		// 0.5→1.0 = fall from 1.0 to 0.0
		if (BeatPhase <= 0.5f)
		{
			RawValue = BeatPhase * 2.0f;
		}
		else
		{
			RawValue = (1.0f - BeatPhase) * 2.0f;
		}
	}
	
	// Clamp to valid range [0.0, 1.0] (FR-007b)
	float ClampedValue = FMath::Clamp(RawValue, 0.0f, 1.0f);
	if (ClampedValue != RawValue)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("Curve returned out-of-range value %.3f, clamped to %.3f"), RawValue, ClampedValue);
	}
	
	return ClampedValue;
}

float UUniversalBeatSubsystem::CheckBeatTimingInternal(FName LabelName, FGameplayTag InputTag)
{
	// T021: Internal timing check implementation
	
	// Handle null/empty label (FR-011a)
	FName SafeLabelName = (LabelName.IsNone() || LabelName == NAME_None) 
		? FName("Default") 
		: LabelName;
	
	// Get current beat phase
	float BeatPhase = CalculateBeatPhase();
	
	// Evaluate timing value via curve
	float TimingValue = EvaluateTimingCurve(BeatPhase);
	
	// Get current beat number for metadata
	int32 BeatNumber = GetCurrentBeatNumber();
	
	// Get timestamp
	double CheckTimestamp = FPlatformTime::Seconds();
	
	// Create result struct
	FBeatTimingResult Result;
	Result.LabelName = SafeLabelName;
	Result.InputTag = InputTag;
	Result.TimingValue = TimingValue;
	Result.CheckTimestamp = CheckTimestamp;
	Result.BeatNumber = BeatNumber;
	
	// Broadcast event (Phase 5 - T026)
	OnBeatInputCheck.Broadcast(SafeLabelName, InputTag, TimingValue);
	
	// Debug logging
	if (bDebugLoggingEnabled)
	{
		if (!SafeLabelName.IsNone())
		{
			UE_LOG(LogUniversalBeat, Log, TEXT("Timing Check [Label:%s] - Beat:%d Phase:%.3f Value:%.3f"), 
				*SafeLabelName.ToString(), BeatNumber, BeatPhase, TimingValue);
		}
		else
		{
			UE_LOG(LogUniversalBeat, Log, TEXT("Timing Check [Tag:%s] - Beat:%d Phase:%.3f Value:%.3f"), 
				*InputTag.ToString(), BeatNumber, BeatPhase, TimingValue);
		}
	}
	
	return TimingValue;
}

void UUniversalBeatSubsystem::RealignBeatTracking()
{
	// T018: Periodic drift compensation (1 second intervals)
	// Recalculate current beat number and phase from absolute StartTime
	// This prevents unbounded drift over hours of gameplay
	
	if (!GetWorld())
	{
		return;
	}
	
	double CurrentTime = bRespectTimeDilation 
		? GetWorld()->GetTimeSeconds() 
		: GetWorld()->GetRealTimeSeconds();
	
	double CalibrationOffsetSeconds = CalibrationOffsetMs / 1000.0;
	double AdjustedTime = CurrentTime + CalibrationOffsetSeconds;
	
	double SecondsPerBeat = 60.0 / CurrentBPM;
	double ElapsedBeats = (AdjustedTime - StartTime) / SecondsPerBeat;
	
	// Update cached beat number and phase (for reference)
	int32 CurrentBeatNumber = FMath::FloorToInt(ElapsedBeats);
	float CurrentBeatPhase = FMath::Frac(ElapsedBeats);
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Verbose, TEXT("Drift compensation: Beat %d, Phase %.3f"), CurrentBeatNumber, CurrentBeatPhase);
	}
}

void UUniversalBeatSubsystem::BroadcastBeatEvent()
{
	// T037: Broadcast beat event with metadata
	int32 CurrentBeatNum = GetCurrentBeatNumber();
	float CurrentPhase = GetCurrentBeatPhase();
	
	// Calculate subdivision index based on phase
	float SubdivisionMultiplier = GetSubdivisionMultiplier(CurrentSubdivision);
	int32 SubdivisionIndex = FMath::FloorToInt(CurrentPhase * SubdivisionMultiplier);
	
	// Create event data
	FBeatEventData EventData;
	EventData.BeatNumber = CurrentBeatNum;
	EventData.SubdivisionIndex = SubdivisionIndex;
	EventData.SubdivisionType = CurrentSubdivision;
	EventData.EventTimestamp = FPlatformTime::Seconds();
	
	// Broadcast event
	OnBeat.Broadcast(EventData);
	
	// Debug logging
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Verbose, TEXT("Beat Event - Beat:%d SubdivIdx:%d Type:%d Phase:%.3f"), 
			CurrentBeatNum, SubdivisionIndex, (int32)CurrentSubdivision, CurrentPhase);
	}
}

void UUniversalBeatSubsystem::PresentCalibrationPrompt()
{
	// T032: Present a calibration prompt (to be called by timer)
	// Game-specific implementation would trigger UI/audio cue here
	UE_LOG(LogUniversalBeat, Log, TEXT("Calibration prompt %d/%d"), 
		(CalibrationTotalPrompts - CalibrationPromptsRemaining + 1), CalibrationTotalPrompts);
}

void UUniversalBeatSubsystem::ProcessCalibrationInput(float TimingValue)
{
	// T032: Process player input during calibration
	// This would be called from game code when player responds to prompt
	// Calculate offset based on timing value and expected beat time
	CalibrationOffsets.Add(TimingValue);
	CalibrationPromptsRemaining--;
	
	if (CalibrationPromptsRemaining <= 0)
	{
		CompleteCalibrationSequence();
	}
}

void UUniversalBeatSubsystem::CompleteCalibrationSequence()
{
	// T032: Calculate final calibration offset and broadcast result
	float CalculatedOffset = 0.0f;
	bool bSuccess = false;
	
	if (CalibrationOffsets.Num() > 0)
	{
		// Calculate average offset from collected data
		float Sum = 0.0f;
		for (float Offset : CalibrationOffsets)
		{
			Sum += Offset;
		}
		CalculatedOffset = Sum / CalibrationOffsets.Num();
		bSuccess = true;
	}
	
	// Broadcast completion event
	OnCalibrationComplete.Broadcast(CalculatedOffset, bSuccess);
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("Calibration complete - Offset:%.2fms Success:%s"), 
			CalculatedOffset, bSuccess ? TEXT("true") : TEXT("false"));
	}
	
	// Reset calibration state
	CalibrationOffsets.Empty();
	CalibrationPromptsRemaining = 0;
	CalibrationTotalPrompts = 0;
}

float UUniversalBeatSubsystem::GetSubdivisionMultiplier(EBeatSubdivision Subdivision) const
{
	// T034: Get subdivision multiplier for timer interval calculation
	switch (Subdivision)
	{
		case EBeatSubdivision::None:    return 1.0f;
		case EBeatSubdivision::Half:    return 2.0f;
		case EBeatSubdivision::Quarter: return 4.0f;
		case EBeatSubdivision::Eighth:  return 8.0f;
		default: return 1.0f;
	}
}

bool UUniversalBeatSubsystem::IsPausedState() const
{
	// T019: Check if game is paused (time dilation == 0)
	if (UWorld* World = GetWorld())
	{
		return World->GetWorldSettings()->TimeDilation == 0.0f;
	}
	return false;
}
