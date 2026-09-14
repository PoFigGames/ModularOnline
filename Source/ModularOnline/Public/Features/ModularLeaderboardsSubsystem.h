// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Features/ModularFeatureSubsystem.h"
#include "Features/ModularFeatureTypes.h"

#include "ModularLeaderboardsSubsystem.generated.h"

namespace UE::Online
{
	class ILeaderboards;
}


/** How a request for a ranked table ends. */
DECLARE_DELEGATE_TwoParams(FModularLeaderboardDelegate, const TArray<FModularLeaderboardEntry>& /*Entries*/, const FModularOnlineResult& /*Result*/);


/**
 * @class UModularLeaderboardsSubsystem
 *
 * @brief Ranked tables, read three ways.
 */
UCLASS(MinimalAPI)
class UModularLeaderboardsSubsystem : public UModularFeatureSubsystem
{
	GENERATED_BODY()

public:

#pragma region UModularFeatureSubsystem

	MODULARONLINE_API virtual FGameplayTag GetFeatureTag() const override;

#pragma endregion UModularFeatureSubsystem

	/** The lines around the local player, which is what a results screen shows. */
	MODULARONLINE_API virtual bool ReadAroundPlayer(int32 LocalPlayerIndex, const FString& BoardName, int32 Offset, int32 Limit, FModularLeaderboardDelegate OnComplete);

	/** The lines around a place in the table, which is what a top ten is. */
	MODULARONLINE_API virtual bool ReadAroundRank(int32 LocalPlayerIndex, const FString& BoardName, int32 Rank, int32 Limit, FModularLeaderboardDelegate OnComplete);

	/** The lines of named people, which is how a friends only board is built. */
	MODULARONLINE_API virtual bool ReadForPlayers(int32 LocalPlayerIndex, const FString& BoardName, const TArray<FModularAccountHandle>& AccountIds, FModularLeaderboardDelegate OnComplete);

protected:
	/** Whether a read can happen at all, answering the caller itself when it cannot. */
	MODULARONLINE_API bool CanRead(const TSharedPtr<UE::Online::ILeaderboards>& Leaderboards, const UE::Online::FAccountId& Account,
		const FModularLeaderboardDelegate& OnComplete) const;

	/** What Blueprint may ask of this subsystem. The answers arrive on the events of the caller's own node. */
	UFUNCTION(BlueprintPure, Category = "ModularOnline|Leaderboards", meta = (DisplayName = "Are Leaderboards Available"))
	MODULARONLINE_API bool K2_IsAvailable() const { return IsAvailable(); }
};
