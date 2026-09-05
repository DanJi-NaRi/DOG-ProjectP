// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class ProjectP : ModuleRules
{
	public ProjectP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Allow module-relative includes (e.g. "GAS/MyAttributeSet.h", "Boss/Core/CPP_BossCharacter.h").
		// All in-module headers are included by their full module-relative path, so no per-folder include
		// paths are needed.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "HTTP",
            "InputCore",
            "Json",
            "NetCore",
            "Niagara",
            "Laser",
            "EnhancedInput",
			"GameplayAbilities",
			"GameplayMessageRuntime",
            "GameplayTags",
            "GameplayTasks",
            "AnimGraphRuntime",
            "UMG",
            "AIModule",
            "NavigationSystem",
            "DeveloperSettings",
            "StateTreeModule",
			"GameplayStateTreeModule",
			"CommonUI",
			"CommonInput",
			"GeometryCollectionEngine",
			"ProceduralMeshComponent",
			"WhisperRuntime"
        });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
