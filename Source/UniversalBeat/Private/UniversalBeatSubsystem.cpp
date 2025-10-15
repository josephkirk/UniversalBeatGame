// Copyright Epic Games, Inc. All Rights Reserved.

#include "UniversalBeatSubsystem.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "SongConfiguration.h"
#include "NoteDataAsset.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "UniversalBeatFunctionLibrary.h"
#include "MovieScene.h"
#include "MovieSceneNoteChartTrack.h"
#include "MovieSceneNoteChartSection.h"
#include "Engine/LocalPlayer.h"

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

	// Initialize note chart tracking
	CurrentNoteIndex = 0;
	CachedSequenceFrameRate = FFrameRate(60, 1); // Default 60 FPS

	// SongPlayer actor will be spawned when needed (lazy initialization)

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
	// Clean up SongPlayer actor
	if (SongPlayerActor)
	{
		SongPlayerActor->Destroy();
		SongPlayerActor = nullptr;
	}

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

// ====================================================================
// Note Chart System Implementation (Stub implementations for Phase 2)
// ====================================================================

FNoteValidationResult UUniversalBeatSubsystem::CheckBeatTimingByTag(FGameplayTag InputTag)
{
	// T070: Full validation implementation with note chart integration
	FNoteValidationResult Result;
	Result.bHit = false;
	Result.NoteTag = InputTag;
	Result.InputTimestamp = FPlatformTime::Seconds();
	Result.Accuracy = 0.0f;
	Result.TimingDirection = ENoteTimingDirection::OnTime;
	Result.TimingOffset = 0.0f;
	
	// Check if we have loaded notes and a valid input tag
	if (CachedNotesSorted.Num() == 0 || !InputTag.IsValid())
	{
		// T073: Fallback to standard beat timing when no note chart loaded
		Result.Accuracy = CheckBeatTimingInternal(NAME_None, InputTag);
		Result.TimingOffset = 0.0f; // Standard timing doesn't provide offset
		
		if (bDebugLoggingEnabled)
		{
			UE_LOG(LogUniversalBeat, Log, TEXT("CheckBeatTimingByTag: No notes loaded, falling back to standard timing - Tag=%s, Accuracy=%.3f"), 
				*InputTag.ToString(), Result.Accuracy);
		}
		
		return Result;
	}
	
	// T074: Query for next note with matching tag
	FNoteInstance FoundNote;
	float CurrentTime = GetCurrentPlaybackTime();
	
	if (!GetNextNoteForTag(InputTag, CurrentTime, FoundNote))
	{
		// No note found within timing window for this tag
		Result.bHit = false;
		Result.Accuracy = 0.0f;
		Result.TimingDirection = ENoteTimingDirection::Late; // Assume late if no note found
		
		if (bDebugLoggingEnabled)
		{
			UE_LOG(LogUniversalBeat, Log, TEXT("CheckBeatTimingByTag: No note found for tag '%s' at time %.3f (Miss)"), 
				*InputTag.ToString(), CurrentTime);
		}
		
		return Result;
	}
	
	// T075: Populate validation result with note data
	Result.NoteTag = FoundNote.NoteData->GetNoteTag();
	Result.NoteData = FoundNote.NoteData;
	Result.NoteTimestamp = FrameToSeconds(FoundNote.Timestamp);
	
	// T072: Calculate timing offset and direction
	Result.TimingOffset = CurrentTime - Result.NoteTimestamp;
	
	if (FMath::Abs(Result.TimingOffset) < 0.001f) // Within 1ms = perfect
	{
		Result.TimingDirection = ENoteTimingDirection::OnTime;
	}
	else if (Result.TimingOffset < 0.0f)
	{
		Result.TimingDirection = ENoteTimingDirection::Early;
	}
	else
	{
		Result.TimingDirection = ENoteTimingDirection::Late;
	}
	
	// T071: Calculate accuracy (1.0 = perfect, 0.0 = edge of timing window)
	// Get timing windows from note data asset
	float PreTimingSeconds = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(
		FoundNote.NoteData->GetPreTiming(), CurrentBPM);
	float PostTimingSeconds = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(
		FoundNote.NoteData->GetPostTiming(), CurrentBPM);
	
	// Maximum acceptable timing difference is the appropriate window based on direction
	float MaxTimingWindow = (Result.TimingOffset < 0.0f) ? PreTimingSeconds : PostTimingSeconds;
	
	// Accuracy = 1.0 - (|offset| / max_window), clamped to [0, 1]
	Result.Accuracy = FMath::Clamp(1.0f - (FMath::Abs(Result.TimingOffset) / MaxTimingWindow), 0.0f, 1.0f);
	Result.bHit = true;
	
	// T074: Mark note as consumed to prevent re-validation
	MarkNoteConsumed(FoundNote);
	
	// T076: Log validation event for debugging
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("CheckBeatTimingByTag: HIT - Tag=%s, Accuracy=%.3f, Offset=%.4fs, Direction=%s"), 
			*InputTag.ToString(), 
			Result.Accuracy, 
			Result.TimingOffset,
			Result.TimingDirection == ENoteTimingDirection::Early ? TEXT("Early") : 
			Result.TimingDirection == ENoteTimingDirection::OnTime ? TEXT("OnTime") : TEXT("Late"));
	}
	
	return Result;
}

bool UUniversalBeatSubsystem::RegisterSong(USongConfiguration* SongConfig)
{
	// TODO: Implement in Phase 8 (User Story 6)
	if (!SongConfig || !SongConfig->GetSongTag().IsValid())
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("RegisterSong: Invalid song configuration"));
		return false;
	}
	
	if (RegisteredSongs.Contains(SongConfig->GetSongTag()))
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("RegisterSong: Song with tag '%s' already registered"), 
			*SongConfig->GetSongTag().ToString());
		return false;
	}
	
	RegisteredSongs.Add(SongConfig->GetSongTag(), SongConfig);
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("RegisterSong: Registered song '%s' with tag '%s'"), 
			*SongConfig->GetSongLabel(), *SongConfig->GetSongTag().ToString());
	}
	
	return true;
}

bool UUniversalBeatSubsystem::UnregisterSong(FGameplayTag SongTag)
{
	// TODO: Implement in Phase 8 (User Story 6)
	if (!RegisteredSongs.Contains(SongTag))
	{
		return false;
	}
	
	// Stop the song if it's currently playing
	if (CurrentlyPlayingSong && CurrentlyPlayingSong->GetSongTag() == SongTag)
	{
		StopCurrentSong();
	}
	
	RegisteredSongs.Remove(SongTag);
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("UnregisterSong: Removed song with tag '%s'"), *SongTag.ToString());
	}
	
	return true;
}

bool UUniversalBeatSubsystem::PlaySongByTag(FGameplayTag SongTag)
{
	// TODO: Implement in Phase 8 (User Story 6)
	TObjectPtr<USongConfiguration>* FoundSong = RegisteredSongs.Find(SongTag);
	if (!FoundSong || !*FoundSong)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("PlaySongByTag: Song with tag '%s' not found"), *SongTag.ToString());
		return false;
	}
	
	// Stop current song if any
	if (CurrentlyPlayingSong)
	{
		StopCurrentSong();
	}
	
	CurrentlyPlayingSong = *FoundSong;
	
	// Broadcast OnSongStarted event
	OnSongStarted.Broadcast(SongTag);
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("PlaySongByTag: Started song '%s' with tag '%s'"), 
			*CurrentlyPlayingSong->GetSongLabel(), *SongTag.ToString());
	}
	
	return true;
}

void UUniversalBeatSubsystem::StopCurrentSong()
{
	// TODO: Implement in Phase 8 (User Story 6)
	if (!CurrentlyPlayingSong)
	{
		return;
	}
	
	FGameplayTag StoppedSongTag = CurrentlyPlayingSong->GetSongTag();
	
	// Clean up playback state
	CleanupSongPlayback();
	
	CurrentlyPlayingSong = nullptr;
	
	// Broadcast OnSongEnded event
	OnSongEnded.Broadcast(StoppedSongTag);
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("StopCurrentSong: Stopped song with tag '%s'"), *StoppedSongTag.ToString());
	}
}

USongConfiguration* UUniversalBeatSubsystem::GetCurrentSong() const
{
	return CurrentlyPlayingSong;
}

TArray<ULevelSequencePlayer*> UUniversalBeatSubsystem::GetActiveTracks() const
{
	TArray<ULevelSequencePlayer*> Result;
	for (ULevelSequencePlayer* Player : ActiveTrackPlayers)
	{
		if (IsValid(Player))
		{
			Result.Add(Player);
		}
	}
	return Result;
}

bool UUniversalBeatSubsystem::IsPlayingSong() const
{
	return CurrentlyPlayingSong != nullptr && ActiveTrackPlayers.Num() > 0;
}

// Helper function implementations (stubs)

void UUniversalBeatSubsystem::CleanupSongPlayback()
{
	// TODO: Implement in Phase 8 (User Story 6)
	// Stop all active players
	for (ULevelSequencePlayer* Player : ActiveTrackPlayers)
	{
		if (IsValid(Player))
		{
			Player->Stop();
		}
	}
	ActiveTrackPlayers.Empty();
	
	// Clear all timers
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Handle : TrackDelayTimers)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
	}
	TrackDelayTimers.Empty();
	LoopingTracks.Empty();
	CompletedTracks.Empty();
}

void UUniversalBeatSubsystem::StartTrackWithDelay(int32 TrackIndex, float DelaySeconds)
{
	// TODO: Implement in Phase 8 (User Story 6)
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("StartTrackWithDelay: Track %d, Delay %.2fs"), TrackIndex, DelaySeconds);
	}
}

void UUniversalBeatSubsystem::OnTrackDelayComplete(int32 TrackIndex)
{
	// TODO: Implement in Phase 8 (User Story 6)
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("OnTrackDelayComplete: Track %d"), TrackIndex);
	}
}

void UUniversalBeatSubsystem::OnTrackSequenceFinished(int32 TrackIndex)
{
	// TODO: Implement in Phase 8 (User Story 6)
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("OnTrackSequenceFinished: Track %d"), TrackIndex);
	}
}

bool UUniversalBeatSubsystem::CheckSongCompletion() const
{
	// TODO: Implement in Phase 8 (User Story 6)
	return false;
}

// ====================================================================
// Note Chart System Implementation (moved from UNoteChartDirector)
// ====================================================================

bool UUniversalBeatSubsystem::LoadNoteChartFromSequence(ULevelSequence* Sequence)
{
	if (!Sequence)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("LoadNoteChartFromSequence: Invalid sequence"));
		return false;
	}

	// Clear existing data
	CachedNotesSorted.Empty();
	ConsumedNoteTimestamps.Empty();
	CurrentNoteIndex = 0;

	// Get the movie scene
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("LoadNoteChartFromSequence: No movie scene"));
		return false;
	}

	// Cache frame rate for performance
	CachedSequenceFrameRate = MovieScene->GetDisplayRate();

	// Find all note chart tracks
	const TArray<UMovieSceneTrack*>& AllTracks = MovieScene->GetTracks();
	for (UMovieSceneTrack* Track : AllTracks)
	{
		UMovieSceneNoteChartTrack* NoteTrack = Cast<UMovieSceneNoteChartTrack>(Track);
		if (!NoteTrack)
		{
			continue;
		}

		// Get all sections from this track
		const TArray<UMovieSceneSection*>& Sections = NoteTrack->GetAllSections();
		for (UMovieSceneSection* Section : Sections)
		{
			UMovieSceneNoteChartSection* NoteSection = Cast<UMovieSceneNoteChartSection>(Section);
			if (NoteSection)
			{
				// Add all notes from this section
				CachedNotesSorted.Append(NoteSection->Notes);
			}
		}
	}

	// Sort notes by timestamp
	CachedNotesSorted.Sort([](const FNoteInstance& A, const FNoteInstance& B)
	{
		return A.Timestamp < B.Timestamp;
	});

	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("LoadNoteChartFromSequence: Loaded %d notes"),
			CachedNotesSorted.Num());
	}

	return CachedNotesSorted.Num() > 0;
}

void UUniversalBeatSubsystem::ClearNoteChart()
{
	CachedNotesSorted.Empty();
	ConsumedNoteTimestamps.Empty();
	CurrentNoteIndex = 0;

	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("ClearNoteChart: Cleared all notes and reset tracking"));
	}
}

TArray<FNoteInstance> UUniversalBeatSubsystem::GetAllNotes() const
{
	return CachedNotesSorted;
}

int32 UUniversalBeatSubsystem::GetTotalNoteCount() const
{
	return CachedNotesSorted.Num();
}

void UUniversalBeatSubsystem::ResetConsumedNotes()
{
	ConsumedNoteTimestamps.Empty();
	CurrentNoteIndex = 0;

	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("ResetConsumedNotes: Reset for loop/restart"));
	}
}

bool UUniversalBeatSubsystem::PlayNoteChartSequence(ULevelSequence* Sequence)
{
	if (!Sequence)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("PlayNoteChartSequence: Invalid sequence"));
		return false;
	}

	// Validate that the sequence has at least one NoteChartTrack
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("PlayNoteChartSequence: Sequence has no movie scene"));
		return false;
	}

	// Check if sequence contains any NoteChartTrack
	bool bHasNoteChartTrack = false;
	const TArray<UMovieSceneTrack*>& AllTracks = MovieScene->GetTracks();
	for (UMovieSceneTrack* Track : AllTracks)
	{
		if (Cast<UMovieSceneNoteChartTrack>(Track))
		{
			bHasNoteChartTrack = true;
			break;
		}
	}

	if (!bHasNoteChartTrack)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("PlayNoteChartSequence: Sequence '%s' contains no NoteChartTrack"),
			*Sequence->GetName());
		return false;
	}

	// Ensure we have a SongPlayer actor and player

	if (!SongPlayerActor)
	{
		UE_LOG(LogUniversalBeat, Error, TEXT("PlayNoteChartSequence: Failed to create SongPlayer actor"));
		return false;
	}

	// Stop current sequence if any
	if (CurrentNoteChartSequence)
	{
		StopNoteChartSequence();
	}

	// Set the new sequence on the actor
	SongPlayerActor->SetSequence(Sequence);
	CurrentNoteChartSequence = Sequence;

	// Load notes from the sequence
	if (!LoadNoteChartFromSequence(Sequence))
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("PlayNoteChartSequence: Failed to load notes from sequence"));
		CurrentNoteChartSequence = nullptr;
		return false;
	}

	// Start playback - get player from actor
	ULevelSequencePlayer* Player = GetSongPlayer();
	if (Player)
	{
		Player->Play();
	}

	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("PlayNoteChartSequence: Started sequence '%s' with %d notes"),
			*Sequence->GetName(), CachedNotesSorted.Num());
	}

	return true;
}

void UUniversalBeatSubsystem::StopNoteChartSequence()
{
	ULevelSequencePlayer* Player = GetSongPlayer();
	if (Player && Player->IsPlaying())
	{
		Player->Stop();
	}

	CurrentNoteChartSequence = nullptr;
	ClearNoteChart();

	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("StopNoteChartSequence: Stopped sequence and cleared notes"));
	}
}

bool UUniversalBeatSubsystem::IsPlayingNoteChart() const
{
	if (!CurrentNoteChartSequence)
	{
		return false;
	}

	ULevelSequencePlayer* Player = GetSongPlayer();
	return Player && Player->IsPlaying();
}

bool UUniversalBeatSubsystem::GetNextNoteForTag(FGameplayTag NoteTag, float CurrentTime, FNoteInstance& OutNote)
{
	if (!NoteTag.IsValid() || CachedNotesSorted.Num() == 0)
	{
		return false;
	}

	// Binary search for notes around current time
	// Start from current index to optimize sequential playback
	for (int32 i = CurrentNoteIndex; i < CachedNotesSorted.Num(); ++i)
	{
		const FNoteInstance& Note = CachedNotesSorted[i];

		// Skip if already consumed
		if (ConsumedNoteTimestamps.Contains(Note.Timestamp.Value))
		{
			continue;
		}

		// Skip if wrong tag
		if (!Note.NoteData || Note.NoteData->GetNoteTag() != NoteTag)
		{
			continue;
		}

		// Convert note timestamp to seconds
		float NoteTimeSeconds = FrameToSeconds(Note.Timestamp);

		// Calculate timing windows
		float PreTimingSeconds = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(
			Note.NoteData->GetPreTiming(), CurrentBPM);
		float PostTimingSeconds = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(
			Note.NoteData->GetPostTiming(), CurrentBPM);

		float WindowStart = NoteTimeSeconds - PreTimingSeconds;
		float WindowEnd = NoteTimeSeconds + PostTimingSeconds;

		// Check if current time is within timing window
		if (CurrentTime >= WindowStart && CurrentTime <= WindowEnd)
		{
			// Update current index for next search
			CurrentNoteIndex = i;
			OutNote = Note;
			return true;
		}

		// If we've passed the window, this note is missed
		if (CurrentTime > WindowEnd)
		{
			// TODO: Trigger miss event in future phase
			continue;
		}

		// If we haven't reached the window yet, stop searching
		if (CurrentTime < WindowStart)
		{
			break;
		}
	}

	return false;
}

void UUniversalBeatSubsystem::MarkNoteConsumed(const FNoteInstance& Note)
{
	ConsumedNoteTimestamps.Add(Note.Timestamp.Value);
}

bool UUniversalBeatSubsystem::IsNoteConsumed(const FNoteInstance& Note) const
{
	return ConsumedNoteTimestamps.Contains(Note.Timestamp.Value);
}

float UUniversalBeatSubsystem::FrameToSeconds(FFrameNumber Frame) const
{
	return CachedSequenceFrameRate.AsSeconds(Frame);
}

FFrameNumber UUniversalBeatSubsystem::SecondsToFrame(float Seconds) const
{
	return CachedSequenceFrameRate.AsFrameNumber(Seconds);
}

float UUniversalBeatSubsystem::GetCurrentPlaybackTime() const
{
	ULevelSequencePlayer* Player = GetSongPlayer();
	if (!Player || !Player->IsPlaying())
	{
		// Fallback to real-time if no player active or not playing
		return FPlatformTime::Seconds();
	}

	// Get current playback position from the player
	FFrameTime CurrentFrame = Player->GetCurrentTime().Time;
	return FrameToSeconds(CurrentFrame.GetFrame());
}

void UUniversalBeatSubsystem::EnsureSongPlayerActor()
{
	// Check if we already have a valid actor
	if (SongPlayerActor && IsValid(SongPlayerActor))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogUniversalBeat, Error, TEXT("EnsureSongPlayerActor: No valid world"));
		return;
	}

	// Use the static factory method to create both player and actor
	// Note: We pass nullptr for InLevelSequence since we'll set it later via SetSequence()
	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	PlaybackSettings.bAutoPlay = false; // Don't auto-play, we'll control playback manually
	
	ALevelSequenceActor* OutActor = nullptr;
	ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(
		World,
		nullptr, // Sequence will be set later
		PlaybackSettings,
		OutActor
	);

	if (!OutActor)
	{
		UE_LOG(LogUniversalBeat, Error, TEXT("EnsureSongPlayerActor: Failed to create SongPlayer actor"));
		return;
	}

	SongPlayerActor = OutActor;

	// Rename the actor for easy identification in the outliner
	SongPlayerActor->Rename(TEXT("SongPlayer"));

	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("EnsureSongPlayerActor: Created SongPlayer actor and player"));
	}
}

ULevelSequencePlayer* UUniversalBeatSubsystem::GetSongPlayer() const
{


	return SongPlayerActor->GetSequencePlayer();
}

