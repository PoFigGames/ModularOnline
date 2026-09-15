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

/** The answer to a query this subsystem was asked for; what it fetched is in the cache. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FModularLeaderboardQueriedEvent, const TArray<FModularLeaderboardEntry>& /*Entries*/, const FModularOnlineResult& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularLeaderboardQueriedDynamic, const TArray<FModularLeaderboardEntry>&, Entries, const FModularOnlineResult&, Result);


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

/** Fired when a query finishes, with what it fetched and how it ended. */
	FModularLeaderboardQueriedEvent OnLeaderboardQueried { };

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
	/** The same event, for Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Leaderboards", meta = (DisplayName = "On Entries Queried"))
	FModularLeaderboardQueriedDynamic K2_OnLeaderboardQueried { };

	/** Answers a query on the caller's delegate and on both events. */
	MODULARONLINE_API void AnnounceEntries(const TArray<FModularLeaderboardEntry>& Entries, const FModularOnlineResult& Result, const FModularLeaderboardDelegate& OnComplete);

	/** Whether a read can happen at all, answering the caller itself when it cannot. */
	MODULARONLINE_API bool CanRead(const TSharedPtr<UE::Online::ILeaderboards>& Leaderboards, const UE::Online::FAccountId& Account,
		const FModularLeaderboardDelegate& OnComplete);

	/** What Blueprint may ask of this subsystem. Every read answers on On Entries Queried. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Leaderboards", meta = (DisplayName = "Read Around Player"))
	MODULARONLINE_API bool K2_ReadAroundPlayer(const FString& BoardName, int32 Offset = 0, int32 Limit = 10, int32 LocalPlayerIndex = 0)
	{
		return ReadAroundPlayer(LocalPlayerIndex, BoardName, Offset, Limit, FModularLeaderboardDelegate { });
	}

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Leaderboards", meta = (DisplayName = "Read Around Rank"))
	MODULARONLINE_API bool K2_ReadAroundRank(const FString& BoardName, int32 Rank = 0, int32 Limit = 10, int32 LocalPlayerIndex = 0)
	{
		return ReadAroundRank(LocalPlayerIndex, BoardName, Rank, Limit, FModularLeaderboardDelegate { });
	}

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Leaderboards", meta = (DisplayName = "Read For Players"))
	MODULARONLINE_API bool K2_ReadForPlayers(const FString& BoardName, const TArray<FModularAccountHandle>& AccountIds, int32 LocalPlayerIndex = 0)
	{
		return ReadForPlayers(LocalPlayerIndex, BoardName, AccountIds, FModularLeaderboardDelegate { });
	}

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Leaderboards", meta = (DisplayName = "Are Leaderboards Available"))
	MODULARONLINE_API bool K2_IsAvailable() const { return IsAvailable(); }
};
