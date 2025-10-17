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

	// T009: Set default values
	CurrentBPM = 120.0f;
	PendingBPM = 0.0f;
	CurrentBeatTick = 0;
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
	EnsureSongPlayerActor();

	// T009: Start Universal Beat Timer immediately at Sixteenth rate
	RecreateTimerWithNewRate();

	UE_LOG(LogUniversalBeat, Log, TEXT("UniversalBeatSubsystem initialized - BPM: %.1f, Timer started at Sixteenth rate"), CurrentBPM);
}

void UUniversalBeatSubsystem::Deinitialize()
{
	// Clean up SongPlayer actor
	if (SongPlayerActor)
	{
		SongPlayerActor->Destroy();
		SongPlayerActor = nullptr;
	}

	// T010: Pause timers instead of clearing (preserves calibration state)
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		
		// Pause Universal Beat Timer
		if (TimerManager.IsTimerActive(BeatBroadcastTimer))
		{
			TimerManager.PauseTimer(BeatBroadcastTimer);
		}
		
		// Clear calibration timer (transient state)
		TimerManager.ClearTimer(CalibrationTimer);
	}
	
	Super::Deinitialize();
	UE_LOG(LogUniversalBeat, Log, TEXT("UniversalBeatSubsystem deinitialized - Timer paused"));
}

// ====================================================================
// 1. BPM Configuration
// ====================================================================

void UUniversalBeatSubsystem::SetBPM(float NewBPM)
{
	// T028: Validate BPM with clamping and queuing
	
	// Check for invalid values
	if (NewBPM <= 0.0f || !FMath::IsFinite(NewBPM))
	{
		UE_LOG(LogUniversalBeat, Error, TEXT("Invalid BPM value %.2f rejected (<=0, NaN, or Inf), resetting to default 120"), NewBPM);
		CurrentBPM = 120.0f;
		PendingBPM = 0.0f;
		RecreateTimerWithNewRate();
		return;
	}
	
	// Check for extreme values and clamp
	if (NewBPM < 20.0f || NewBPM > 400.0f)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("BPM %.2f is outside practical range [20-400], clamping"), NewBPM);
		NewBPM = FMath::Clamp(NewBPM, 20.0f, 400.0f);
	}
	
	// Only queue if BPM actually changed
	if (NewBPM != CurrentBPM)
	{
		PendingBPM = NewBPM;
		
		if (bDebugLoggingEnabled)
		{
			UE_LOG(LogUniversalBeat, Log, TEXT("BPM change queued: %.2f (will apply at next whole beat)"), NewBPM);
		}
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
	// T030: Validate and set calibration offset with timer recreation
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
	
	// Recreate timer with new offset as initial delay
	// Note: This causes a brief timing discontinuity, acceptable during calibration
	RecreateTimerWithNewRate();
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("Calibration offset set to %.2fms - Timer recreated with adjusted initial delay"), CalibrationOffsetMs);
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
	// T024: Enable beat broadcasting with subdivision filtering
	// Timer always runs at Sixteenth rate; this just controls which timer ticks fire OnBeat events
	bBeatBroadcastingEnabled = true;
	CurrentSubdivision = Subdivision;
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("Beat broadcasting enabled - Subdivision:%d (filtering on %d-tick boundaries)"), 
			(int32)Subdivision, GetTicksForSubdivision(Subdivision));
	}
}

void UUniversalBeatSubsystem::DisableBeatBroadcasting()
{
	// T025: Disable beat broadcasting
	// Timer continues running for timing checks; just stop firing OnBeat events
	bBeatBroadcastingEnabled = false;
	CurrentSubdivision = EBeatSubdivision::None;
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("Beat broadcasting disabled - Timer continues for timing checks"));
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
	// Calculate beat number from current tick counter
	// Since timer ticks at InternalSubdivision default to Sixteenth rate (16 ticks per beat)
	return CurrentBeatTick / InternalSubdivision;
}

float UUniversalBeatSubsystem::GetCurrentBeatPhase() const
{
	// T025: Return calculated beat phase
	return CalculateBeatPhase();
}

// ====================================================================
// Internal Helper Functions
// ====================================================================

float UUniversalBeatSubsystem::GetTimerRate() const
{
	// T006: Return timer rate for Sixteenth subdivision (1/16th notes)
	return (60.0f / GetBPM()) / InternalSubdivision + (GetCalibrationOffset() / 1000.0f);
}

int32 UUniversalBeatSubsystem::GetTicksForSubdivision(EBeatSubdivision Subdivision) const
{
	// T007: Map subdivision enum to tick divisor
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

void UUniversalBeatSubsystem::RecreateTimerWithNewRate()
{
	// T008: Centralized timer recreation logic
	if (!GetWorld())
	{
		return;
	}

	UWorld* World = GetWorld();
	FTimerManager& TimerManager = World->GetTimerManager();

	// Clear existing timer
	if (TimerManager.IsTimerActive(BeatBroadcastTimer))
	{
		TimerManager.ClearTimer(BeatBroadcastTimer);
	}

	// Calculate new timer rate (Sixteenth note intervals)
	float TimerRate = GetTimerRate();

	// Create new timer with appropriate time mode
	if (bRespectTimeDilation)
	{
		TimerManager.SetTimer(
			BeatBroadcastTimer,
			this,
			&UUniversalBeatSubsystem::BroadcastBeatEvent,
			TimerRate,
			true,  // Loop
			0
		);
	}
	else
	{
		TimerManager.SetTimerForNextTick([this, TimerRate, World]()
		{
			World->GetTimerManager().SetTimer(
				BeatBroadcastTimer,
				this,
				&UUniversalBeatSubsystem::BroadcastBeatEvent,
				TimerRate,
				true,  // Loop
				0.0f
			);
		});
	}

	// Reset tick counter when timer is recreated
	CurrentBeatTick = 0;

	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("Timer recreated: Rate=%.6f, InitialDelay=%.6f, BPM=%.2f"),
			TimerRate, 0.0f, CurrentBPM);
	}
}

float UUniversalBeatSubsystem::CalculateBeatPhase() const
{
	// T012: Calculate BeatPhase from Universal Beat Timer's remaining time
	// IMPORTANT: Must be called from game thread only
	
	if (!GetWorld())
	{
		return 0.0f;
	}
	
	UWorld* World = GetWorld();
	FTimerManager& TimerManager = World->GetTimerManager();
	
	// Check if timer is active
	if (!TimerManager.IsTimerActive(BeatBroadcastTimer))
	{
		// Timer not started yet, return default
		return -1.0f;
	}
	
	// Get remaining time until next timer fire
	float TimerRemaining = TimerManager.GetTimerRemaining(BeatBroadcastTimer);
	
	// Get current timer rate (Sixteenth subdivision)
	float TimerRate = TimerManager.GetTimerRate(BeatBroadcastTimer);
	

	// Remap TimerRemaining from [0, TimerRate] to [-1.0, +1.0]
	// Formula: BeatPhase = ((TimerRemaining / TimerRate) * 2.0) - 1.0
	// We should check for TimerRate > 0 to avoid division by zero, but it should never be zero in practice
	// When TimerRemaining = 0 (just fired): BeatPhase = -1.0 (beat edge)
	// When TimerRemaining = TimerRate/2 (halfway): BeatPhase = 0.0 (beat peak)
	// When TimerRemaining = TimerRate (just started): BeatPhase = +1.0 (beat edge)
	float BeatPhase = ((TimerRemaining / TimerRate) * 2.0f) - 1.0f;
	
	// Safety clamp to ensure range
	BeatPhase = FMath::Clamp(BeatPhase, -1.0f, 1.0f);
	
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
	// T013: Internal timing check implementation with FPS warning
	
	// Static flag for one-time low FPS warning
	static bool bLowFPSWarningLogged = false;
	
	// Handle null/empty label (FR-011a)
	FName SafeLabelName = (LabelName.IsNone() || LabelName == NAME_None) 
		? FName("Default") 
		: LabelName;
	
	// Check frame rate and log warning once if below 30 FPS
	if (!bLowFPSWarningLogged && GetWorld())
	{
		float DeltaSeconds = GetWorld()->GetDeltaSeconds();
		if (DeltaSeconds > 0.033f)  // 30 FPS threshold
		{
			UE_LOG(LogUniversalBeat, Warning, TEXT("Frame rate below 30 FPS detected (%.1f ms/frame). Timing accuracy may degrade below specification."),
				DeltaSeconds * 1000.0f);
			bLowFPSWarningLogged = true;
		}
	}
	
	// Get current beat phase (-1.0 to +1.0)
	float BeatPhase = CalculateBeatPhase();
	
	// Apply abs() to get curve input (0.0 to 1.0)
	float CurveInput = FMath::Abs(BeatPhase);
	
	// Evaluate timing value via curve
	float TimingValue = EvaluateTimingCurve(CurveInput);
	
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
	
	// T026: Debug logging for timing check correlation
	if (bDebugLoggingEnabled)
	{
		float TimerRemaining = GetWorld() ? GetWorld()->GetTimerManager().GetTimerRemaining(BeatBroadcastTimer) : 0.0f;
		
		UE_LOG(LogUniversalBeat, Verbose, TEXT("Timing Check [Label:%s] - Beat #%d (TimerRemaining: %.6f, BeatPhase: %.3f, TimingValue: %.3f)"), 
			*SafeLabelName.ToString(), BeatNumber, TimerRemaining, BeatPhase, TimingValue);
	
	}
	
	return TimingValue;
}

void UUniversalBeatSubsystem::BroadcastBeatEvent()
{
	// T014: Beat broadcasting callback with BPM queuing and subdivision filtering
	
	// Increment tick counter
	CurrentBeatTick++;
	
	// Check for pending BPM change at whole beat boundary (every InternalSubdivision = 16 ticks)
	if (PendingBPM > 0.0f && (CurrentBeatTick % InternalSubdivision == 0))
	{
		if (bDebugLoggingEnabled)
		{
			UE_LOG(LogUniversalBeat, Log, TEXT("Applying queued BPM change: %.2f -> %.2f at tick %d"),
				CurrentBPM, PendingBPM, CurrentBeatTick);
		}
		
		// Apply pending BPM
		CurrentBPM = PendingBPM;
		PendingBPM = 0.0f;
		OnBPMChanged.Broadcast(CurrentBPM);
		// Recreate timer with new rate
		RecreateTimerWithNewRate();
		
		// Return early - timer recreation will restart callbacks
		return;
	}
	
	// Check if broadcasting is enabled
	if (!bBeatBroadcastingEnabled)
	{
		return;
	}
	
	// Get ticks per broadcast for current subdivision
	int32 TicksPerBroadcast = GetTicksForSubdivision(CurrentSubdivision);
	
	// Check if we should broadcast on this tick
	if (TicksPerBroadcast > 0 && (CurrentBeatTick % TicksPerBroadcast == 0))
	{
		// Calculate subdivision index relative to CurrentSubdivision for event data. It should count from 0 to 7 if CurrentSubdivision is 8th beat and so on
		// Use modulo to cycle the index within each beat (e.g., 0-7 for eighth notes, 0-3 for quarter notes)
		int32 SubdivisionsPerBeat = InternalSubdivision / TicksPerBroadcast;
		int32 SubdivisionIndex = (CurrentBeatTick / TicksPerBroadcast) % SubdivisionsPerBeat;
		// Get current beat phase and number
		float CurrentPhase = GetCurrentBeatPhase();
		int32 CurrentBeatNum = GetCurrentBeatNumber();
		
		// Create event data
		FBeatEventData EventData;
		EventData.BeatNumber = CurrentBeatNum;
		EventData.SubdivisionIndex = SubdivisionIndex;
		EventData.SubdivisionType = CurrentSubdivision;
		EventData.EventTimestamp = FPlatformTime::Seconds();
		
		// Broadcast event
		OnBeat.Broadcast(EventData);
		
		// T026: Debug logging for synchronization validation
		if (bDebugLoggingEnabled)
		{
			float TimerRemaining = GetWorld() ? GetWorld()->GetTimerManager().GetTimerRemaining(BeatBroadcastTimer) : 0.0f;
			UE_LOG(LogUniversalBeat, Verbose, TEXT("Beat #%d fired (TimerRemaining: %.6f, BeatPhase: %.3f, Tick: %d, Subdivision: %d)"), 
				CurrentBeatNum, TimerRemaining, CurrentPhase, CurrentBeatTick, (int32)CurrentSubdivision);
		}
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

int UUniversalBeatSubsystem::GetSubdivisionMultiplier(EBeatSubdivision Subdivision) const
{
	// T034: Get subdivision multiplier for timer interval calculation
	switch (Subdivision)
	{
		case EBeatSubdivision::None:    return 1;
		case EBeatSubdivision::Half:    return 2;
		case EBeatSubdivision::Quarter: return 4;
		case EBeatSubdivision::Eighth:  return 8;
		case EBeatSubdivision::Sixteenth:  return 16;
		default: return 1;
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
// Note Chart System Implementation 
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


bool UUniversalBeatSubsystem::PlaySongByTag(FGameplayTag SongTag, bool bQueue)
{
	TObjectPtr<USongConfiguration>* FoundSong = RegisteredSongs.Find(SongTag);
	if (!FoundSong || !*FoundSong)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("PlaySongByTag: Song with tag '%s' not found"), *SongTag.ToString());
		return false;
	}
	
	// If not queuing, clear existing queue and stop current song
	if (!bQueue)
	{
		// Empty the queue
		QueuedSongs.Empty();
		
		// Stop current song if any
		if (CurrentlyPlayingSong)
		{
			StopCurrentSong();
		}
	}
	
	// Enqueue the song
	QueuedSongs.Enqueue(*FoundSong);
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("PlaySongByTag: %s song '%s' with tag '%s'"), 
			bQueue ? TEXT("Queued") : TEXT("Playing"), 
			*(*FoundSong)->GetSongLabel(), *SongTag.ToString());
	}
	
	// If nothing is currently playing, start the queue
	if (!CurrentlyPlayingSong)
	{
		PlaySong();
	}
	
	return true;
}

bool UUniversalBeatSubsystem::PlaySongByAsset(TSoftObjectPtr<USongConfiguration> SongAsset, bool bQueue)
{
	// Load the asset synchronously if it's a soft reference
	USongConfiguration* LoadedSong = SongAsset.LoadSynchronous();
	
	if (!LoadedSong)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("PlaySongByAsset: Failed to load song asset"));
		return false;
	}
	
	if (!LoadedSong->GetSongTag().IsValid())
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("PlaySongByAsset: Song '%s' has invalid tag"), 
			*LoadedSong->GetSongLabel());
		return false;
	}
	
	// Auto-register the song if it's not already registered
	if (!RegisteredSongs.Contains(LoadedSong->GetSongTag()))
	{
		if (!RegisterSong(LoadedSong))
		{
			UE_LOG(LogUniversalBeat, Warning, TEXT("PlaySongByAsset: Failed to auto-register song '%s'"), 
				*LoadedSong->GetSongLabel());
			return false;
		}
		
		if (bDebugLoggingEnabled)
		{
			UE_LOG(LogUniversalBeat, Log, TEXT("PlaySongByAsset: Auto-registered song '%s' with tag '%s'"), 
				*LoadedSong->GetSongLabel(), *LoadedSong->GetSongTag().ToString());
		}
	}
	
	// Use PlaySongByTag to handle the actual playback
	return PlaySongByTag(LoadedSong->GetSongTag(), bQueue);
}

void UUniversalBeatSubsystem::StopCurrentSong()
{
	// TODO: Implement in Phase 8 (User Story 6)
	if (!CurrentlyPlayingSong)
	{
		return;
	}
	
	// Store reference to song before cleanup
	TObjectPtr<USongConfiguration> StoppedSong = CurrentlyPlayingSong;
	
	// Clean up playback state
	CleanupSongPlayback();
	
	CurrentlyPlayingSong = nullptr;
	
	// Broadcast OnSongEnded event with the song configuration
	OnSongEnded.Broadcast();
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("StopCurrentSong: Stopped song '%s' with tag '%s'"), 
			*StoppedSong->GetSongLabel(), *StoppedSong->GetSongTag().ToString());
	}
}

void UUniversalBeatSubsystem::PlaySong()
{
	// Dequeue next song from queue
	TObjectPtr<USongConfiguration> NextSong;
	if (!QueuedSongs.Dequeue(NextSong) || !NextSong)
	{
		if (bDebugLoggingEnabled)
		{
			UE_LOG(LogUniversalBeat, Log, TEXT("PlaySong: No more songs in queue"));
		}
		return;
	}
	
	CurrentlyPlayingSong = NextSong;
	
	// Broadcast OnSongStarted event with the song configuration
	OnSongStarted.Broadcast();
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("PlaySong: Started song '%s' with tag '%s'"), 
			*CurrentlyPlayingSong->GetSongLabel(), *CurrentlyPlayingSong->GetSongTag().ToString());
	}
	
	// Validate tracks exist
	const TArray<FNoteTrackEntry>& Tracks = CurrentlyPlayingSong->Tracks;
	if (Tracks.Num() == 0)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("PlaySong: Song '%s' has no tracks configured"), 
			*CurrentlyPlayingSong->GetSongTag().ToString());
		CurrentlyPlayingSong = nullptr;
		// Try next song in queue
		PlaySong();
		return;
	}
	
	// Clear track queue and enqueue all tracks from this song
	QueuedTracks.Empty();
	
	for (const FNoteTrackEntry& Track : Tracks)
	{
		QueuedTracks.Enqueue(Track);
	}
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("PlaySong: Enqueued %d tracks for song '%s'"), 
			Tracks.Num(), *CurrentlyPlayingSong->GetSongTag().ToString());
	}
	
	// Start playing tracks
	PlayTrack();
}

USongConfiguration* UUniversalBeatSubsystem::GetCurrentSong() const
{
	return CurrentlyPlayingSong;
}

TArray<ULevelSequencePlayer*> UUniversalBeatSubsystem::GetActiveTracks() const
{
	TArray<ULevelSequencePlayer*> Result;
	// With sequential playback, return the single SongPlayer if a song is playing
	if (CurrentlyPlayingSong)
	{
		ULevelSequencePlayer* Player = GetSongPlayer();
		if (Player && Player->IsPlaying())
		{
			Result.Add(Player);
		}
	}
	return Result;
}

bool UUniversalBeatSubsystem::IsPlayingSong() const
{
	if (!CurrentlyPlayingSong)
	{
		return false;
	}
	
	ULevelSequencePlayer* Player = GetSongPlayer();
	return Player && Player->IsPlaying();
}

// Helper function implementations (stubs)

void UUniversalBeatSubsystem::CleanupSongPlayback()
{
	// Stop SongPlayer if playing
	ULevelSequencePlayer* Player = GetSongPlayer();
	if (Player && Player->IsPlaying())
	{
		Player->Stop();
	}
	
	// Clear delay timer
	if (UWorld* World = GetWorld())
	{
		if (TrackDelayTimer.IsValid())
		{
			World->GetTimerManager().ClearTimer(TrackDelayTimer);
			TrackDelayTimer.Invalidate();
		}
	}
	
	// Clear track queue
	QueuedTracks.Empty();

	// Reset current track info
	CurrentTrackInfo = FNoteTrackEntry();
	
	// Clear current song
	CurrentlyPlayingSong = nullptr;
	
	// Clear note chart
	ClearNoteChart();
}

void UUniversalBeatSubsystem::PlayTrack()
{
	// Dequeue next track
	if (!QueuedTracks.Dequeue(CurrentTrackInfo))
	{
		// No more tracks in queue - check song completion
		if (bDebugLoggingEnabled)
		{
			UE_LOG(LogUniversalBeat, Log, TEXT("PlayTrack: No more tracks in queue, checking song completion"));
		}
		CheckSongCompletion();
		return;
	}
	
	// Validate track sequence
	ULevelSequence* TrackSequence = CurrentTrackInfo.TrackSequence.LoadSynchronous();
	if (!TrackSequence)
	{
		UE_LOG(LogUniversalBeat, Error, TEXT("PlayTrack: Failed to load track sequence"));
		// Try next track
		PlayTrack();
		return;
	}
	
	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("PlayTrack: Starting track with delay %.2fs, loop=%d"), 
			CurrentTrackInfo.DelayOffset, 
			CurrentTrackInfo.LoopCount);
	}
	
	// Apply delay if needed
	if (CurrentTrackInfo.DelayOffset > 0.0f)
	{
		StartTrackWithDelay(CurrentTrackInfo.DelayOffset);
	}
	else
	{
		OnTrackDelayComplete();
	}
}

void UUniversalBeatSubsystem::StartTrackWithDelay(float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogUniversalBeat, Error, TEXT("StartTrackWithDelay: No valid world"));
		return;
	}

	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("StartTrackWithDelay: Delay %.2fs"), DelaySeconds);
	}

	// Clear previous timer if any
	if (TrackDelayTimer.IsValid())
	{
		World->GetTimerManager().ClearTimer(TrackDelayTimer);
	}

	// Create timer for delayed start
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &UUniversalBeatSubsystem::OnTrackDelayComplete);
	
	World->GetTimerManager().SetTimer(TrackDelayTimer, TimerDelegate, DelaySeconds, false);
}

void UUniversalBeatSubsystem::OnTrackDelayComplete()
{
	if (!CurrentlyPlayingSong)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("OnTrackDelayComplete: No song playing"));
		return;
	}

	// Load the track sequence asset from CurrentTrackInfo
	ULevelSequence* TrackSequence = CurrentTrackInfo.TrackSequence.LoadSynchronous();
	if (!TrackSequence)
	{
		UE_LOG(LogUniversalBeat, Error, TEXT("OnTrackDelayComplete: Failed to load track sequence"));
		// Try next track
		PlayTrack();
		return;
	}

	// Ensure SongPlayerActor exists
	EnsureSongPlayerActor();
	if (!SongPlayerActor)
	{
		UE_LOG(LogUniversalBeat, Error, TEXT("OnTrackDelayComplete: No valid SongPlayerActor"));
		return;
	}

	// Sequential playback using single SongPlayerActor
	// Set the sequence for this track
	SongPlayerActor->SetSequence(TrackSequence);
	
	ULevelSequencePlayer* Player = GetSongPlayer();
	if (!Player)
	{
		UE_LOG(LogUniversalBeat, Error, TEXT("OnTrackDelayComplete: Failed to get player"));
		return;
	}

	// Load note chart from the sequence for validation
	LoadNoteChartFromSequence(TrackSequence);

	// Configure loop settings from CurrentTrackInfo
	FMovieSceneSequencePlaybackSettings PlaybackSettings = SongPlayerActor->PlaybackSettings;
	PlaybackSettings.LoopCount.Value = CurrentTrackInfo.LoopCount;
	Player->SetPlaybackSettings(PlaybackSettings);

	// Clear any previous finish bindings and bind our callback
	Player->OnFinished.Clear();
	Player->OnFinished.AddDynamic(this, &UUniversalBeatSubsystem::OnSongPlayerFinished);

	// Start playback
	Player->Play();

	// Find track index for broadcasting (search in original song tracks)
	int32 TrackIndex = 0;
	if (CurrentlyPlayingSong)
	{
		for (int32 i = 0; i < CurrentlyPlayingSong->Tracks.Num(); ++i)
		{
			if (CurrentlyPlayingSong->Tracks[i].TrackSequence == CurrentTrackInfo.TrackSequence)
			{
				TrackIndex = i;
				break;
			}
		}
		
		// Broadcast track started event
		OnTrackStarted.Broadcast(TrackIndex);
	}

	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("OnTrackDelayComplete: Track %d started playback"), TrackIndex);
	}
}

void UUniversalBeatSubsystem::OnTrackSequenceFinished()
{
	if (!CurrentlyPlayingSong)
	{
		UE_LOG(LogUniversalBeat, Warning, TEXT("OnTrackSequenceFinished: No song playing"));
		return;
	}

	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("OnTrackSequenceFinished: Track finished"));
	}

	// Find track index for broadcasting
	int32 TrackIndex = 0;
	if (CurrentlyPlayingSong)
	{
		for (int32 i = 0; i < CurrentlyPlayingSong->Tracks.Num(); ++i)
		{
			if (CurrentlyPlayingSong->Tracks[i].TrackSequence == CurrentTrackInfo.TrackSequence)
			{
				TrackIndex = i;
				break;
			}
		}
		
		// Broadcast track ended event
		OnTrackEnded.Broadcast(TrackIndex);
	}

	if (bDebugLoggingEnabled)
	{
		UE_LOG(LogUniversalBeat, Log, TEXT("OnTrackSequenceFinished: Track %d completed, playing next"), TrackIndex);
	}

	// Play next track from queue (or complete song if queue empty)
	// Note: If track was configured to loop, the player will handle it automatically
	// and OnFinished won't be called until all loops are complete
	PlayTrack();
}

void UUniversalBeatSubsystem::OnSongPlayerFinished()
{
	// Delegate to the track-specific handler
	OnTrackSequenceFinished();
}

bool UUniversalBeatSubsystem::CheckSongCompletion() const
{
	if (!CurrentlyPlayingSong)
	{
		return false;
	}

	// Song is complete when track queue is empty and no track is playing
	if (QueuedTracks.IsEmpty())
	{
		if (bDebugLoggingEnabled)
		{
			UE_LOG(LogUniversalBeat, Log, TEXT("CheckSongCompletion: Song '%s' completed"), 
				*CurrentlyPlayingSong->GetSongLabel());
		}

		// Store reference to completed song before cleanup
		TObjectPtr<USongConfiguration> CompletedSong = CurrentlyPlayingSong;
		
		// Broadcast song ended with the song configuration
		OnSongEnded.Broadcast();
		
		// Clean up current song
		const_cast<UUniversalBeatSubsystem*>(this)->CleanupSongPlayback();
		
		// Try to play next song in queue
		const_cast<UUniversalBeatSubsystem*>(this)->PlaySong();
		
		return true;
	}

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
				// Add all notes from this section's runtime notes
				CachedNotesSorted.Append(NoteSection->RuntimeNotes);
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
	EnsureSongPlayerActor();
	
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
	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	PlaybackSettings.bAutoPlay = false; // Don't auto-play, we'll control playback manually
	
	// ALevelSequenceActor* OutActor = nullptr;
	// ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(
	// 	this,
	// 	nullptr, // Sequence will be set later
	// 	PlaybackSettings,
	// 	OutActor
	// );
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.bAllowDuringConstructionScript = true;
	SpawnParams.Name = FName(TEXT("SongPlayerActor"));
	// Defer construction for autoplay so that BeginPlay() is called
	SpawnParams.bDeferConstruction = true;

	ALevelSequenceActor* OutActor = World->SpawnActor<ALevelSequenceActor>(SpawnParams);
	OutActor->SetActorLabel(TEXT("SongPlayerActor"));
	ULevelSequencePlayer* Player = OutActor->GetSequencePlayer();
	OutActor->PlaybackSettings = PlaybackSettings;
	if (!OutActor || !Player)
	{
		UE_LOG(LogUniversalBeat, Error, TEXT("EnsureSongPlayerActor: Failed to create SongPlayer actor (OutActor=%s, Player=%s)"),
			OutActor ? TEXT("Valid") : TEXT("NULL"),
			Player ? TEXT("Valid") : TEXT("NULL"));
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
	if (!SongPlayerActor || !IsValid(SongPlayerActor))
	{
		return nullptr;
	}

	return SongPlayerActor->GetSequencePlayer();
}

