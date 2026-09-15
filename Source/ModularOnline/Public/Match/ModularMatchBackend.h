// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Core/ModularOnlineTypes.h"
#include "GameplayTagContainer.h"
#include "Match/ModularMatchTypes.h"
#include "Online/CoreOnline.h"
#include "Online/OnlineServices.h"


/** How an operation over a match answers. */
DECLARE_DELEGATE_OneParam(FModularMatchOperationDelegate, const FModularOnlineResult& /*Result*/);

/** How a search answers. */
DECLARE_DELEGATE_TwoParams(FModularMatchSearchDelegate, const FModularOnlineResult& /*Result*/, const TArray<FModularMatchInfo>& /*Matches*/);


namespace PoFigGames::Online
{
	/**
	 * @struct FModularMatchContext
	 *
	 * @brief Who is asking, of which services, about which of their matches.
	 */
	struct FModularMatchContext
	{
		/** The services instance this call goes to. */
		UE::Online::IOnlineServicesPtr Services { nullptr };

		/** The account of the local player making the call. */
		UE::Online::FAccountId LocalAccount { };

		/** Local name of the match, which is how the services tell a game's several matches apart. */
		FName LocalName { };

		/** True when everything needed to make a call is here. */
		bool IsValid() const { return Services.IsValid() && LocalAccount.IsValid() && !LocalName.IsNone(); }
	};


	/**
	 * @class IModularMatchBackend
	 *
	 * @brief One way of carrying a match: a lobby, or a registered session.
	 */
	class IModularMatchBackend
	{
	public:
		virtual ~IModularMatchBackend() = default;

		/** The component this backend is built on; a provider without it cannot use this backend. */
		virtual FGameplayTag GetRequiredFeature() const = 0;

		/** Opens a match and publishes it as the settings ask. */
		virtual void CreateMatch(const FModularMatchContext& Context, const FModularMatchSettings& Settings, FModularMatchOperationDelegate OnComplete) = 0;

		/** Looks for matches others have opened. */
		virtual void FindMatches(const FModularMatchContext& Context, const FModularMatchSearchParams& Params, FModularMatchSearchDelegate OnComplete) = 0;

		/** Joins a match a search or an invitation produced. */
		virtual void JoinMatch(const FModularMatchContext& Context, const FModularMatchHandle& Match, bool bUsePresence, FModularMatchOperationDelegate OnComplete) = 0;

		/** Leaves the match this local player is in. */
		virtual void LeaveMatch(const FModularMatchContext& Context, FModularMatchOperationDelegate OnComplete) = 0;

		/** Invites somebody to the match this local player is in. */
		virtual void InviteToMatch(const FModularMatchContext& Context, const UE::Online::FAccountId& TargetAccount, FModularMatchOperationDelegate OnComplete) = 0;

		/** Removes somebody from the match. Only the host may. */
		virtual void KickMember(const FModularMatchContext& Context, const UE::Online::FAccountId& TargetAccount, FModularMatchOperationDelegate OnComplete) = 0;

		/** Republishes what the match says about itself: its map, its name, who may join. */
		virtual void UpdateSettings(const FModularMatchContext& Context, const FModularMatchSettings& Settings, FModularMatchOperationDelegate OnComplete) = 0;

		/** The match this local player is in, if any. */
		virtual bool GetCurrentMatch(const FModularMatchContext& Context, FModularMatchHandle& OutMatch) const = 0;

		/**
		 * A string a stranger published, in a shape a widget may show.
		 *
		 * Match names, map names and every other attribute come off the wire as the host typed them. Control
		 * characters and an unbounded length are the two that do damage on the way to a list.
		 */
		static MODULARONLINE_API FString DescribePublishedText(const FString& Published);

		/**
		 * Whether an attribute belongs to the provider rather than to the match.
		 *
		 * A provider publishes its own bookkeeping beside a match under a reserved prefix, and an update
		 * that takes it off is either refused outright or blanks the fields a search filters on.
		 */
		static MODULARONLINE_API bool IsProviderAttribute(FName Attribute);
	};
}
