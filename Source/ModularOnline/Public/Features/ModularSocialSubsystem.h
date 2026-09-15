// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Features/ModularFeatureSubsystem.h"
#include "Features/ModularFeatureTypes.h"
#include "Online/OnlineAsyncOpHandle.h"

#include "ModularSocialSubsystem.generated.h"

namespace UE::Online
{
	struct FRelationshipUpdated;
}


/** How a request about the friends list ends. */
DECLARE_DELEGATE_TwoParams(FModularFriendsDelegate, const TArray<FModularFriend>& /*Friends*/, const FModularOnlineResult& /*Result*/);

/** The answer to a query this subsystem was asked for; what it fetched is in the cache. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FModularFriendsQueriedEvent, const TArray<FModularFriend>& /*Friends*/, const FModularOnlineResult& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularFriendsQueriedDynamic, const TArray<FModularFriend>&, Friends, const FModularOnlineResult&, Result);

/** What one account is to the local player changed. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FModularRelationshipEvent, const FModularAccountHandle& /*AccountId*/, EModularRelationship /*Relationship*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularRelationshipDynamic, const FModularAccountHandle&, AccountId, EModularRelationship, Relationship);


/**
 * @class UModularSocialSubsystem
 *
 * @brief Who the player knows: friends, invitations and the people they would rather not meet.
 */
UCLASS(MinimalAPI)
class UModularSocialSubsystem : public UModularFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Fired when somebody becomes a friend, stops being one, or is blocked. */
	FModularRelationshipEvent OnRelationshipChanged { };

/** Fired when a query finishes, with what it fetched and how it ended. */
	FModularFriendsQueriedEvent OnFriendsQueried { };

	#pragma region UModularFeatureSubsystem

	MODULARONLINE_API virtual void Deinitialize() override;
	MODULARONLINE_API virtual FGameplayTag GetFeatureTag() const override;

#pragma endregion UModularFeatureSubsystem

	/** Fetches the friends list, and keeps listening for changes to it afterwards. */
	MODULARONLINE_API virtual bool QueryFriends(int32 LocalPlayerIndex, FModularFriendsDelegate OnComplete = FModularFriendsDelegate { });

	/** The friends list as the services last reported it, without asking again. */
	MODULARONLINE_API bool GetFriends(int32 LocalPlayerIndex, TArray<FModularFriend>& OutFriends) const;

	/** Asks somebody to be a friend. */
	MODULARONLINE_API virtual bool SendFriendInvite(int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId);

	/** Answers an invitation somebody sent. */
	MODULARONLINE_API virtual bool RespondToFriendInvite(int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId, bool bAccept);

protected:
	/** The same event, for Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Social", meta = (DisplayName = "On Friends Queried"))
	FModularFriendsQueriedDynamic K2_OnFriendsQueried { };

	/** Answers a query on the caller's delegate and on both events. */
	MODULARONLINE_API void AnnounceFriends(const TArray<FModularFriend>& Friends, const FModularOnlineResult& Result, const FModularFriendsDelegate& OnComplete);
	/** The same event, for Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Social", meta = (DisplayName = "On Relationship Changed"))
	FModularRelationshipDynamic K2_OnRelationshipChanged { };

	/** Subscription to what the services say about relationships. */
	UE::Online::FOnlineEventDelegateHandle RelationshipHandle { };

	/** True once that subscription exists. */
	bool bListeningToRelationships { false };

	/** Starts listening, if it has not already. */
	MODULARONLINE_API void EnsureListening();

	/** The services reported a change. */
	MODULARONLINE_API void HandleRelationshipUpdated(const UE::Online::FRelationshipUpdated& EventParameters);

	/** What Blueprint may ask of this subsystem. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Social", meta = (DisplayName = "Query Friends"))
	MODULARONLINE_API bool K2_QueryFriends(int32 LocalPlayerIndex = 0) { return QueryFriends(LocalPlayerIndex); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Social", meta = (DisplayName = "Get Friends"))
	MODULARONLINE_API bool K2_GetFriends(TArray<FModularFriend>& OutFriends, int32 LocalPlayerIndex = 0) const { return GetFriends(LocalPlayerIndex, OutFriends); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Social", meta = (DisplayName = "Send Friend Invite"))
	MODULARONLINE_API bool K2_SendFriendInvite(const FModularAccountHandle& TargetAccountId, int32 LocalPlayerIndex = 0) { return SendFriendInvite(LocalPlayerIndex, TargetAccountId); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Social", meta = (DisplayName = "Respond To Friend Invite"))
	MODULARONLINE_API bool K2_RespondToFriendInvite(const FModularAccountHandle& TargetAccountId, bool bAccept, int32 LocalPlayerIndex = 0) { return RespondToFriendInvite(LocalPlayerIndex, TargetAccountId, bAccept); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Social", meta = (DisplayName = "Is Social Available"))
	MODULARONLINE_API bool K2_IsAvailable() const { return IsAvailable(); }
};
