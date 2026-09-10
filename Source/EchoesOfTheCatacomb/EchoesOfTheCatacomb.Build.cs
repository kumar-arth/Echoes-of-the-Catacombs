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
			"Slate"
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

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
