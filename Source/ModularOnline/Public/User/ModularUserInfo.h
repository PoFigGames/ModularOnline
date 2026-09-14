// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Core/ModularOnlineTypes.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Online/CoreOnline.h"
#include "UObject/Object.h"
#include "User/ModularUserTypes.h"

#include "ModularUserInfo.generated.h"

class ULocalPlayer;


/**
 * @class UModularUserInfo
 *
 * @brief One local player, and who they are on each of the online roles.
 */
UCLASS(MinimalAPI)
class UModularUserInfo : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * @struct FRoleData
	 *
	 * @brief Who this player is on one of the roles, and what that role last said they may do.
	 */
	struct FRoleData
	{
		/** The account signed in on this role, invalid while nobody is. */
		UE::Online::FAccountId AccountId { };

		/** Display name of that account, refreshed whenever the account changes. */
		FString Nickname { };

		/** Avatar of that account, where the services publish one. */
		FString AvatarUrl { };

		/** Last answer to every privilege that was asked about on this role. */
		TMap<EModularOnlinePrivilege, EModularOnlinePrivilegeResult> Privileges { };
	};

	/** Index of this player in the game instance: zero is the primary player. */
	int32 LocalPlayerIndex { INDEX_NONE };

	/** The system user behind this player. */
	FPlatformUserId PlatformUser { PLATFORMUSERID_NONE };

	/** The controller this player holds, which is how local players are told apart. */
	FInputDeviceId PrimaryInputDevice { INPUTDEVICEID_NONE };

	/** How far this player got through signing in. */
	EModularUserState State { EModularUserState::Unknown };

	/** True when this player has no account of their own and plays as a guest of the primary one. */
	bool bIsGuest { false };

	/** Signed in somewhere, locally or online. */
	MODULARONLINE_API bool IsLoggedIn() const;

	/** A login is running for this player right now. */
	MODULARONLINE_API bool IsLoggingIn() const;

	/** Whether this player has an account with the online services at all. */
	MODULARONLINE_API bool IsSignedInOnline() const;

	/** The account on a role, invalid when nobody is signed in there. */
	MODULARONLINE_API UE::Online::FAccountId GetAccountId(EModularOnlineRole Role = EModularOnlineRole::Default) const;

	/** Display name on a role, empty when unknown. */
	MODULARONLINE_API FString GetNickname(EModularOnlineRole Role = EModularOnlineRole::Default) const;

	/** Avatar on a role, empty when the services publish none. */
	MODULARONLINE_API FString GetAvatarUrl(EModularOnlineRole Role = EModularOnlineRole::Default) const;

	/** Last answer about a privilege, Unknown when it was never asked for. */
	MODULARONLINE_API EModularOnlinePrivilegeResult GetPrivilege(EModularOnlinePrivilege Privilege, EModularOnlineRole Role = EModularOnlineRole::Default) const;

	/** Tells a local player who it belongs to, so that the engine and the player state agree with us. */
	MODULARONLINE_API void ApplyToLocalPlayer(ULocalPlayer* LocalPlayer) const;

	/** Debug line: index, system user, guest flag, state and the account of every role. */
	MODULARONLINE_API FString ToDebugString() const;

	/** Identity per role. Only the roles this player was actually signed in on are present. */
	MODULARONLINE_API FRoleData& FindOrAddRoleData(EModularOnlineRole Role);
	MODULARONLINE_API const FRoleData* FindRoleData(EModularOnlineRole Role) const;

private:
	TMap<EModularOnlineRole, FRoleData> RoleData { };
};
