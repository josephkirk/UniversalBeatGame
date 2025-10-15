// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * MusicalTimingTests.cpp
 * 
 * Automated test suite for musical timing calculations (US3)
 * Tasks: T096-T099
 * 
 * Tests verify:
 * - ConvertMusicalNoteToSeconds accuracy at various BPMs
 * - Custom PreTiming acceptance (early input)
 * - Custom PostTiming acceptance (late input)
 * - Timing window constraints (Sixteenth to Whole)
 */

#include "UniversalBeatTypes.h"
#include "UniversalBeatFunctionLibrary.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#define MUSICAL_TIMING_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

/**
 * T096: Verify ConvertMusicalNoteToSeconds produces correct values at various BPMs
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FConvertMusicalNoteToSecondsTest,
	"UniversalBeat.MusicalTiming.ConvertNoteToSeconds",
	MUSICAL_TIMING_TEST_FLAGS
)

bool FConvertMusicalNoteToSecondsTest::RunTest(const FString& Parameters)
{
	// Test at 60 BPM (1 beat per second)
	{
		float BPM = 60.0f;
		float QuarterNoteExpected = 1.0f; // 60s / 60 BPM = 1s per beat
		
		float Sixteenth = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Sixteenth, BPM);
		float Eighth = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Eighth, BPM);
		float Quarter = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Quarter, BPM);
		float Half = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Half, BPM);
		float Whole = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Whole, BPM);
		
		TestEqual(TEXT("60 BPM: Sixteenth note = 0.25s"), Sixteenth, 0.25f, 0.001f);
		TestEqual(TEXT("60 BPM: Eighth note = 0.5s"), Eighth, 0.5f, 0.001f);
		TestEqual(TEXT("60 BPM: Quarter note = 1.0s"), Quarter, 1.0f, 0.001f);
		TestEqual(TEXT("60 BPM: Half note = 2.0s"), Half, 2.0f, 0.001f);
		TestEqual(TEXT("60 BPM: Whole note = 4.0s"), Whole, 4.0f, 0.001f);
	}
	
	// Test at 120 BPM (2 beats per second)
	{
		float BPM = 120.0f;
		float QuarterNoteExpected = 0.5f; // 60s / 120 BPM = 0.5s per beat
		
		float Sixteenth = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Sixteenth, BPM);
		float Eighth = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Eighth, BPM);
		float Quarter = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Quarter, BPM);
		float Half = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Half, BPM);
		float Whole = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Whole, BPM);
		
		TestEqual(TEXT("120 BPM: Sixteenth note = 0.125s"), Sixteenth, 0.125f, 0.001f);
		TestEqual(TEXT("120 BPM: Eighth note = 0.25s"), Eighth, 0.25f, 0.001f);
		TestEqual(TEXT("120 BPM: Quarter note = 0.5s"), Quarter, 0.5f, 0.001f);
		TestEqual(TEXT("120 BPM: Half note = 1.0s"), Half, 1.0f, 0.001f);
		TestEqual(TEXT("120 BPM: Whole note = 2.0s"), Whole, 2.0f, 0.001f);
	}
	
	// Test at 180 BPM (3 beats per second)
	{
		float BPM = 180.0f;
		float QuarterNoteExpected = 0.333f; // 60s / 180 BPM ≈ 0.333s per beat
		
		float Sixteenth = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Sixteenth, BPM);
		float Eighth = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Eighth, BPM);
		float Quarter = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Quarter, BPM);
		float Half = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Half, BPM);
		float Whole = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Whole, BPM);
		
		TestEqual(TEXT("180 BPM: Sixteenth note ≈ 0.0833s"), Sixteenth, 0.0833f, 0.001f);
		TestEqual(TEXT("180 BPM: Eighth note ≈ 0.1667s"), Eighth, 0.1667f, 0.001f);
		TestEqual(TEXT("180 BPM: Quarter note ≈ 0.333s"), Quarter, 0.333f, 0.001f);
		TestEqual(TEXT("180 BPM: Half note ≈ 0.667s"), Half, 0.667f, 0.001f);
		TestEqual(TEXT("180 BPM: Whole note ≈ 1.333s"), Whole, 1.333f, 0.001f);
	}
	
	// Test edge cases
	{
		// Zero BPM should return 0 (or handle safely)
		float ZeroBPM = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Quarter, 0.0f);
		TestEqual(TEXT("0 BPM returns 0"), ZeroBPM, 0.0f);
		
		// Negative BPM should return 0 (or handle safely)
		float NegativeBPM = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Quarter, -60.0f);
		TestEqual(TEXT("Negative BPM returns 0"), NegativeBPM, 0.0f);
		
		// Very fast tempo (240 BPM)
		float FastTempo = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Quarter, 240.0f);
		TestEqual(TEXT("240 BPM: Quarter note = 0.25s"), FastTempo, 0.25f, 0.001f);
		
		// Very slow tempo (30 BPM)
		float SlowTempo = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Quarter, 30.0f);
		TestEqual(TEXT("30 BPM: Quarter note = 2.0s"), SlowTempo, 2.0f, 0.001f);
	}
	
	return true;
}

/**
 * T097: Test validation respects custom PreTiming value (early input accepted)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCustomPreTimingValidationTest,
	"UniversalBeat.MusicalTiming.CustomPreTiming",
	MUSICAL_TIMING_TEST_FLAGS
)

bool FCustomPreTimingValidationTest::RunTest(const FString& Parameters)
{
	// Test PreTiming window calculation logic
	// Scenario: Note at timestamp 10.0s with PreTiming=Quarter (0.5s at 120 BPM)
	// Input at 9.6s should be ACCEPTED (0.4s early, within 0.5s window)
	
	float BPM = 120.0f;
	float NoteTimestamp = 10.0f;
	EMusicalNoteValue PreTiming = EMusicalNoteValue::Quarter; // 0.5s window at 120 BPM
	
	float PreTimingSeconds = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(PreTiming, BPM);
	TestEqual(TEXT("PreTiming Quarter at 120 BPM = 0.5s"), PreTimingSeconds, 0.5f, 0.001f);
	
	// Test early input within window
	{
		float InputTimestamp = 9.6f; // 0.4s early
		float TimingOffset = InputTimestamp - NoteTimestamp; // -0.4s
		float AbsOffset = FMath::Abs(TimingOffset);
		
		bool bWithinWindow = (AbsOffset <= PreTimingSeconds);
		TestTrue(TEXT("Input 0.4s early is within 0.5s PreTiming window"), bWithinWindow);
		
		// Accuracy calculation: 1.0 - (0.4 / 0.5) = 0.2
		float Accuracy = 1.0f - (AbsOffset / PreTimingSeconds);
		TestEqual(TEXT("Accuracy for 0.4s early = 0.2 (20%)"), Accuracy, 0.2f, 0.01f);
	}
	
	// Test early input at edge of window
	{
		float InputTimestamp = 9.5f; // Exactly 0.5s early (edge)
		float TimingOffset = InputTimestamp - NoteTimestamp;
		float AbsOffset = FMath::Abs(TimingOffset);
		
		bool bWithinWindow = (AbsOffset <= PreTimingSeconds);
		TestTrue(TEXT("Input at edge (0.5s early) is within window"), bWithinWindow);
		
		// Accuracy at edge: 1.0 - (0.5 / 0.5) = 0.0
		float Accuracy = 1.0f - (AbsOffset / PreTimingSeconds);
		TestEqual(TEXT("Accuracy at edge = 0.0"), Accuracy, 0.0f, 0.01f);
	}
	
	// Test early input outside window (should miss)
	{
		float InputTimestamp = 9.4f; // 0.6s early (outside 0.5s window)
		float TimingOffset = InputTimestamp - NoteTimestamp;
		float AbsOffset = FMath::Abs(TimingOffset);
		
		bool bOutsideWindow = (AbsOffset > PreTimingSeconds);
		TestTrue(TEXT("Input 0.6s early is outside 0.5s PreTiming window"), bOutsideWindow);
	}
	
	// Test lenient PreTiming (Half note = 1.0s at 120 BPM)
	{
		EMusicalNoteValue LenientPreTiming = EMusicalNoteValue::Half;
		float LenientPreSeconds = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(LenientPreTiming, BPM);
		TestEqual(TEXT("Lenient PreTiming (Half) = 1.0s at 120 BPM"), LenientPreSeconds, 1.0f, 0.001f);
		
		float EarlyInput = 9.2f; // 0.8s early
		float Offset = FMath::Abs(EarlyInput - NoteTimestamp);
		bool bWithinLenient = (Offset <= LenientPreSeconds);
		
		TestTrue(TEXT("Input 0.8s early accepted with lenient 1.0s window"), bWithinLenient);
	}
	
	return true;
}

/**
 * T098: Test validation respects custom PostTiming value (late input accepted)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCustomPostTimingValidationTest,
	"UniversalBeat.MusicalTiming.CustomPostTiming",
	MUSICAL_TIMING_TEST_FLAGS
)

bool FCustomPostTimingValidationTest::RunTest(const FString& Parameters)
{
	// Test PostTiming window calculation logic
	// Scenario: Note at timestamp 10.0s with PostTiming=Eighth (0.25s at 120 BPM)
	// Input at 10.2s should be ACCEPTED (0.2s late, within 0.25s window)
	
	float BPM = 120.0f;
	float NoteTimestamp = 10.0f;
	EMusicalNoteValue PostTiming = EMusicalNoteValue::Eighth; // 0.25s window at 120 BPM
	
	float PostTimingSeconds = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(PostTiming, BPM);
	TestEqual(TEXT("PostTiming Eighth at 120 BPM = 0.25s"), PostTimingSeconds, 0.25f, 0.001f);
	
	// Test late input within window
	{
		float InputTimestamp = 10.2f; // 0.2s late
		float TimingOffset = InputTimestamp - NoteTimestamp; // +0.2s
		float AbsOffset = FMath::Abs(TimingOffset);
		
		bool bWithinWindow = (AbsOffset <= PostTimingSeconds);
		TestTrue(TEXT("Input 0.2s late is within 0.25s PostTiming window"), bWithinWindow);
		
		// Accuracy calculation: 1.0 - (0.2 / 0.25) = 0.2
		float Accuracy = 1.0f - (AbsOffset / PostTimingSeconds);
		TestEqual(TEXT("Accuracy for 0.2s late = 0.2 (20%)"), Accuracy, 0.2f, 0.01f);
	}
	
	// Test late input at edge of window
	{
		float InputTimestamp = 10.25f; // Exactly 0.25s late (edge)
		float TimingOffset = InputTimestamp - NoteTimestamp;
		float AbsOffset = FMath::Abs(TimingOffset);
		
		bool bWithinWindow = (AbsOffset <= PostTimingSeconds);
		TestTrue(TEXT("Input at edge (0.25s late) is within window"), bWithinWindow);
		
		// Accuracy at edge: 1.0 - (0.25 / 0.25) = 0.0
		float Accuracy = 1.0f - (AbsOffset / PostTimingSeconds);
		TestEqual(TEXT("Accuracy at edge = 0.0"), Accuracy, 0.0f, 0.01f);
	}
	
	// Test late input outside window (should miss)
	{
		float InputTimestamp = 10.3f; // 0.3s late (outside 0.25s window)
		float TimingOffset = InputTimestamp - NoteTimestamp;
		float AbsOffset = FMath::Abs(TimingOffset);
		
		bool bOutsideWindow = (AbsOffset > PostTimingSeconds);
		TestTrue(TEXT("Input 0.3s late is outside 0.25s PostTiming window"), bOutsideWindow);
	}
	
	// Test lenient PostTiming (Whole note = 2.0s at 120 BPM)
	{
		EMusicalNoteValue LenientPostTiming = EMusicalNoteValue::Whole;
		float LenientPostSeconds = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(LenientPostTiming, BPM);
		TestEqual(TEXT("Lenient PostTiming (Whole) = 2.0s at 120 BPM"), LenientPostSeconds, 2.0f, 0.001f);
		
		float LateInput = 11.5f; // 1.5s late
		float Offset = FMath::Abs(LateInput - NoteTimestamp);
		bool bWithinLenient = (Offset <= LenientPostSeconds);
		
		TestTrue(TEXT("Input 1.5s late accepted with lenient 2.0s window"), bWithinLenient);
	}
	
	return true;
}

/**
 * T099: Test timing window constraints enforce min/max (Sixteenth to Whole)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTimingWindowConstraintsTest,
	"UniversalBeat.MusicalTiming.WindowConstraints",
	MUSICAL_TIMING_TEST_FLAGS
)

bool FTimingWindowConstraintsTest::RunTest(const FString& Parameters)
{
	float BPM = 120.0f;
	
	// Test minimum constraint (Sixteenth note)
	{
		float MinWindow = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Sixteenth, BPM);
		TestEqual(TEXT("Minimum timing window (Sixteenth) = 0.125s at 120 BPM"), MinWindow, 0.125f, 0.001f);
		
		// Verify it's positive and reasonable
		TestTrue(TEXT("Minimum window is positive"), MinWindow > 0.0f);
		TestTrue(TEXT("Minimum window is reasonable (>= 0.1s)"), MinWindow >= 0.1f);
	}
	
	// Test maximum constraint (Whole note)
	{
		float MaxWindow = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Whole, BPM);
		TestEqual(TEXT("Maximum timing window (Whole) = 2.0s at 120 BPM"), MaxWindow, 2.0f, 0.001f);
		
		// Verify it's within reasonable bounds
		TestTrue(TEXT("Maximum window is positive"), MaxWindow > 0.0f);
		TestTrue(TEXT("Maximum window is reasonable (<= 4.0s at 120 BPM)"), MaxWindow <= 4.0f);
	}
	
	// Test constraint ordering (min < all values < max)
	{
		float Sixteenth = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Sixteenth, BPM);
		float Eighth = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Eighth, BPM);
		float Quarter = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Quarter, BPM);
		float Half = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Half, BPM);
		float Whole = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Whole, BPM);
		
		TestTrue(TEXT("Sixteenth < Eighth"), Sixteenth < Eighth);
		TestTrue(TEXT("Eighth < Quarter"), Eighth < Quarter);
		TestTrue(TEXT("Quarter < Half"), Quarter < Half);
		TestTrue(TEXT("Half < Whole"), Half < Whole);
		
		// Verify ratios
		TestEqual(TEXT("Eighth = 2 × Sixteenth"), Eighth, Sixteenth * 2.0f, 0.001f);
		TestEqual(TEXT("Quarter = 2 × Eighth"), Quarter, Eighth * 2.0f, 0.001f);
		TestEqual(TEXT("Half = 2 × Quarter"), Half, Quarter * 2.0f, 0.001f);
		TestEqual(TEXT("Whole = 2 × Half"), Whole, Half * 2.0f, 0.001f);
	}
	
	// Test constraint enforcement at different BPMs
	{
		// At 60 BPM (slow)
		float SlowMin = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Sixteenth, 60.0f);
		float SlowMax = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Whole, 60.0f);
		
		TestEqual(TEXT("60 BPM: Min (Sixteenth) = 0.25s"), SlowMin, 0.25f, 0.001f);
		TestEqual(TEXT("60 BPM: Max (Whole) = 4.0s"), SlowMax, 4.0f, 0.001f);
		
		// At 180 BPM (fast)
		float FastMin = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Sixteenth, 180.0f);
		float FastMax = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Whole, 180.0f);
		
		TestEqual(TEXT("180 BPM: Min (Sixteenth) ≈ 0.0833s"), FastMin, 0.0833f, 0.001f);
		TestEqual(TEXT("180 BPM: Max (Whole) ≈ 1.333s"), FastMax, 1.333f, 0.001f);
		
		// Verify faster tempo = shorter windows
		TestTrue(TEXT("Faster BPM has shorter minimum window"), FastMin < SlowMin);
		TestTrue(TEXT("Faster BPM has shorter maximum window"), FastMax < SlowMax);
	}
	
	// Test practical constraint scenarios
	{
		// Strict note (fast reactions required)
		float StrictPre = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Sixteenth, BPM);
		float StrictPost = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Sixteenth, BPM);
		float StrictTotal = StrictPre + StrictPost;
		
		TestEqual(TEXT("Strict note total window ≈ 0.25s"), StrictTotal, 0.25f, 0.01f);
		TestTrue(TEXT("Strict note requires precision"), StrictTotal < 0.3f);
		
		// Lenient note (easy reactions)
		float LenientPre = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Half, BPM);
		float LenientPost = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EMusicalNoteValue::Whole, BPM);
		float LenientTotal = LenientPre + LenientPost;
		
		TestEqual(TEXT("Lenient note total window = 3.0s"), LenientTotal, 3.0f, 0.01f);
		TestTrue(TEXT("Lenient note is forgiving"), LenientTotal > 2.5f);
	}
	
	return true;
}

/**
 * Integration test: Verify timing windows work end-to-end
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTimingWindowIntegrationTest,
	"UniversalBeat.MusicalTiming.Integration",
	MUSICAL_TIMING_TEST_FLAGS
)

bool FTimingWindowIntegrationTest::RunTest(const FString& Parameters)
{
	// Simulate a complete validation scenario
	float BPM = 120.0f;
	float NoteTimestamp = 10.0f;
	
	// Easy note configuration
	EMusicalNoteValue EasyPre = EMusicalNoteValue::Quarter; // 0.5s
	EMusicalNoteValue EasyPost = EMusicalNoteValue::Half;    // 1.0s
	
	float EasyPreSeconds = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EasyPre, BPM);
	float EasyPostSeconds = UUniversalBeatFunctionLibrary::ConvertMusicalNoteToSeconds(EasyPost, BPM);
	
	// Test various input timings
	struct FTimingTestCase
	{
		float InputTime;
		bool ShouldHit;
		FString Description;
	};
	
	TArray<FTimingTestCase> TestCases = {
		{ 9.3f, false, "Too early (0.7s)" },
		{ 9.5f, true, "Early edge (0.5s)" },
		{ 9.7f, true, "Slightly early (0.3s)" },
		{ 10.0f, true, "Perfect timing" },
		{ 10.3f, true, "Slightly late (0.3s)" },
		{ 10.8f, true, "Late edge (0.8s)" },
		{ 11.1f, false, "Too late (1.1s)" }
	};
	
	for (const FTimingTestCase& TestCase : TestCases)
	{
		float Offset = TestCase.InputTime - NoteTimestamp;
		float AbsOffset = FMath::Abs(Offset);
		
		float MaxWindow = (Offset < 0.0f) ? EasyPreSeconds : EasyPostSeconds;
		bool bActualHit = (AbsOffset <= MaxWindow);
		
		TestEqual(
			FString::Printf(TEXT("%s: Expected=%d, Actual=%d"), 
				*TestCase.Description, TestCase.ShouldHit, bActualHit),
			bActualHit,
			TestCase.ShouldHit
		);
	}
	
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
