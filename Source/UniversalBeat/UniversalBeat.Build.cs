// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UniversalBeat : ModuleRules
{
	public UniversalBeat(ReadOnlyTargetRules Target) : base(Target)
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
				"GameplayTags",
				"LevelSequence",
				"MovieScene",
				"MovieSceneTracks"
			}
		);
		PrivateDependencyModuleNames.AddRange(new string[]
			{
				"SlateCore",
				"AnimGraphRuntime",
				"PropertyPath"
			});
		if (Target.bBuildWithEditorOnlyData && Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(new string[]
				{
					"BlueprintGraph"
				});
			PrivateDependencyModuleNames.AddRange(new string[]
				{
					"AnimationBlueprintLibrary",
					"DataLayerEditor",
					"EditorFramework",
					"UnrealEd"
				});
		}
	}
}
