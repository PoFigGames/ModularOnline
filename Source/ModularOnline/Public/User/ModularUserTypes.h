// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Core/ModularOnlineTypes.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "UObject/ObjectMacros.h"

#include "ModularUserTypes.generated.h"

namespace UE::Online
{
	enum class EUserPrivileges : uint8;
	enum class EPrivilegeResults : uint32;
}


/**
 * @enum EModularOnlinePrivilege
 *
 * @brief What a signed in account is allowed to do.
 */
UENUM(BlueprintType)
enum class EModularOnlinePrivilege : uint8
{
	/** Play at all, online or not. A failure here usually means the account does not own the game. */
	CanPlay,

	/** Play in online modes. */
	CanPlayOnline,

	/** Use text chat. */
	CanCommunicateViaText,

	/** Use voice chat. */
	CanCommunicateViaVoice,

	/** See content made by other players. */
	CanUseUserGeneratedContent,

	/** Play with people on other platforms. */
	CanUseCrossPlay,

	/** Not a privilege; the number of them. */
	Count					UMETA(Hidden)
};


/**
 * @enum EModularOnlinePrivilegeResult
 *
 * @brief Why a privilege is or is not granted, in terms a screen can put to the player.
 */
UENUM(BlueprintType)
enum class EModularOnlinePrivilegeResult : uint8
{
	/** Never asked for. */
	Unknown,

	/** Granted. */
	Available,

	/** Nobody is signed in on the services that were asked. */
	NotLoggedIn,

	/** The account does not own the game or the content. */
	LicenseInvalid,

	/** The game or the system has to be updated first. */
	VersionOutdated,

	/** No network. Worth asking again later. */
	NetworkUnavailable,

	/** Parental controls forbid it. */
	AgeRestricted,

	/** The account lacks a subscription or is of the wrong type. */
	AccountTypeRestricted,

	/** The account is restricted or banned by the service. */
	AccountUseRestricted,

	/** The platform refused for a reason it did not explain. */
	PlatformFailure
};


/**
 * @enum EModularUserState
 *
 * @brief How far a local player has got through signing in.
 */
UENUM(BlueprintType)
enum class EModularUserState : uint8
{
	/** Nothing has been attempted for this player yet. */
	Unknown,

	/** A login is running. */
	LoggingIn,

	/** Signed in as far as the platform, and no further. */
	LoggedInLocally,

	/** Signed in on the publisher backend as well, which only a project that has one ever reaches. */
	LoggedInOnline,

	/** The login was attempted and failed. The player has to do something before trying again. */
	LoginFailed
};


/**
 * @enum EModularLoginStep
 *
 * @brief The steps a login goes through, in the order they run.
 */
UENUM(BlueprintType)
enum class EModularLoginStep : uint8
{
	/** Sign in to the services of the machine: the console account, the Steam client. */
	PlatformLogin,

	/** Carry the platform identity over to the backend, so the player signs in once rather than twice. */
	TransferAuth,

	/** Sign in to the backend, either with the carried identity or on its own. */
	ServiceLogin,

	/** Ask the services whether this account may do what the caller asked for. */
	PrivilegeCheck,

	/** Not a step; the login is over. */
	Finished				UMETA(Hidden)
};


/**
 * @enum EModularStepPolicy
 *
 * @brief How much a login step matters on this platform.
 */
UENUM(BlueprintType)
enum class EModularStepPolicy : uint8
{
	/** The login fails if the step fails. */
	Required,

	/** The step is attempted, and a failure only means the next step has more to do. */
	Optional,

	/** The step does not apply here and is not attempted at all. */
	Skip
};


/**
 * @struct FModularLoginParams
 *
 * @brief What a caller asks for when signing a local player in.
 */
USTRUCT(BlueprintType)
struct FModularLoginParams
{
	GENERATED_BODY()

	/** Which local player this is: zero is the primary one, higher indices are local multiplayer. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	int32 LocalPlayerIndex { 0 };

	/** The system user to sign in. Left unset, the input device decides, and failing that the primary user. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FPlatformUserId PlatformUser { };

	/** The controller this player holds, which is how a console tells local players apart. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FInputDeviceId InputDevice { };

	/** How far the login has to get: CanPlay stops at the machine, CanPlayOnline goes to the backend. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	EModularOnlinePrivilege RequestedPrivilege { EModularOnlinePrivilege::CanPlay };

	/** Whether this player may end up a guest when the platform has no account for them. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	bool bAllowGuest { false };

	/** Whether the platform may put its own login screen in front of the player. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	bool bAllowLoginUI { true };
};


/**
 * @struct FModularPrivilegeConversions
 *
 * @brief Translation between the privileges of this plugin and those of the online services.
 */
struct FModularPrivilegeConversions
{
	static MODULARONLINE_API UE::Online::EUserPrivileges ToOnlineServices(EModularOnlinePrivilege Privilege);
	static MODULARONLINE_API EModularOnlinePrivilege FromOnlineServices(UE::Online::EUserPrivileges Privilege);
	static MODULARONLINE_API EModularOnlinePrivilegeResult FromOnlineServices(UE::Online::EUserPrivileges Privilege, UE::Online::EPrivilegeResults Results);
};
