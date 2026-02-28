// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

using UnrealBuildTool;

public class Skelot : ModuleRules
{
	public Skelot(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		//bUseUnity = true;

        PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);

        PrivateIncludePaths.AddRange(new string[] {
            System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
            System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Internal"),
        });
        PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",  "RenderCore", "Engine", "NiagaraAnimNotifies", "Niagara", "Renderer", 
				// ... add other public dependencies that you statically link with here ...
			}

			);


        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject", "Projects", "Slate", "SlateCore", "Chaos", "PhysicsCore", "Landscape", "RHI", "DeveloperSettings", 
				// ... add private dependencies that you statically link with here ...	
			}
			);
        
		

        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "EditorStyle", "ContentBrowser", "DerivedDataCache" });
        }

        DynamicallyLoadedModuleNames.AddRange(new string[]
			{
				// ... add any modules that your module loads dynamically here ...
		});

		
        PublicDefinitions.AddRange(new string[] {
            "SKELOT_WITH_EXTRA_BONE=1", //should we support more than 4 bone influence ? will generate more Vertex Factories and Shader Permutation
			"SKELOT_WITH_MANUAL_VERTEX_FETCH=1",
            "SKELOT_WITH_GPUSCENE=1",	
        } );

		
    }
}
