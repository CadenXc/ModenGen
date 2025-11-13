// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ModelGen : ModuleRules
{
	public ModelGen(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore" ,
			"ProceduralMeshComponent",
			"MeshDescription",
			"StaticMeshDescription", 
			"MeshConversion",
			"AssetRegistry",
			"PhysicsCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[] { 
			"PhysXCooking",
		});

		// 注意：已移除VHACD依赖，改用全平台支持的QuickHull实现（ModelGenConvexDecomp）
		// PublicDependencyModuleNames.Add("VHACD");

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
