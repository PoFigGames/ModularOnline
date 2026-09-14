// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Features/ModularFeatureSubsystem.h"
#include "Features/ModularFeatureTypes.h"
#include "Online/OnlineAsyncOpHandle.h"

#include "ModularAchievementsSubsystem.generated.h"

namespace UE::Online
{
	struct FAchievementStateUpdated;
}


/** How a request about achievements ends. */
DECLARE_DELEGATE_TwoParams(FModularAchievementsDelegate, const TArray<FModularAchievement>& /*Achievements*/, const FModularOnlineResult& /*Result*/);

/** One or more achievements were earned. */
DECLARE_MULTICAST_DELEGATE_OneParam(FModularAchievementsUnlockedEvent, const TArray<FString>& /*AchievementIds*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularAchievementsUnlockedDynamic, const TArray<FString>&, AchievementIds);


/**
 * @class UModularAchievementsSubsystem
 *
 * @brief What the player has earned, and the earning of it.
 */
UCLASS(MinimalAPI)
class UModularAchievementsSubsystem : public UModularFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Fired when the services say an achievement was earned, however it was earned. */
	FModularAchievementsUnlockedEvent OnAchievementsUnlocked { };

#pragma region UModularFeatureSubsystem

	MODULARONLINE_API virtual void Deinitialize() override;
	MODULARONLINE_API virtual FGameplayTag GetFeatureTag() const override;

#pragma endregion UModularFeatureSubsystem

	/** Fetches every achievement of the title together with what this player has done about them. */
	MODULARONLINE_API virtual bool QueryAchievements(int32 LocalPlayerIndex, FModularAchievementsDelegate OnComplete = FModularAchievementsDelegate { });

	/** What was last fetched, without asking again. */
	MODULARONLINE_API bool GetAchievements(int32 LocalPlayerIndex, TArray<FModularAchievement>& OutAchievements) const;

	/** Awards achievements. The services decide what to do about ones the player already has. */
	MODULARONLINE_API virtual bool UnlockAchievements(int32 LocalPlayerIndex, const TArray<FString>& AchievementIds);

	/** Opens the platform's own achievements screen, where the platform has one. */
	MODULARONLINE_API virtual bool ShowAchievementsUI(int32 LocalPlayerIndex);

protected:
	/** The same event, for Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Achievements", meta = (DisplayName = "On Achievements Unlocked"))
	FModularAchievementsUnlockedDynamic K2_OnAchievementsUnlocked { };

	/** Subscription to what the services say about achievements. */
	UE::Online::FOnlineEventDelegateHandle UnlockedHandle { };

	/** True once that subscription exists. */
	bool bListeningToUnlocks { false };

	/** Starts listening, if it has not already. */
	MODULARONLINE_API void EnsureListening();

	/** The services reported something earned. */
	MODULARONLINE_API void HandleAchievementsUnlocked(const UE::Online::FAchievementStateUpdated& EventParameters);

	/** What Blueprint may ask of this subsystem. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Achievements", meta = (DisplayName = "Query Achievements"))
	MODULARONLINE_API bool K2_QueryAchievements(int32 LocalPlayerIndex = 0) { return QueryAchievements(LocalPlayerIndex); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Achievements", meta = (DisplayName = "Get Achievements"))
	MODULARONLINE_API bool K2_GetAchievements(TArray<FModularAchievement>& OutAchievements, int32 LocalPlayerIndex = 0) const { return GetAchievements(LocalPlayerIndex, OutAchievements); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Achievements", meta = (DisplayName = "Unlock Achievements", AutoCreateRefTerm = "AchievementIds"))
	MODULARONLINE_API bool K2_UnlockAchievements(const TArray<FString>& AchievementIds, int32 LocalPlayerIndex = 0) { return UnlockAchievements(LocalPlayerIndex, AchievementIds); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Achievements", meta = (DisplayName = "Show Achievements UI"))
	MODULARONLINE_API bool K2_ShowAchievementsUI(int32 LocalPlayerIndex = 0) { return ShowAchievementsUI(LocalPlayerIndex); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Achievements", meta = (DisplayName = "Are Achievements Available"))
	MODULARONLINE_API bool K2_IsAvailable() const { return IsAvailable(); }
};
