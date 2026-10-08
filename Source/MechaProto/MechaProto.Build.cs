// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MechaProto : ModuleRules
{
	public MechaProto(ReadOnlyTargetRules Target) : base(Target)
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
			"SlateCore",
			"EngineSettings",
			"CoreOnline",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"Niagara"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"MechaProto",
			"MechaProto/Framework",
			"MechaProto/Characters",
			"MechaProto/Interaction",
			"MechaProto/Online",
			"MechaProto/Voice",
			"MechaProto/Movement",
			"MechaProto/Climb",
			"MechaProto/Stations",
			"MechaProto/Items",
			"MechaProto/Enemies",
			"MechaProto/Hull",
			"MechaProto/Mech",
			"MechaProto/UI",
			"MechaProto/Repair",
			"MechaProto/Variant_Horror",
			"MechaProto/Variant_Horror/UI",
			"MechaProto/Variant_Shooter",
			"MechaProto/Variant_Shooter/AI",
			"MechaProto/Variant_Shooter/UI",
			"MechaProto/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
