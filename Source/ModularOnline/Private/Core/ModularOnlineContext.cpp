// Copyright PoFig Games Studio. All Rights Reserved.

#include "Core/ModularOnlineContext.h"

#include "Core/ModularOnlineTags.h"

namespace PoFigGames::Online
{
	FModularOnlineContext::FModularOnlineContext(const EModularOnlineRole InRole, UE::Online::IOnlineServicesPtr InServices)
		: Role(InRole)
		, Services(MoveTemp(InServices))
	{
		RefreshFeatures();
	}

	UE::Online::EOnlineServices FModularOnlineContext::GetProvider() const
	{
		return Services.IsValid() ? Services->GetServicesProvider() : UE::Online::EOnlineServices::None;
	}

	FString FModularOnlineContext::GetProviderName() const
	{
		return LexToString(GetProvider());
	}

	void FModularOnlineContext::RefreshFeatures()
	{
		Features.Reset();

		if (!Services.IsValid())
		{
			return;
		}

		// Asked through the named getters rather than the component registry, because these fifteen are
		// the components the engine itself declares: a provider either registered one or it did not, and
		// this is the cheapest way to find out. Anything beyond them is reached with GetInterface.
		const auto AddIfPresent = [this](const bool bIsPresent, const FGameplayTag& Feature)
		{
			if (bIsPresent)
			{
				Features.AddTag(Feature);
			}
		};

		AddIfPresent(Services->GetAuthInterface().IsValid(), ModularOnlineTags::Feature_Auth);
		AddIfPresent(Services->GetPrivilegesInterface().IsValid(), ModularOnlineTags::Feature_Privileges);
		AddIfPresent(Services->GetConnectivityInterface().IsValid(), ModularOnlineTags::Feature_Connectivity);
		AddIfPresent(Services->GetUserInfoInterface().IsValid(), ModularOnlineTags::Feature_UserInfo);
		AddIfPresent(Services->GetExternalUIInterface().IsValid(), ModularOnlineTags::Feature_ExternalUI);
		AddIfPresent(Services->GetLobbiesInterface().IsValid(), ModularOnlineTags::Feature_Lobbies);
		AddIfPresent(Services->GetSessionsInterface().IsValid(), ModularOnlineTags::Feature_Sessions);
		AddIfPresent(Services->GetPresenceInterface().IsValid(), ModularOnlineTags::Feature_Presence);
		AddIfPresent(Services->GetSocialInterface().IsValid(), ModularOnlineTags::Feature_Social);
		AddIfPresent(Services->GetAchievementsInterface().IsValid(), ModularOnlineTags::Feature_Achievements);
		AddIfPresent(Services->GetStatsInterface().IsValid(), ModularOnlineTags::Feature_Stats);
		AddIfPresent(Services->GetLeaderboardsInterface().IsValid(), ModularOnlineTags::Feature_Leaderboards);
		AddIfPresent(Services->GetTitleFileInterface().IsValid(), ModularOnlineTags::Feature_TitleFile);
		AddIfPresent(Services->GetUserFileInterface().IsValid(), ModularOnlineTags::Feature_UserFile);
		AddIfPresent(Services->GetCommerceInterface().IsValid(), ModularOnlineTags::Feature_Commerce);
	}
}
