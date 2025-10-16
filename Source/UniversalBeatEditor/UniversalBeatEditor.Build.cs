// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UniversalBeatEditor : ModuleRules
{
	public UniversalBeatEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// UE5 best practice: disable legacy include paths
		bLegacyPublicIncludePaths = false;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UniversalBeat"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Sequencer",
				"MovieScene",
				"MovieSceneTools",
				"MovieSceneTracks",
				"LevelSequence",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"EditorStyle"
			}
		);
	}
}
