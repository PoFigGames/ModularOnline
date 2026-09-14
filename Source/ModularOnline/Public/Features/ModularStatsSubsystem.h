// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Features/ModularFeatureSubsystem.h"
#include "Features/ModularFeatureTypes.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/Stats.h"

#include "ModularStatsSubsystem.generated.h"


/** A set of statistics, keyed by name. Named because a macro cannot take a type with a comma in it. */
using FModularStatMap = TMap<FString, FModularStatValue>;

/** How a request about statistics ends. */
DECLARE_DELEGATE_TwoParams(FModularStatsDelegate, const FModularStatMap& /*Stats*/, const FModularOnlineResult& /*Result*/);

/** The services recorded new numbers for somebody. */
DECLARE_MULTICAST_DELEGATE_OneParam(FModularStatsUpdatedEvent, const FModularAccountHandle& /*AccountId*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularStatsUpdatedDynamic, const FModularAccountHandle&, AccountId);


/**
 * @class UModularStatsSubsystem
 *
 * @brief The numbers a backend keeps about a player.
 */
UCLASS(MinimalAPI)
class UModularStatsSubsystem : public UModularFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Fired when the services record new numbers, whoever caused it. */
	FModularStatsUpdatedEvent OnStatsUpdated { };

#pragma region UModularFeatureSubsystem

	MODULARONLINE_API virtual void Deinitialize() override;
	MODULARONLINE_API virtual FGameplayTag GetFeatureTag() const override;

#pragma endregion UModularFeatureSubsystem

	/** Sends new numbers for the local player. */
	MODULARONLINE_API virtual bool UpdateStats(int32 LocalPlayerIndex, const TMap<FString, FModularStatValue>& Stats);

	/** Asks the services for somebody's numbers; an empty list of names asks for all of them. */
	MODULARONLINE_API virtual bool QueryStats(int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId, const TArray<FString>& StatNames, FModularStatsDelegate OnComplete = FModularStatsDelegate { });

	/** What the services last said about somebody, without asking again. */
	MODULARONLINE_API bool GetCachedStats(int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId, TMap<FString, FModularStatValue>& OutStats) const;

protected:
	/** The same event, for Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Stats", meta = (DisplayName = "On Stats Updated"))
	FModularStatsUpdatedDynamic K2_OnStatsUpdated { };

	/** Subscription to what the services say about statistics. */
	UE::Online::FOnlineEventDelegateHandle StatsUpdatedHandle { };

	/** True once that subscription exists. */
	bool bListeningToStats { false };

	/** Starts listening, if it has not already. */
	MODULARONLINE_API void EnsureListening();

	/** The services recorded something. */
	MODULARONLINE_API void HandleStatsUpdated(const UE::Online::FStatsUpdated& EventParameters);

	/** What Blueprint may ask of this subsystem. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Stats", meta = (DisplayName = "Update Stats", AutoCreateRefTerm = "Stats"))
	MODULARONLINE_API bool K2_UpdateStats(const TMap<FString, FModularStatValue>& Stats, int32 LocalPlayerIndex = 0) { return UpdateStats(LocalPlayerIndex, Stats); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Stats", meta = (DisplayName = "Query Stats", AutoCreateRefTerm = "StatNames"))
	MODULARONLINE_API bool K2_QueryStats(const FModularAccountHandle& TargetAccountId, const TArray<FString>& StatNames, int32 LocalPlayerIndex = 0) { return QueryStats(LocalPlayerIndex, TargetAccountId, StatNames); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Stats", meta = (DisplayName = "Get Cached Stats"))
	MODULARONLINE_API bool K2_GetCachedStats(const FModularAccountHandle& TargetAccountId, TMap<FString, FModularStatValue>& OutStats, int32 LocalPlayerIndex = 0) const { return GetCachedStats(LocalPlayerIndex, TargetAccountId, OutStats); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Stats", meta = (DisplayName = "Are Stats Available"))
	MODULARONLINE_API bool K2_IsAvailable() const { return IsAvailable(); }
};
