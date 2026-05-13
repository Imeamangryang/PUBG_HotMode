// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class PUBG_HotMode : ModuleRules
{
	public PUBG_HotMode(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "SlateCore", "GameplayTags", "NetCore", "Niagara" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		PublicIncludePaths.AddRange(new string[]
		{
			"PUBG_HotMode/GEONU",
			"PUBG_HotMode/SHIN",
			"PUBG_HotMode/WON/Public",
		});
	}
}
