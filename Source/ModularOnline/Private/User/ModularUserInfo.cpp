// Copyright PoFig Games Studio. All Rights Reserved.

#include "User/ModularUserInfo.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/OnlineReplStructs.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularUserInfo)


using PoFigGames::Online::LexToString;

bool UModularUserInfo::IsLoggedIn() const
{
	return State == EModularUserState::LoggedInLocally || State == EModularUserState::LoggedInOnline;
}

bool UModularUserInfo::IsLoggingIn() const
{
	return State == EModularUserState::LoggingIn;
}

UModularUserInfo::FRoleData& UModularUserInfo::FindOrAddRoleData(const EModularOnlineRole Role)
{
	return RoleData.FindOrAdd(Role);
}

const UModularUserInfo::FRoleData* UModularUserInfo::FindRoleData(const EModularOnlineRole Role) const
{
	if (const auto Found = RoleData.Find(Role))
	{
		return Found;
	}

	// A project with one provider signs in once, under the default role; asking it about the platform or
	// the service is the same question.
	if (Role != EModularOnlineRole::Default)
	{
		return RoleData.Find(EModularOnlineRole::Default);
	}

	return nullptr;
}

bool UModularUserInfo::IsSignedInOnline() const
{
	if (!IsLoggedIn() || bIsGuest)
	{
		return false;
	}

	// Any role holding a real account is enough: which of them it is depends on how the project is
	// configured, and the caller asking this question does not care.
	for (const auto& Pair : RoleData)
	{
		if (Pair.Value.AccountId.IsValid())
		{
			return true;
		}
	}

	return false;
}

UE::Online::FAccountId UModularUserInfo::GetAccountId(const EModularOnlineRole Role) const
{
	if (const auto Data = FindRoleData(Role))
	{
		return Data->AccountId;
	}

	return UE::Online::FAccountId { };
}

FString UModularUserInfo::GetNickname(const EModularOnlineRole Role) const
{
	if (const auto Data = FindRoleData(Role))
	{
		return Data->Nickname;
	}

	return FString { };
}

FString UModularUserInfo::GetAvatarUrl(const EModularOnlineRole Role) const
{
	if (const auto Data = FindRoleData(Role))
	{
		return Data->AvatarUrl;
	}

	return FString { };
}

EModularOnlinePrivilegeResult UModularUserInfo::GetPrivilege(const EModularOnlinePrivilege Privilege, const EModularOnlineRole Role) const
{
	if (const auto Data = FindRoleData(Role))
	{
		if (const auto Found = Data->Privileges.Find(Privilege))
		{
			return *Found;
		}
	}

	return EModularOnlinePrivilegeResult::Unknown;
}

void UModularUserInfo::ApplyToLocalPlayer(ULocalPlayer* LocalPlayer) const
{
	if (!LocalPlayer)
	{
		return;
	}

	LocalPlayer->SetPlatformUserId(PlatformUser);

	const FUniqueNetIdRepl NetId { GetAccountId(EModularOnlineRole::Default) };
	LocalPlayer->SetCachedUniqueNetId(NetId);

	// The player state carries the id to the server, so it has to agree with what we just cached.
	if (const auto PlayerController = LocalPlayer->GetPlayerController(nullptr); PlayerController && PlayerController->PlayerState)
	{
		PlayerController->PlayerState->SetUniqueId(NetId);
	}
}

FString UModularUserInfo::ToDebugString() const
{
	TStringBuilder<256> Builder;
	Builder.Appendf(TEXT("player %d, system user %d%s, state %s"),
		LocalPlayerIndex,
		PlatformUser.GetInternalId(),
		bIsGuest ? TEXT(" (guest)") : TEXT(""),
		*LexToString(State));

	for (const auto& Pair : RoleData)
	{
		Builder.Appendf(TEXT(", %s: %s"),
			*LexToString(Pair.Key),
			*UE::Online::ToLogString(Pair.Value.AccountId));
	}

	return Builder.ToString();
}
