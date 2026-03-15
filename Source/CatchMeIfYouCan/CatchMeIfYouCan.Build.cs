// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CatchMeIfYouCan : ModuleRules
{
	public CatchMeIfYouCan(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput", 
			"GameplayAbilities", 
			"GameplayTags", 
			"GameplayTasks", 
			"UMG",
			"AnimGraphRuntime",
			"Niagara",
			"NiagaraCore",
			"MotionWarping",
			
			// OnlineSubSystem
			"OnlineSubsystem",
			"OnlineSubsystemEOS",
			"OnlineSubsystemUtils",
			
			// NetDriver, Socket
			"SocketSubSystemEOS",
			"Sockets",
			"Networking"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "AIModule" });
		
		PublicIncludePaths.AddRange(
			new string[] {
				"CatchMeIfYouCan"
			}
		);

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
