// Copyright PoFig Games Studio. All Rights Reserved.

#include "Features/ModularSocialSubsystem.h"

#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineTags.h"
#include "Online/OnlineResult.h"
#include "Online/Social.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularSocialSubsystem)

void UModularSocialSubsystem::AnnounceFriends(const TArray<FModularFriend>& Friends, const FModularOnlineResult& Result, const FModularFriendsDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(Friends, Result);
	OnFriendsQueried.Broadcast(Friends, Result);
	K2_OnFriendsQueried.Broadcast(Friends, Result);
}

namespace PoFigGames::Online::Private
{
	/** What the services call a relationship, in the words this plugin speaks. */
	static EModularRelationship FromOnlineRelationship(const UE::Online::ERelationship Relationship)
	{
		switch (Relationship)
		{
		case UE::Online::ERelationship::Friend:
			return EModularRelationship::Friend;

		case UE::Online::ERelationship::InviteSent:
			return EModularRelationship::InviteSent;

		case UE::Online::ERelationship::InviteReceived:
			return EModularRelationship::InviteReceived;

		case UE::Online::ERelationship::Blocked:
			return EModularRelationship::Blocked;

		case UE::Online::ERelationship::NotFriend:
			break;
		}

		return EModularRelationship::NotFriend;
	}

	/** One entry of the friends list, as a screen reads it. */
	static FModularFriend DescribeFriend(const UE::Online::FFriend& Friend)
	{
		FModularFriend Described;
		Described.AccountId = MakeModularAccount(Friend.FriendId);
		Described.DisplayName = Friend.DisplayName;
		Described.Nickname = Friend.Nickname;
		Described.Relationship = FromOnlineRelationship(Friend.Relationship);

		return Described;
	}
}

void UModularSocialSubsystem::Deinitialize()
{
	RelationshipHandle = UE::Online::FOnlineEventDelegateHandle { };
	bListeningToRelationships = false;

	Super::Deinitialize();
}

FGameplayTag UModularSocialSubsystem::GetFeatureTag() const
{
	return ModularOnlineTags::Feature_Social;
}

void UModularSocialSubsystem::EnsureListening()
{
	const auto Social = GetInterface<UE::Online::ISocial>();

	if (!ShouldStartListening(bListeningToRelationships) || !Social.IsValid())
	{
		return;
	}

	RelationshipHandle = Social->OnRelationshipUpdated().Add(this, &ThisClass::HandleRelationshipUpdated);
	bListeningToRelationships = true;
}

void UModularSocialSubsystem::HandleRelationshipUpdated(const UE::Online::FRelationshipUpdated& EventParameters)
{
	const auto AccountId = MakeModularAccount(EventParameters.RemoteAccountId);
	const auto Relationship = PoFigGames::Online::Private::FromOnlineRelationship(EventParameters.NewRelationship);

	OnRelationshipChanged.Broadcast(AccountId, Relationship);
	K2_OnRelationshipChanged.Broadcast(AccountId, Relationship);
}

bool UModularSocialSubsystem::QueryFriends(const int32 LocalPlayerIndex, FModularFriendsDelegate OnComplete)
{
	const auto Social = GetInterface<UE::Online::ISocial>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!Social.IsValid())
	{
		AnnounceFriends(TArray<FModularFriend> { }, MissingFeature(), OnComplete);

		return false;
	}

	if (!Account.IsValid())
	{
		AnnounceFriends(TArray<FModularFriend> { }, NotSignedIn(), OnComplete);

		return false;
	}

	EnsureListening();

	Social->QueryFriends({ Account }).OnComplete(this, [this, LocalPlayerIndex, OnComplete](const UE::Online::TOnlineResult<UE::Online::FQueryFriends>& Result)
	{
		if (Result.IsError())
		{
			AnnounceFriends(TArray<FModularFriend> { }, FModularOnlineResult::FromOnlineError(Result.GetErrorValue()), OnComplete);

			return;
		}

		// The query itself answers with nothing; the list it filled is read back from the cache.
		TArray<FModularFriend> Friends;
		GetFriends(LocalPlayerIndex, Friends);

		AnnounceFriends(Friends, FModularOnlineResult::Success(), OnComplete);
	});

	return true;
}

bool UModularSocialSubsystem::GetFriends(const int32 LocalPlayerIndex, TArray<FModularFriend>& OutFriends) const
{
	const auto Social = GetInterface<UE::Online::ISocial>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!Social.IsValid() || !Account.IsValid())
	{
		return false;
	}

	const auto Cached = Social->GetFriends({ Account });
	if (!Cached.IsOk())
	{
		return false;
	}

	OutFriends.Reset(Cached.GetOkValue().Friends.Num());

	for (const auto& Friend : Cached.GetOkValue().Friends)
	{
		OutFriends.Add(PoFigGames::Online::Private::DescribeFriend(*Friend));
	}

	return true;
}

bool UModularSocialSubsystem::SendFriendInvite(const int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId)
{
	const auto Social = GetInterface<UE::Online::ISocial>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);
	const auto Target = TargetAccountId.AccountId;

	if (!Social.IsValid() || !Account.IsValid() || !Target.IsValid())
	{
		UE_LOG(LogModularOnline, Verbose, TEXT("No friend invite was sent to %s."), *TargetAccountId.Id);

		return false;
	}

	Social->SendFriendInvite({ Account, Target }).OnComplete(this, [this](const UE::Online::TOnlineResult<UE::Online::FSendFriendInvite>& Result)
		{
			UE_CLOG(Result.IsError(), LogModularOnline, Warning, TEXT("The services refused to send a friend invite: %s"), *ToLogString(Result.GetErrorValue()));
		});

	return true;
}

bool UModularSocialSubsystem::RespondToFriendInvite(const int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId, const bool bAccept)
{
	const auto Social = GetInterface<UE::Online::ISocial>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);
	const auto Target = TargetAccountId.AccountId;

	if (!Social.IsValid() || !Account.IsValid() || !Target.IsValid())
	{
		return false;
	}

	if (bAccept)
	{
		Social->AcceptFriendInvite({ Account, Target }).OnComplete(this, [this](const UE::Online::TOnlineResult<UE::Online::FAcceptFriendInvite>& Result)
		{
			UE_CLOG(Result.IsError(), LogModularOnline, Warning, TEXT("The services refused to accept a friend invite: %s"), *ToLogString(Result.GetErrorValue()));
		});
	}
	else
	{
		Social->RejectFriendInvite({ Account, Target }).OnComplete(this, [this](const UE::Online::TOnlineResult<UE::Online::FRejectFriendInvite>& Result)
		{
			UE_CLOG(Result.IsError(), LogModularOnline, Warning, TEXT("The services refused to reject a friend invite: %s"), *ToLogString(Result.GetErrorValue()));
		});
	}

	return true;
}
