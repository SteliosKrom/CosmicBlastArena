// Copyright David Sanchez 2025. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class SDL3_DGS : ModuleRules
{
    public SDL3_DGS(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        if (Target.Platform == UnrealTargetPlatform.Win64 && Target.Type != TargetType.Server)
        {
            PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));
            PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "Win64", "SDL3.lib"));
            PublicDelayLoadDLLs.Add("SDL3.dll");

            // External modules are never marked as precompiled, so UBT always evaluates
            // this rule and copies SDL3.dll to $(TargetOutputDir) alongside the game
            // executable -- unlike normal code modules where the copy is skipped when a
            // .precompiled manifest is present (Fab-distributed precompiled plugins).
            RuntimeDependencies.Add("$(TargetOutputDir)/SDL3.dll",
                Path.Combine(ModuleDirectory, "lib", "Win64", "SDL3.dll"));
        }
    }
}
