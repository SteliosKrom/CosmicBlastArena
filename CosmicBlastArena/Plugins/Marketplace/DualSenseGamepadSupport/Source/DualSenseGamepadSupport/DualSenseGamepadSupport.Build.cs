// Copyright David Sanchez 2025. All Rights Reserved.
// DualSenseGamepadSupport.Build.cs
using UnrealBuildTool;
using System.IO;

public class DualSenseGamepadSupport : ModuleRules
{
    public DualSenseGamepadSupport(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "Engine",
                "InputCore",
                "ApplicationCore",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "InputDevice",
                "Projects",
            }
        );

        PublicDefinitions.Add("WITH_SDL3=0");

        if (Target.Platform == UnrealTargetPlatform.Win64 && Target.Type != TargetType.Server)
        {
            // SDL3 is declared as an External module (SDL3_DGS.Build.cs). External modules are
            // never precompiled, so UBT always evaluates their RuntimeDependencies and copies
            // SDL3.dll to the target output directory before UAT runs staging.
            PublicDefinitions.Remove("WITH_SDL3=0");
            PublicDefinitions.Add("WITH_SDL3=1");
            PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "ThirdParty", "SDL3", "include"));
            PrivateDependencyModuleNames.Add("SDL3_DGS");
        }
    }
}
