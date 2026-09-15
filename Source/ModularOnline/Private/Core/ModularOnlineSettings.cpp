// Copyright PoFig Games Studio. All Rights Reserved.

#include "Core/ModularOnlineSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularOnlineSettings)

namespace PoFigGames::Online::Private
{
	/** Where every section of this plugin appears in the project settings. */
	static const FName SettingsCategory { TEXT("Plugins") };
}

/** The ini section each of these classes reads and writes. */

void UModularOnlineSettings::OverrideConfigSection(FString& OutSectionName)
{
	OutSectionName = TEXT("ModularOnline.Providers");
}

FName UModularOnlineSettings::GetCategoryName() const
{
	return PoFigGames::Online::Private::SettingsCategory;
}

void UModularMatchBackendSettings::OverrideConfigSection(FString& OutSectionName)
{
	OutSectionName = TEXT("ModularOnline.Matches");
}

FName UModularMatchBackendSettings::GetCategoryName() const
{
	return PoFigGames::Online::Private::SettingsCategory;
}

void UModularCrossPlaySettings::OverrideConfigSection(FString& OutSectionName)
{
	OutSectionName = TEXT("ModularOnline.CrossPlay");
}

FName UModularCrossPlaySettings::GetCategoryName() const
{
	return PoFigGames::Online::Private::SettingsCategory;
}

FName UModularAccountSettings::GetAvatarAttribute(const FString& ProviderName) const
{
	if (const auto Configured = AvatarAttributeByProvider.Find(ProviderName))
	{
		return *Configured;
	}

	return DefaultAvatarAttribute;
}

void UModularAccountSettings::OverrideConfigSection(FString& OutSectionName)
{
	OutSectionName = TEXT("ModularOnline.Accounts");
}

FName UModularAccountSettings::GetCategoryName() const
{
	return PoFigGames::Online::Private::SettingsCategory;
}

const FModularPresenceState* UModularPresenceSettings::FindState(const FGameplayTag State) const
{
	return States.FindByPredicate([State](const FModularPresenceState& Candidate) { return Candidate.State == State; });
}

void UModularPresenceSettings::OverrideConfigSection(FString& OutSectionName)
{
	OutSectionName = TEXT("ModularOnline.Presence");
}

FName UModularPresenceSettings::GetCategoryName() const
{
	return PoFigGames::Online::Private::SettingsCategory;
}

FName UModularServerSettings::GetServerCredentialsType(const FString& ProviderName) const
{
	if (const auto Configured = ServerCredentialsTypeByProvider.Find(ProviderName))
	{
		return *Configured;
	}

	return DefaultServerCredentialsType;
}

void UModularServerSettings::OverrideConfigSection(FString& OutSectionName)
{
	OutSectionName = TEXT("ModularOnline.DedicatedServer");
}

FName UModularServerSettings::GetCategoryName() const
{
	return PoFigGames::Online::Private::SettingsCategory;
}
