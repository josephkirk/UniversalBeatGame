// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UniversalBeatTypes.h"
#include "Curves/CurveFloat.h"
#include "UniversalBeatSubsystem.generated.h"

// Event dispatcher delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBeatInputCheck, FName, LabelName, FGameplayTag, InputTag, float, TimingValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeat, FBeatEventData, BeatData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCalibrationComplete, float, CalculatedOffsetMs, bool, bSuccess);

/**
 * UniversalBeat Subsystem - Game thread only
 * 
 * Genre-agnostic rhythm game system providing beat tracking, timing checks,
 * and event-driven integration for Blueprint-based rhythm mechanics.
 * 
 * IMPORTANT: This subsystem must only be accessed from the game thread.
 * Do not call subsystem methods from async tasks or other threads.
 * 
 * Performance Requirements:
 * - 30+ FPS: Full timing accuracy maintained
 * - <30 FPS: Timing accuracy may degrade below specification
 * - 60+ FPS: Optimal performance target
 */
UCLASS()
class UNIVERSALBEAT_API UUniversalBeatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** USubsystem implementation */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ====================================================================
	// 1. BPM Configuration
	// ====================================================================

	/**
	 * Set the beats per minute for rhythm tracking.
	 * Resets start time to prevent phase discontinuity.
	 * 
	 * @param NewBPM Desired tempo (must be > 0, practical range 20-400)
	 * 
	 * If invalid (<=0), logs error and resets to default 120 BPM.
	 * Logs warning if outside typical range (60-240).
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Configuration", meta = (Tooltip = "Set the beats per minute. Invalid values reset to 120 BPM."))
	void SetBPM(float NewBPM);

	/**
	 * Get the current beats per minute.
	 * 
	 * @return Current BPM value
	 */
	UFUNCTION(BlueprintPure, Category = "UniversalBeat|Configuration", meta = (Tooltip = "Get current BPM value."))
	float GetBPM() const;

	/**
	 * Get the duration of one beat in seconds at current BPM.
	 * 
	 * @return Seconds per beat (60.0 / BPM)
	 */
	UFUNCTION(BlueprintPure, Category = "UniversalBeat|Configuration", meta = (Tooltip = "Get seconds per beat at current BPM."))
	float GetSecondsPerBeat() const;

	/**
	 * Set whether beat timing respects game time dilation (slow-motion, pause).
	 * 
	 * @param bRespect True = beats slow with time dilation, False = real-time beats
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Configuration", meta = (Tooltip = "Choose whether beats follow time dilation or real-time."))
	void SetRespectTimeDilation(bool bRespect);

	/**
	 * Get whether beat timing respects game time dilation.
	 * 
	 * @return True if respecting time dilation, False if real-time
	 */
	UFUNCTION(BlueprintPure, Category = "UniversalBeat|Configuration", meta = (Tooltip = "Check if beats follow time dilation."))
	bool GetRespectTimeDilation() const;

	// ====================================================================
	// 2. Timing Checks
	// ====================================================================

	/**
	 * Check beat timing accuracy using a label identifier.
	 * Returns 0.0-1.0 value based on how close input was to beat.
	 * Broadcasts OnBeatInputCheck event.
	 * 
	 * @param LabelName String identifier for this input (e.g., "Jump"). Empty/null treated as "Default".
	 * @return Timing accuracy: 0.0 = mid-beat (worst), 1.0 = on-beat (perfect)
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Timing", meta = (Tooltip = "Check timing accuracy with label identifier. Returns 0.0-1.0."))
	float CheckBeatTimingByLabel(FName LabelName);

	/**
	 * Check beat timing accuracy using a gameplay tag identifier.
	 * Returns 0.0-1.0 value based on how close input was to beat.
	 * Broadcasts OnBeatInputCheck event.
	 * 
	 * @param InputTag Gameplay tag identifier for this input
	 * @return Timing accuracy: 0.0 = mid-beat (worst), 1.0 = on-beat (perfect)
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Timing", meta = (Tooltip = "Check timing accuracy with gameplay tag. Returns 0.0-1.0."))
	float CheckBeatTimingByTag(FGameplayTag InputTag);

	/**
	 * Set the curve asset used to calculate timing accuracy from beat phase.
	 * Curve input [0.0-1.0] = position in beat cycle, output [0.0-1.0] = accuracy.
	 * 
	 * @param NewCurve Curve asset (null will use linear fallback)
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Timing", meta = (Tooltip = "Set timing curve asset. Null uses linear fallback."))
	void SetTimingCurve(UCurveFloat* NewCurve);

	/**
	 * Get the current timing curve asset.
	 * 
	 * @return Current curve asset (may be null)
	 */
	UFUNCTION(BlueprintPure, Category = "UniversalBeat|Timing", meta = (Tooltip = "Get current timing curve asset."))
	UCurveFloat* GetTimingCurve() const;

	// ====================================================================
	// 3. Calibration
	// ====================================================================

	/**
	 * Set player calibration offset to compensate for hardware latency.
	 * Offset is applied to all timing calculations.
	 * 
	 * @param OffsetMs Timing offset in milliseconds (range: -200 to +200, clamped if out of range)
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Calibration", meta = (Tooltip = "Set calibration offset in milliseconds (-200 to +200)."))
	void SetCalibrationOffset(float OffsetMs);

	/**
	 * Get the current calibration offset.
	 * 
	 * @return Calibration offset in milliseconds
	 */
	UFUNCTION(BlueprintPure, Category = "UniversalBeat|Calibration", meta = (Tooltip = "Get current calibration offset in milliseconds."))
	float GetCalibrationOffset() const;

	/**
	 * Run an automated calibration sequence to measure player timing offset.
	 * System presents prompts at beat intervals, player responds, average offset calculated.
	 * Broadcasts OnCalibrationComplete when finished.
	 * 
	 * @param NumPrompts Number of prompts to present (5-20 recommended)
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Calibration", meta = (Tooltip = "Run automated calibration sequence. Broadcasts OnCalibrationComplete when done."))
	void RunCalibrationSequence(int32 NumPrompts = 10);

	// ====================================================================
	// 4. Beat Broadcasting
	// ====================================================================

	/**
	 * Enable beat event broadcasting with optional subdivisions.
	 * Fires OnBeat event at beat intervals (or subdivisions).
	 * 
	 * @param Subdivision Subdivision level (None = full beats only, Quarter = 4x per beat, etc.)
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Broadcasting", meta = (Tooltip = "Enable beat event broadcasting with subdivisions."))
	void EnableBeatBroadcasting(EBeatSubdivision Subdivision = EBeatSubdivision::None);

	/**
	 * Disable beat event broadcasting.
	 * Stops firing OnBeat events.
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Broadcasting", meta = (Tooltip = "Disable beat event broadcasting."))
	void DisableBeatBroadcasting();

	/**
	 * Check if beat broadcasting is currently enabled.
	 * 
	 * @return True if broadcasting enabled, False otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "UniversalBeat|Broadcasting", meta = (Tooltip = "Check if beat broadcasting is enabled."))
	bool IsBeatBroadcastingEnabled() const;

	// ====================================================================
	// 5. Debug & Utility
	// ====================================================================

	/**
	 * Enable detailed logging for development and troubleshooting.
	 * Logs timing checks, BPM changes, beat events, calibration, etc.
	 * 
	 * @param bEnabled True to enable debug logging
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Debug", meta = (Tooltip = "Enable/disable debug logging."))
	void EnableDebugLogging(bool bEnabled);

	/**
	 * Check if debug logging is currently enabled.
	 * 
	 * @return True if debug logging enabled
	 */
	UFUNCTION(BlueprintPure, Category = "UniversalBeat|Debug", meta = (Tooltip = "Check if debug logging is enabled."))
	bool IsDebugLoggingEnabled() const;

	/**
	 * Get the current beat number since system started.
	 * 
	 * @return Beat counter (0, 1, 2, 3, ...)
	 */
	UFUNCTION(BlueprintPure, Category = "UniversalBeat|Utility", meta = (Tooltip = "Get current beat number since start."))
	int32 GetCurrentBeatNumber() const;

	/**
	 * Get the current position within the beat cycle.
	 * 
	 * @return Beat phase: 0.0 = start of beat, 0.5 = on beat, 1.0 = end of beat
	 */
	UFUNCTION(BlueprintPure, Category = "UniversalBeat|Utility", meta = (Tooltip = "Get current beat phase (0.0-1.0 within beat)."))
	float GetCurrentBeatPhase() const;

	// ====================================================================
	// 6. Event Dispatchers
	// ====================================================================

	/**
	 * Event fired when a beat timing check occurs.
	 * Receives LabelName, InputTag, and TimingValue.
	 * 
	 * Performance: Standard UE5 multicast delegate. Scales to hundreds of listeners.
	 * For best performance, avoid expensive operations in event handlers.
	 * Use Unreal Insights to profile event handler performance if needed.
	 */
	UPROPERTY(BlueprintAssignable, Category = "UniversalBeat|Events")
	FOnBeatInputCheck OnBeatInputCheck;

	/**
	 * Event fired when a beat or beat subdivision occurs (if broadcasting enabled).
	 * Receives FBeatEventData with beat number, subdivision info, and timestamp.
	 * 
	 * Performance: Standard UE5 multicast delegate. Scales to hundreds of listeners.
	 * Use Unreal Insights to profile event handler performance if needed.
	 */
	UPROPERTY(BlueprintAssignable, Category = "UniversalBeat|Events")
	FOnBeat OnBeat;

	/**
	 * Event fired when calibration sequence completes.
	 * Receives calculated offset in milliseconds and success status.
	 */
	UPROPERTY(BlueprintAssignable, Category = "UniversalBeat|Events")
	FOnCalibrationComplete OnCalibrationComplete;

private:
	// ====================================================================
	// Internal State
	// ====================================================================

	/** Current beats per minute */
	float CurrentBPM = 120.0f;

	/** Whether beat timing follows time dilation */
	bool bRespectTimeDilation = false;

	/** Absolute time when beat tracking started (from FPlatformTime::Seconds) */
	double StartTime = 0.0;

	/** Timing curve asset for accuracy calculation */
	UPROPERTY()
	TObjectPtr<UCurveFloat> TimingCurve = nullptr;

	/** Player calibration offset in milliseconds */
	float CalibrationOffsetMs = 0.0f;

	/** Whether beat broadcasting is enabled */
	bool bBeatBroadcastingEnabled = false;

	/** Current subdivision level for broadcasting */
	EBeatSubdivision CurrentSubdivision = EBeatSubdivision::None;

	/** Whether debug logging is enabled */
	bool bDebugLoggingEnabled = false;

	/** Timer handle for drift compensation (1 second intervals) */
	FTimerHandle DriftCompensationTimer;

	/** Timer handle for beat broadcasting */
	FTimerHandle BeatBroadcastTimer;

	/** Timer handle for calibration sequence */
	FTimerHandle CalibrationTimer;

	/** Calibration sequence state */
	int32 CalibrationPromptsRemaining = 0;
	int32 CalibrationTotalPrompts = 0;
	TArray<float> CalibrationOffsets;

	/** Flag to prevent repeated curve fallback warnings */
	bool bCurveFallbackWarningLogged = false;

	/** Cached beat phase when entering pause (for pause handling) */
	mutable float CachedPausePhase = 0.0f;
	
	/** Cached time when entering pause */
	mutable double CachedPauseTime = 0.0;
	
	/** Whether we are currently in a paused state */
	mutable bool bIsPaused = false;

	// ====================================================================
	// Internal Helper Functions
	// ====================================================================

	/** Calculate current beat phase (0.0-1.0 within beat) */
	float CalculateBeatPhase() const;

	/** Evaluate timing curve with validation and fallback */
	float EvaluateTimingCurve(float BeatPhase);

	/** Internal timing check implementation shared by label and tag versions */
	float CheckBeatTimingInternal(FName LabelName, FGameplayTag InputTag);

	/** Periodic drift compensation callback (1 second intervals) */
	void RealignBeatTracking();

	/** Beat broadcasting callback */
	void BroadcastBeatEvent();

	/** Calibration sequence callbacks */
	void PresentCalibrationPrompt();
	void ProcessCalibrationInput(float TimingValue);
	void CompleteCalibrationSequence();

	/** Get subdivision multiplier from enum */
	float GetSubdivisionMultiplier(EBeatSubdivision Subdivision) const;
	
	/** Check if game is currently paused */
	bool IsPausedState() const;
};
