// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Features/ModularFeatureSubsystem.h"
#include "Features/ModularFeatureTypes.h"

#include "ModularUserInfoSubsystem.generated.h"


/** How a request about other people ends. */
DECLARE_DELEGATE_TwoParams(FModularUserProfilesDelegate, const TArray<FModularUserProfile>& /*Profiles*/, const FModularOnlineResult& /*Result*/);

/** The answer to a query this subsystem was asked for; what it fetched is in the cache. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FModularProfilesQueriedEvent, const TArray<FModularUserProfile>& /*Profiles*/, const FModularOnlineResult& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularProfilesQueriedDynamic, const TArray<FModularUserProfile>&, Profiles, const FModularOnlineResult&, Result);


/**
 * @class UModularUserInfoSubsystem
 *
 * @brief Names and pictures of accounts other than the local one.
 */
UCLASS(MinimalAPI)
class UModularUserInfoSubsystem : public UModularFeatureSubsystem
{
	GENERATED_BODY()

public:

	/** Fired when a query finishes, with what it fetched and how it ended. */
	FModularProfilesQueriedEvent OnProfilesQueried { };

#pragma region UModularFeatureSubsystem

	MODULARONLINE_API virtual FGameplayTag GetFeatureTag() const override;

#pragma endregion UModularFeatureSubsystem

	/** Asks the services about a set of accounts at once, which is cheaper than asking one by one. */
	MODULARONLINE_API virtual bool QueryProfiles(int32 LocalPlayerIndex, const TArray<FModularAccountHandle>& AccountIds, FModularUserProfilesDelegate OnComplete = FModularUserProfilesDelegate { });

	/** What the services last said about one account, without asking again. */
	MODULARONLINE_API bool GetProfile(int32 LocalPlayerIndex, const FModularAccountHandle& AccountId, FModularUserProfile& OutProfile) const;

	/** Opens the platform's own profile page for somebody, where the platform has one. */
	MODULARONLINE_API virtual bool ShowProfile(int32 LocalPlayerIndex, const FModularAccountHandle& AccountId);

protected:
	/** The same event, for Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|UserInfo", meta = (DisplayName = "On Profiles Queried"))
	FModularProfilesQueriedDynamic K2_OnProfilesQueried { };

	/** Answers a query on the caller's delegate and on both events. */
	MODULARONLINE_API void AnnounceProfiles(const TArray<FModularUserProfile>& Profiles, const FModularOnlineResult& Result, const FModularUserProfilesDelegate& OnComplete);

	/** What Blueprint may ask of this subsystem. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|UserInfo", meta = (DisplayName = "Query Profiles", AutoCreateRefTerm = "AccountIds"))
	MODULARONLINE_API bool K2_QueryProfiles(const TArray<FModularAccountHandle>& AccountIds, int32 LocalPlayerIndex = 0) { return QueryProfiles(LocalPlayerIndex, AccountIds); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|UserInfo", meta = (DisplayName = "Get Profile"))
	MODULARONLINE_API bool K2_GetProfile(const FModularAccountHandle& AccountId, FModularUserProfile& OutProfile, int32 LocalPlayerIndex = 0) const { return GetProfile(LocalPlayerIndex, AccountId, OutProfile); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|UserInfo", meta = (DisplayName = "Show Profile"))
	MODULARONLINE_API bool K2_ShowProfile(const FModularAccountHandle& AccountId, int32 LocalPlayerIndex = 0) { return ShowProfile(LocalPlayerIndex, AccountId); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|UserInfo", meta = (DisplayName = "Is User Info Available"))
	MODULARONLINE_API bool K2_IsAvailable() const { return IsAvailable(); }
};
