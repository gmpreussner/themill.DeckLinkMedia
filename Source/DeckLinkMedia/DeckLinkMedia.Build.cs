// Copyright 2017 The Mill, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    using System.IO;

    public class DeckLinkMedia : ModuleRules
	{
		public DeckLinkMedia(TargetInfo Target)
		{
            DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
                    "CoreUObject",
                    "DeckLinkMediaFactory",
					"RenderCore",
					"RHI",
                    "Projects",
                    "Engine",
                    "Slate",
                    "SlateCore",
                }
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"DeckLinkMedia/Private",
                    "DeckLinkMedia/Private/Assets",
                    "DeckLinkMedia/Private/Player",
				}
			);

            PublicDependencyModuleNames.AddRange(
                new string[] {
                    "MediaAssets",
                }
            );

            //Blackmagic DeckLink Sdk
            string SdiDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "ThirdParty"));
            string LibDir = Path.Combine(SdiDir, "DeckLinkLibs");
            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                PublicLibraryPaths.Add(Path.Combine(LibDir));
                PublicDelayLoadDLLs.Add("DeckLink Audio64.dll");
                PublicDelayLoadDLLs.Add("Decklink64.dll");
                PublicDelayLoadDLLs.Add("DeckLinkAPI64.dll");
            }
            else
            {
                System.Console.WriteLine("DeckLinkMedia does not supported this platform");
            }
        }
	}
}
