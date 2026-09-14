// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Internationalization/Text.h"
#include "Online/CoreOnline.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"

#include "ModularOnlineTypes.generated.h"

namespace UE::Online
{
	class FOnlineError;
}



/**
 * @struct FModularAccountHandle
 *
 * @brief Which account something is about.
 */
USTRUCT(BlueprintType)
struct FModularAccountHandle
{
	GENERATED_BODY()

	/** How the provider writes this account down, for logs and for telling two entries in a list apart. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString Id { };

	/** What the services are actually asked with. */
	UE::Online::FAccountId AccountId { };

	/** True when this names an account the services can be asked about. */
	bool IsValid() const
	{
		return AccountId.IsValid();
	}

	bool operator==(const FModularAccountHandle& Other) const
	{
		return AccountId == Other.AccountId;
	}
};


/** Names an account the way the services do, with the string only for showing and for logs. */
inline FModularAccountHandle MakeModularAccount(const UE::Online::FAccountId& AccountId)
{
	return FModularAccountHandle { UE::Online::ToString(AccountId), AccountId };
}

namespace PoFigGames::Online
{
	/** Name of a reflected enum value, for logs and debug strings. */
	template <typename EnumType> requires requires { StaticEnum<EnumType>(); }
	FString LexToString(const EnumType Value)
	{
		return StaticEnum<EnumType>()->GetNameStringByValue(static_cast<int64>(Value));
	}
}


/**
 * @enum EModularOnlineRole
 *
 * @brief Which of the online services a call is addressed to.
 */
UENUM(BlueprintType)
enum class EModularOnlineRole : uint8
{
	/** Whatever the project runs on, as configured under OnlineServices.DefaultServices. */
	Default,

	/** The services of the machine the game runs on. Falls back to Default when there is no separate one. */
	Platform,

	/** The publisher backend shared across platforms. Falls back to Default when the project has none. */
	Service
};


/**
 * @enum EModularOnlineErrorCategory
 *
 * @brief Why an operation did not succeed, coarse enough for a screen to branch on.
 */
UENUM(BlueprintType)
enum class EModularOnlineErrorCategory : uint8
{
	/** The operation succeeded. */
	None,

	/** The current provider does not implement this component at all. Nothing the player can do. */
	NotSupported,

	/** No account is signed in on the addressed services. */
	NotLoggedIn,

	/** The backend is unreachable. Worth retrying later. */
	NoConnection,

	/** The account exists but is not allowed to do this. */
	AccessDenied,

	/** The request does not fit the state the game is in, or its parameters were rejected. */
	InvalidState,

	/** The caller cancelled the operation, or the world it belonged to went away. */
	Cancelled,

	/** The services did not answer in time. */
	TimedOut,

	/** Anything the mapping above does not recognise. */
	Unknown
};


/**
 * @struct FModularOnlineResult
 *
 * @brief How an operation ended, in a shape Blueprint can read.
 */
USTRUCT(BlueprintType)
struct FModularOnlineResult
{
	GENERATED_BODY()

	/** True when the operation did what was asked. The error fields are empty in that case. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	bool bWasSuccessful { true };

	/** Coarse reason for a failure, for a screen to branch on. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	EModularOnlineErrorCategory Category { EModularOnlineErrorCategory::None };

	/** Exact error id of the online services, for logs and support. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString ErrorId { };

	/** Text to show the player. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FText ErrorText { };

	/** The component that was missing, when the failure was NotSupported. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FGameplayTag MissingFeature { };

	/** An operation that did what was asked. */
	static MODULARONLINE_API FModularOnlineResult Success();

	/** The provider of the addressed role does not implement the component this call needed. */
	static MODULARONLINE_API FModularOnlineResult NotSupported(FGameplayTag Feature);

	/** Translates an answer of the online services, mapping the error onto a category. */
	static MODULARONLINE_API FModularOnlineResult FromOnlineError(const UE::Online::FOnlineError& InError);

	/** Debug string, error id included. */
	MODULARONLINE_API FString ToLogString() const;
};
