// Copyright PoFig Games Studio. All Rights Reserved.

using UnrealBuildTool;

/**
 * Settings shared by every module of this plugin.
 *
 * The plugin is built inside whatever project takes it, so it cannot read that project's rules; it asks
 * for what it needs itself, and every one of its modules is then compiled the same way wherever it lands.
 * The settings sit on the module rather than on a target, because a plugin has no target of its own and a
 * target in the shared build environment carries no compiler settings anyway.
 */
public static class ModularOnlineDefaults
{
	public static void Apply(ModuleRules module)
	{
		module.PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		var warnings = module.CppCompileWarningSettings;

		// A local hiding a member reads as the wrong thing entirely. MSVC also reports a local hiding a
		// member of a base class (C4458) where clang reported nothing when the two were compared on
		// 2026-09-15, so name locals apart from inherited members even when the local build is happy.
		warnings.ShadowVariableWarningLevel = WarningLevel.Error;

		// This plugin switches over engine enums that keep growing between versions, and over its own.
		warnings.EnumConversionWarningLevel = WarningLevel.Error;
		warnings.EnumEnumConversionWarningLevel = WarningLevel.Error;
		warnings.EnumFloatConversionWarningLevel = WarningLevel.Error;

		// A switch over an enum with no default label has to name every value. MSVC kept its counterpart
		// (C4062) off when this was checked on 2026-09-15, so this one bites on clang first.
		warnings.SwitchWarningLevel = WarningLevel.Error;

		// Comparisons and operators which cannot mean what they say.
		warnings.TautologicalCompareWarningLevel = WarningLevel.Error;
		warnings.BitwiseInsteadOfLogicalWarningLevel = WarningLevel.Error;

		// A memcpy over a non-trivial type, and deleting through a base with no virtual destructor.
		warnings.NonTrivialMemAccessWarningLevel = WarningLevel.Error;
		warnings.DeleteNonVirtualDtorWarningLevel = WarningLevel.Error;

		// A name misspelled in an #if quietly evaluates to zero.
		warnings.UndefinedIdentifierWarningLevel = WarningLevel.Error;
	}
}

public class ModularOnline : ModuleRules
{
	public ModularOnline(ReadOnlyTargetRules target) : base(target)
	{
		ModularOnlineDefaults.Apply(this);

		PublicDependencyModuleNames.AddRange(
			[
				"Core",
				"CoreOnline",
				// UDeveloperSettings, UGameInstanceSubsystem and the travel types, all of them in public
				// headers: a module that includes one of ours must not have to name these itself.
				"DeveloperSettings",
				"Engine",
				"GameplayTags",
				"OnlineServicesInterface",
			]
		);

		PrivateDependencyModuleNames.AddRange(
			[
				// The device mapper, which is what tells local players and their controllers apart.
				"ApplicationCore",
				"CoreUObject",
				// FKey and the viewport key handler, which is what a press start screen listens on.
				"InputCore",
				// The world scoped GetServices, which is what keeps two clients of a play in editor session
				// from sharing one login.
				"OnlineSubsystemUtils",
			]
		);
	}
}
