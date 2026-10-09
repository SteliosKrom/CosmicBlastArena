// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CosmicBlastArena : ModuleRules
{
	public CosmicBlastArena(ReadOnlyTargetRules Target) : base(Target)
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
			"CosmicBlastArena",
			"CosmicBlastArena/Variant_Platforming",
			"CosmicBlastArena/Variant_Platforming/Animation",
			"CosmicBlastArena/Variant_Combat",
			"CosmicBlastArena/Variant_Combat/AI",
			"CosmicBlastArena/Variant_Combat/Animation",
			"CosmicBlastArena/Variant_Combat/Gameplay",
			"CosmicBlastArena/Variant_Combat/Interfaces",
			"CosmicBlastArena/Variant_Combat/UI",
			"CosmicBlastArena/Variant_SideScrolling",
			"CosmicBlastArena/Variant_SideScrolling/AI",
			"CosmicBlastArena/Variant_SideScrolling/Gameplay",
			"CosmicBlastArena/Variant_SideScrolling/Interfaces",
			"CosmicBlastArena/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
