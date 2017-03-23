// Copyright 2017 The Mill, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DeckLinkMediaFactory : ModuleRules
	{
		public DeckLinkMediaFactory(TargetInfo Target)
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
                    "MediaAssets",
                }
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
					"DeckLinkMedia",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"DeckLinkMediaFactory/Private",
				}
			);

			if (Target.Type == TargetRules.TargetType.Editor)
			{
				DynamicallyLoadedModuleNames.Add("Settings");
				PrivateIncludePathModuleNames.Add("Settings");
			}

			if ((Target.Platform == UnrealTargetPlatform.Win32) ||
				(Target.Platform == UnrealTargetPlatform.Win64))
			{
				DynamicallyLoadedModuleNames.Add("DeckLinkMedia");
			}
		}
	}
}
