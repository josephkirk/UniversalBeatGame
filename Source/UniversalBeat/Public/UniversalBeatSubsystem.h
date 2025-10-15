// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UniversalBeatTypes.h"
#include "Curves/CurveFloat.h"
#include "Engine/TimerHandle.h"
#include "UniversalBeatSubsystem.generated.h"

// Forward declarations
class USongConfiguration;
class ULevelSequencePlayer;
class ULevelSequence;
class UMovieSceneNoteChartSection;
class ALevelSequenceActor;

// Event dispatcher delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBeatInputCheck, FName, LabelName, FGameplayTag, InputTag, float, TimingValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeat, FBeatEventData, BeatData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCalibrationComplete, float, CalculatedOffsetMs, bool, bSuccess);

// Note chart system event delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSongStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSongEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrackStarted, int32, TrackIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrackEnded, int32, TrackIndex);

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
 * 
 * Note: Uses a dedicated "SongPlayer" LevelSequenceActor for all note chart playback.
 */
UCLASS()
class UNIVERSALBEAT_API UUniversalBeatSubsystem : public ULocalPlayerSubsystem
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
	 * Enhanced version for note chart system - returns detailed validation result.
	 * If no note chart is active, falls back to standard beat timing.
	 * Broadcasts OnBeatInputCheck event.
	 * 
	 * @param InputTag Gameplay tag identifier for this input
	 * @return Detailed validation result with accuracy, hit status, and timing feedback
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Timing", meta = (Tooltip = "Check timing accuracy with detailed feedback. Enhanced for note charts."))
	FNoteValidationResult CheckBeatTimingByTag(FGameplayTag InputTag);

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
	// 5. Note Chart System
	// ====================================================================

	/**
	 * Play a level sequence containing note chart tracks.
	 * Automatically loads notes and starts playback using the dedicated SongPlayer actor.
	 * 
	 * @param Sequence The level sequence containing note chart tracks
	 * @return True if sequence was successfully loaded and started
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|NoteChart", meta = (Tooltip = "Play sequence with note chart tracks."))
	bool PlayNoteChartSequence(ULevelSequence* Sequence);

	/**
	 * Stop the currently playing note chart sequence.
	 * Clears all loaded notes and resets tracking state.
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|NoteChart", meta = (Tooltip = "Stop current note chart sequence."))
	void StopNoteChartSequence();

	/**
	 * Get all loaded notes from the active note chart.
	 * 
	 * @return Array of all note instances (sorted by timestamp)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|NoteChart", meta = (Tooltip = "Get all loaded notes."))
	TArray<FNoteInstance> GetAllNotes() const;

	/**
	 * Get the total number of loaded notes.
	 * 
	 * @return Note count
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|NoteChart", meta = (Tooltip = "Get total note count."))
	int32 GetTotalNoteCount() const;

	/**
	 * Reset consumed notes (for looping sequences).
	 * Allows notes to be validated again on subsequent plays.
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|NoteChart", meta = (Tooltip = "Reset consumed notes for looping."))
	void ResetConsumedNotes();

	/**
	 * Check if a note chart sequence is currently playing.
	 * 
	 * @return True if a sequence is active
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|NoteChart", meta = (Tooltip = "Check if note chart is playing."))
	bool IsPlayingNoteChart() const;


	/**
	 * Register a song configuration for playback by tag.
	 * 
	 * @param SongConfig Song configuration data asset
	 * @return True if successfully registered, False if invalid or duplicate tag
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Songs", meta = (Tooltip = "Register song for playback. Returns false for invalid or duplicate tags."))
	bool RegisterSong(USongConfiguration* SongConfig);

	/**
	 * Unregister a song configuration.
	 * Stops the song if currently playing.
	 * 
	 * @param SongTag Gameplay tag of the song to unregister
	 * @return True if song was found and removed
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Songs", meta = (Tooltip = "Unregister song. Stops song if playing."))
	bool UnregisterSong(FGameplayTag SongTag);

	/**
	 * Play a registered song by its gameplay tag.
	 * 
	 * @param SongTag Gameplay tag of the song to play
	 * @param bQueue If true, enqueue song after current. If false, clear queue and play immediately.
	 * @return True if song was found and queued/started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Songs", meta = (Tooltip = "Play registered song by tag. Queue=true adds to playlist, Queue=false plays immediately."))
	bool PlaySongByTag(FGameplayTag SongTag, bool bQueue = false);

	/**
	 * Play a song by directly passing its data asset reference.
	 * Song will be automatically registered if not already registered.
	 * 
	 * @param SongAsset Soft object reference to the song configuration data asset
	 * @param bQueue If true, enqueue song after current. If false, clear queue and play immediately.
	 * @return True if song asset is valid and queued/started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Songs", meta = (Tooltip = "Play song by asset reference. Auto-registers if needed. Queue=true adds to playlist, Queue=false plays immediately."))
	bool PlaySongByAsset(TSoftObjectPtr<USongConfiguration> SongAsset, bool bQueue = false);
	
	/**
	 * Stop the currently playing song.
	 * Cleans up all active track players and timers.
	 */
	UFUNCTION(BlueprintCallable, Category = "UniversalBeat|Songs", meta = (Tooltip = "Stop current song and clean up track players."))
	void StopCurrentSong();

	/**
	 * Get the currently playing song configuration.
	 * 
	 * @return Current song config, or null if none playing
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Songs", meta = (Tooltip = "Get currently playing song configuration."))
	USongConfiguration* GetCurrentSong() const;

	/**
	 * Get the active track players for the current song.
	 * 
	 * @return Array of level sequence players for active tracks
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Songs", meta = (Tooltip = "Get active level sequence players."))
	TArray<ULevelSequencePlayer*> GetActiveTracks() const;

	/**
	 * Check if a song is currently playing.
	 * 
	 * @return True if any song is active
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UniversalBeat|Songs", meta = (Tooltip = "Check if any song is currently playing."))
	bool IsPlayingSong() const;

	// ====================================================================
	// 6. Debug & Utility
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
	// 7. Event Dispatchers
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

	// Note Chart System Events

	/**
	 * Event fired when a song starts playing.
	 * Receives the song configuration asset.
	 */
	UPROPERTY(BlueprintAssignable, Category = "UniversalBeat|Song Events")
	FOnSongStarted OnSongStarted;

	/**
	 * Event fired when a song ends (all non-looping tracks complete).
	 * Receives the song configuration asset.
	 */
	UPROPERTY(BlueprintAssignable, Category = "UniversalBeat|Song Events")
	FOnSongEnded OnSongEnded;

	/**
	 * Event fired when an individual track starts playing (after delay).
	 * Receives song tag and track index.
	 */
	UPROPERTY(BlueprintAssignable, Category = "UniversalBeat|Song Events")
	FOnTrackStarted OnTrackStarted;

	/**
	 * Event fired when an individual track ends (non-looping tracks only).
	 * Receives song tag and track index.
	 */
	UPROPERTY(BlueprintAssignable, Category = "UniversalBeat|Song Events")
	FOnTrackEnded OnTrackEnded;

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

	// Note Chart System State

	/** Map of registered song configurations by gameplay tag */
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<USongConfiguration>> RegisteredSongs;

	/** Queue of songs to play */
	TQueue<TObjectPtr<USongConfiguration>> QueuedSongs;

	/** Queue of tracks from current song to play sequentially */
	TQueue<FNoteTrackEntry> QueuedTracks;

	/** Currently playing song configuration */
	UPROPERTY()
	TObjectPtr<USongConfiguration> CurrentlyPlayingSong = nullptr;

	/** Currently playing track info */
	FNoteTrackEntry CurrentTrackInfo;

	/** Timer handle for delayed track start */
	FTimerHandle TrackDelayTimer;

	// Note Chart Tracking (moved from UNoteChartDirector)

	/** Dedicated sequence actor for note chart playback */
	UPROPERTY()
	TObjectPtr<ALevelSequenceActor> SongPlayerActor = nullptr;

	/** Currently loaded note chart sequence */
	UPROPERTY()
	TObjectPtr<ULevelSequence> CurrentNoteChartSequence = nullptr;

	/** Cached sorted notes from all note chart sections */
	UPROPERTY()
	TArray<FNoteInstance> CachedNotesSorted;

	/** Set of consumed note timestamps (for fast lookup) */
	UPROPERTY()
	TSet<int32> ConsumedNoteTimestamps;

	/** Current note index for sequential playback tracking */
	int32 CurrentNoteIndex = 0;

	/** Frame rate of the registered sequence (cached for performance) */
	FFrameRate CachedSequenceFrameRate;

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

	// Note Chart System Helpers

	/** Clean up song playback state */
	void CleanupSongPlayback();

	/** Dequeue and play next song from queue */
	void PlaySong();

	/** Dequeue and play next track from current song */
	void PlayTrack();

	/** Start a track with delay */
	void StartTrackWithDelay(float DelaySeconds);

	/** Callback when a track delay timer expires */
	void OnTrackDelayComplete();

	/** Callback when a track sequence finishes */
	UFUNCTION()
	void OnTrackSequenceFinished();
	
	/** Callback when SongPlayerActor finishes (delegates to OnTrackSequenceFinished) */
	UFUNCTION()
	void OnSongPlayerFinished();

	/** Check if all non-looping tracks have completed */
	bool CheckSongCompletion() const;

	// Note Chart Helpers (moved from UNoteChartDirector)

	/** Load notes from a level sequence for validation tracking */
	bool LoadNoteChartFromSequence(ULevelSequence* Sequence);

	/** Clear all loaded notes and reset tracking state */
	void ClearNoteChart();

	/** Find next note with matching tag within timing window */
	bool GetNextNoteForTag(FGameplayTag NoteTag, float CurrentTime, FNoteInstance& OutNote);

	/** Mark a note as consumed (prevents re-validation) */
	void MarkNoteConsumed(const FNoteInstance& Note);

	/** Check if a note has been consumed */
	bool IsNoteConsumed(const FNoteInstance& Note) const;

	/** Convert frame number to seconds using cached sequence frame rate */
	float FrameToSeconds(FFrameNumber Frame) const;

	/** Convert seconds to frame number using cached sequence frame rate */
	FFrameNumber SecondsToFrame(float Seconds) const;

	/** Get current playback time from song player */
	float GetCurrentPlaybackTime() const;

	/** Create or get the SongPlayer actor and player using static factory method */
	void EnsureSongPlayerActor();

	/** Get the sequence player from SongPlayerActor with validation */
	ULevelSequencePlayer* GetSongPlayer() const;
};
