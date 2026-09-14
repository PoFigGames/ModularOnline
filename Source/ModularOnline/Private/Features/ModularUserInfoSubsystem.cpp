// Copyright PoFig Games Studio. All Rights Reserved.

#include "Features/ModularUserInfoSubsystem.h"

#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineTags.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineResult.h"
#include "Online/UserInfo.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularUserInfoSubsystem)

FGameplayTag UModularUserInfoSubsystem::GetFeatureTag() const
{
	return ModularOnlineTags::Feature_UserInfo;
}

bool UModularUserInfoSubsystem::QueryProfiles(const int32 LocalPlayerIndex, const TArray<FModularAccountHandle>& AccountIds, FModularUserProfilesDelegate OnComplete)
{
	const auto UserInfo = GetInterface<UE::Online::IUserInfo>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!UserInfo.IsValid())
	{
		OnComplete.ExecuteIfBound(TArray<FModularUserProfile> { }, MissingFeature());

		return false;
	}

	if (!Account.IsValid())
	{
		OnComplete.ExecuteIfBound(TArray<FModularUserProfile> { }, NotSignedIn());

		return false;
	}

	UE::Online::FQueryUserInfo::Params Params;
	Params.LocalAccountId = Account;
	Params.AccountIds.Reserve(AccountIds.Num());

	for (const auto& AccountId : AccountIds)
	{
		if (AccountId.IsValid())
		{
			Params.AccountIds.Add(AccountId.AccountId);
		}
		else
		{
			UE_LOG(LogModularOnline, Verbose, TEXT("'%s' names no account the services can be asked about and was left out of the query."), *AccountId.Id);
		}
	}

	if (Params.AccountIds.IsEmpty())
	{
		// Nothing this provider could be asked about. Answering empty rather than never answering keeps
		// the caller from waiting for something that will not come.
		OnComplete.ExecuteIfBound(TArray<FModularUserProfile> { }, FModularOnlineResult::Success());

		return true;
	}

	UserInfo->QueryUserInfo(MoveTemp(Params)).OnComplete(this, [this, LocalPlayerIndex, AccountIds, OnComplete](const UE::Online::TOnlineResult<UE::Online::FQueryUserInfo>& Result)
	{
		if (Result.IsError())
		{
			OnComplete.ExecuteIfBound(TArray<FModularUserProfile> { }, FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));

			return;
		}

		// The query fills a cache; what it filled is read back for the accounts that were asked about.
		TArray<FModularUserProfile> Profiles;
		Profiles.Reserve(AccountIds.Num());

		for (const auto& AccountId : AccountIds)
		{
			if (FModularUserProfile Profile; GetProfile(LocalPlayerIndex, AccountId, Profile))
			{
				Profiles.Add(MoveTemp(Profile));
			}
		}

		OnComplete.ExecuteIfBound(Profiles, FModularOnlineResult::Success());
	});

	return true;
}

bool UModularUserInfoSubsystem::GetProfile(const int32 LocalPlayerIndex, const FModularAccountHandle& AccountId, FModularUserProfile& OutProfile) const
{
	const auto UserInfo = GetInterface<UE::Online::IUserInfo>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);
	const auto Target = AccountId.AccountId;

	if (!UserInfo.IsValid() || !Account.IsValid() || !Target.IsValid())
	{
		return false;
	}

	const auto Cached = UserInfo->GetUserInfo({ Account, Target });
	if (!Cached.IsOk())
	{
		return false;
	}

	OutProfile.AccountId = AccountId;
	OutProfile.DisplayName = Cached.GetOkValue().UserInfo->DisplayName;

	// The avatar is a separate question, and a provider that answers the first does not have to answer
	// the second; an empty url is a normal answer, not a failure.
	if (const auto Avatar = UserInfo->GetUserAvatar({ Account, Target }); Avatar.IsOk())
	{
		OutProfile.AvatarUrl = Avatar.GetOkValue().AvatarUrl;
	}

	return true;
}

bool UModularUserInfoSubsystem::ShowProfile(const int32 LocalPlayerIndex, const FModularAccountHandle& AccountId)
{
	const auto UserInfo = GetInterface<UE::Online::IUserInfo>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);
	const auto Target = AccountId.AccountId;

	if (!UserInfo.IsValid() || !Account.IsValid() || !Target.IsValid())
	{
		return false;
	}

	UserInfo->ShowUserProfile({ Account, Target }).OnComplete(this, [](const UE::Online::TOnlineResult<UE::Online::FShowUserProfile>& Result)
		{
			UE_CLOG(Result.IsError(), LogModularOnline, Warning, TEXT("The services refused to open a profile: %s"), *ToLogString(Result.GetErrorValue()));
		});

	return true;
}
