// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Match/ModularMatchBackend.h"
#include "Online/Lobbies.h"


namespace PoFigGames::Online
{
	/**
	 * @class FModularLobbyBackend
	 *
	 * @brief A match carried in a lobby, which is how Steam and Epic Game Services carry one.
	 */
	class FModularLobbyBackend final : public IModularMatchBackend
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

		/** Turns a lobby into the shape a search result has. Public so that events can use it too. */
		static MODULARONLINE_API FModularMatchInfo DescribeLobby(const UE::Online::FLobby& Lobby);

	private:
		/** The lobby this local player is in under the given local name, or nothing. */
		static TSharedPtr<const UE::Online::FLobby> FindJoinedLobby(const FModularMatchContext& Context);
	};
}
