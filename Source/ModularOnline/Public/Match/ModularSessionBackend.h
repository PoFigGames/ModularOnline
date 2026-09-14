// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Match/ModularMatchBackend.h"
#include "Online/Sessions.h"


namespace PoFigGames::Online
{
	/**
	 * @class FModularSessionBackend
	 *
	 * @brief A match carried in a registered session, which is how a dedicated server publishes one.
	 */
	class FModularSessionBackend final : public IModularMatchBackend
	{
	public:

#pragma region IModularMatchBackend

		MODULARONLINE_API virtual FGameplayTag GetRequiredFeature() const override;
		MODULARONLINE_API virtual void CreateMatch(const FModularMatchContext& Context, const FModularMatchSettings& Settings, FModularMatchOperationDelegate OnComplete) override;
		MODULARONLINE_API virtual void FindMatches(const FModularMatchContext& Context, const FModularMatchSearchParams& Params, FModularMatchSearchDelegate OnComplete) override;
		MODULARONLINE_API virtual void JoinMatch(const FModularMatchContext& Context, const FModularMatchHandle& Match, bool bUsePresence, FModularMatchOperationDelegate OnComplete) override;
		MODULARONLINE_API virtual void LeaveMatch(const FModularMatchContext& Context, FModularMatchOperationDelegate OnComplete) override;
		MODULARONLINE_API virtual void InviteToMatch(const FModularMatchContext& Context, const UE::Online::FAccountId& TargetAccount, FModularMatchOperationDelegate OnComplete) override;
		MODULARONLINE_API virtual void KickMember(const FModularMatchContext& Context, const UE::Online::FAccountId& TargetAccount, FModularMatchOperationDelegate OnComplete) override;
		MODULARONLINE_API virtual void UpdateSettings(const FModularMatchContext& Context, const FModularMatchSettings& Settings, FModularMatchOperationDelegate OnComplete) override;
		MODULARONLINE_API virtual bool GetCurrentMatch(const FModularMatchContext& Context, FModularMatchHandle& OutMatch) const override;

#pragma endregion IModularMatchBackend

		/** Turns a session into the shape a search result has. Public so that events can use it too. */
		static MODULARONLINE_API FModularMatchInfo DescribeSession(const UE::Online::ISession& Session);

	private:
		/** The session this machine holds under the given local name, or nothing. */
		static TSharedPtr<const UE::Online::ISession> FindOwnSession(const FModularMatchContext& Context);
	};
}
