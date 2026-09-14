// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Blueprint/AsyncAction_ModularOnline.h"
#include "Match/ModularMatchTypes.h"

#include "AsyncAction_ModularMatch.generated.h"

class UModularMatchSubsystem;


/** Where a search puts what it found. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularMatchSearchCompleted, const TArray<FModularMatchInfo>&, Matches, const FModularOnlineResult&, Result);


/**
 * @class UAsyncAction_ModularHostMatch
 *
 * @brief Opens a match and travels to its map.
 */
UCLASS(MinimalAPI)
class UAsyncAction_ModularHostMatch : public UAsyncAction_ModularOnlineResultBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Match", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static MODULARONLINE_API UAsyncAction_ModularHostMatch* HostMatch(const UObject* WorldContextObject, FModularMatchSettings Settings, int32 LocalPlayerIndex = 0);

#pragma region UBlueprintAsyncActionBase

	MODULARONLINE_API virtual void Activate() override;

#pragma endregion UBlueprintAsyncActionBase

private:
	TWeakObjectPtr<UModularMatchSubsystem> Matches { };
	FModularMatchSettings MatchSettings { };
	int32 PlayerIndex { 0 };
};


/**
 * @class UAsyncAction_ModularJoinMatch
 *
 * @brief Joins a match a search or an invitation produced, and travels to it.
 */
UCLASS(MinimalAPI)
class UAsyncAction_ModularJoinMatch : public UAsyncAction_ModularOnlineResultBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Match", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static MODULARONLINE_API UAsyncAction_ModularJoinMatch* JoinMatch(const UObject* WorldContextObject, FModularMatchHandle Match, int32 LocalPlayerIndex = 0);

#pragma region UBlueprintAsyncActionBase

	MODULARONLINE_API virtual void Activate() override;

#pragma endregion UBlueprintAsyncActionBase

private:
	TWeakObjectPtr<UModularMatchSubsystem> Matches { };
	FModularMatchHandle Handle { };
	int32 PlayerIndex { 0 };
};


/**
 * @class UAsyncAction_ModularFindMatches
 *
 * @brief Looks for matches to join.
 */
UCLASS(MinimalAPI)
class UAsyncAction_ModularFindMatches : public UAsyncAction_ModularOnlineBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Match", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static MODULARONLINE_API UAsyncAction_ModularFindMatches* FindMatches(const UObject* WorldContextObject, FModularMatchSearchParams Params, int32 LocalPlayerIndex = 0);

	/** Matches were found; the list may still be empty when nobody is hosting. */
	UPROPERTY(BlueprintAssignable)
	FModularMatchSearchCompleted OnSuccess;

	/** The search itself failed, or this platform cannot search at all. */
	UPROPERTY(BlueprintAssignable)
	FModularMatchSearchCompleted OnFailure;

#pragma region UBlueprintAsyncActionBase

	MODULARONLINE_API virtual void Activate() override;

#pragma endregion UBlueprintAsyncActionBase

private:
	TWeakObjectPtr<UModularMatchSubsystem> Matches { };
	FModularMatchSearchParams SearchParams { };
	int32 PlayerIndex { 0 };
};
