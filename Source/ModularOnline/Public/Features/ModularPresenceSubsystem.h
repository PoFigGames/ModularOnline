// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Features/ModularFeatureSubsystem.h"
#include "Features/ModularFeatureTypes.h"
#include "Online/OnlineAsyncOpHandle.h"

#include "ModularPresenceSubsystem.generated.h"

namespace UE::Online
{
	struct FPresenceUpdated;
	struct FUserPresence;
}


/** How a request about somebody's presence ends. */
DECLARE_DELEGATE_TwoParams(FModularPresenceDelegate, const FModularPresence& /*Presence*/, const FModularOnlineResult& /*Result*/);

/** Somebody's presence changed. */
DECLARE_MULTICAST_DELEGATE_OneParam(FModularPresenceUpdatedEvent, const FModularPresence& /*Presence*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularPresenceUpdatedDynamic, const FModularPresence&, Presence);


/**
 * @class UModularPresenceSubsystem
 *
 * @brief What the player is shown to be doing, and what their friends are doing.
 */
UCLASS(MinimalAPI)
class UModularPresenceSubsystem : public UModularFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Fired whenever the services report a change, for the local player or for anybody watched. */
	FModularPresenceUpdatedEvent OnPresenceUpdated { };

#pragma region UModularFeatureSubsystem

	MODULARONLINE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	MODULARONLINE_API virtual void Deinitialize() override;
	MODULARONLINE_API virtual FGameplayTag GetFeatureTag() const override;

#pragma endregion UModularFeatureSubsystem

	/** Publishes what the local player is doing. */
	MODULARONLINE_API virtual bool SetPresence(int32 LocalPlayerIndex, EModularPresenceStatus Status, const FString& StatusText, const TMap<FString, FString>& Properties, EModularPresenceJoinability Joinability = EModularPresenceJoinability::Unknown);

	/** Publishes a state the project described in its presence settings. */
	MODULARONLINE_API virtual bool PublishState(int32 LocalPlayerIndex, FGameplayTag State, const TMap<FString, FString>& Values);

	/** Asks the services what somebody is doing, and keeps listening for changes afterwards. */
	MODULARONLINE_API virtual bool QueryPresence(int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId, FModularPresenceDelegate OnComplete = FModularPresenceDelegate { });

	/** What the services last said about somebody, without asking again. */
	MODULARONLINE_API bool GetCachedPresence(int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId, FModularPresence& OutPresence) const;

protected:
	/** The same events, for Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Presence", meta = (DisplayName = "On Presence Updated"))
	FModularPresenceUpdatedDynamic K2_OnPresenceUpdated { };

	/** Subscription to what the services say about presence. */
	UE::Online::FOnlineEventDelegateHandle PresenceUpdatedHandle { };

	/** True once that subscription exists; the handle itself does not say. */
	bool bListeningToPresence { false };

	/** Turns what the services answered into what a screen reads. */
	MODULARONLINE_API static FModularPresence Describe(const UE::Online::FUserPresence& Presence);

	/** The services reported a change. */
	MODULARONLINE_API void HandlePresenceUpdated(const UE::Online::FPresenceUpdated& EventParameters);

	/** What Blueprint may ask of this subsystem. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Presence", meta = (DisplayName = "Publish Presence State", AutoCreateRefTerm = "Values"))
	MODULARONLINE_API bool K2_PublishState(FGameplayTag State, const TMap<FString, FString>& Values, int32 LocalPlayerIndex = 0) { return PublishState(LocalPlayerIndex, State, Values); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Presence", meta = (DisplayName = "Set Presence", AutoCreateRefTerm = "Properties"))
	MODULARONLINE_API bool K2_SetPresence(EModularPresenceStatus Status, const FString& StatusText, const TMap<FString, FString>& Properties, EModularPresenceJoinability Joinability = EModularPresenceJoinability::Unknown, int32 LocalPlayerIndex = 0) { return SetPresence(LocalPlayerIndex, Status, StatusText, Properties, Joinability); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Presence", meta = (DisplayName = "Query Presence"))
	MODULARONLINE_API bool K2_QueryPresence(const FModularAccountHandle& TargetAccountId, int32 LocalPlayerIndex = 0) { return QueryPresence(LocalPlayerIndex, TargetAccountId); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Presence", meta = (DisplayName = "Get Cached Presence"))
	MODULARONLINE_API bool K2_GetCachedPresence(const FModularAccountHandle& TargetAccountId, FModularPresence& OutPresence, int32 LocalPlayerIndex = 0) const { return GetCachedPresence(LocalPlayerIndex, TargetAccountId, OutPresence); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Presence", meta = (DisplayName = "Is Presence Available"))
	MODULARONLINE_API bool K2_IsAvailable() const { return IsAvailable(); }
};
