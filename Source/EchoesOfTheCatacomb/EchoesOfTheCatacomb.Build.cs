// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EchoesOfTheCatacomb : ModuleRules
{
	public EchoesOfTheCatacomb(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"HTTP",
			"Json",
			"JsonUtilities",
			"AudioCapture",
			"AudioMixer",
			"Voice"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"EchoesOfTheCatacomb",
			"EchoesOfTheCatacomb/Variant_Platforming",
			"EchoesOfTheCatacomb/Variant_Platforming/Animation",
			"EchoesOfTheCatacomb/Variant_Combat",
			"EchoesOfTheCatacomb/Variant_Combat/AI",
			"EchoesOfTheCatacomb/Variant_Combat/Animation",
			"EchoesOfTheCatacomb/Variant_Combat/Gameplay",
			"EchoesOfTheCatacomb/Variant_Combat/Interfaces",
			"EchoesOfTheCatacomb/Variant_Combat/UI",
			"EchoesOfTheCatacomb/Variant_SideScrolling",
			"EchoesOfTheCatacomb/Variant_SideScrolling/AI",
			"EchoesOfTheCatacomb/Variant_SideScrolling/Gameplay",
			"EchoesOfTheCatacomb/Variant_SideScrolling/Interfaces",
			"EchoesOfTheCatacomb/Variant_SideScrolling/UI"
		});
	}
}
