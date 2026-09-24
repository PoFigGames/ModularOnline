// Copyright PoFig Games Studio. All Rights Reserved.

using UnrealBuildTool;

public class ModularOnlineDeveloper : ModuleRules
{
	public ModularOnlineDeveloper(ReadOnlyTargetRules target) : base(target)
	{
		ModularOnlineDefaults.Apply(this);

		PrivateDependencyModuleNames.AddRange(
			[
				"Core",
				"CoreOnline",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"ModularOnline",
				"OnlineServicesInterface",
				// IPluginManager, which is where the tests find the plugin's own translations.
				"Projects",
			]
		);

		// The descriptor builder and the serializer registry, which the account id test asks about.
		SetupIrisSupport(target);
	}
}
