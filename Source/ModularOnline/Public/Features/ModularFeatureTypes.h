// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Core/ModularOnlineTypes.h"
#include "Internationalization/Text.h"
#include "Misc/DateTime.h"
#include "UObject/ObjectMacros.h"

#include "ModularFeatureTypes.generated.h"


/**
 * @enum EModularPresenceJoinability
 *
 * @brief Whether somebody's friends can get into what they are doing.
 */
UENUM(BlueprintType)
enum class EModularPresenceJoinability : uint8
{
	/** Nothing has been said, which is how a provider treats somebody who is not in anything joinable. */
	Unknown,

	/** Anybody who can see them can join. */
	Public,

	/** Only their friends. */
	FriendsOnly,

	/** Only somebody they invited. */
	InviteOnly,

	/** Nobody, for now. */
	Private
};


/**
 * @enum EModularPresenceStatus
 *
 * @brief Whether somebody is around, in the words their friends see.
 */
UENUM(BlueprintType)
enum class EModularPresenceStatus : uint8
{
	/** Not signed in anywhere. */
	Offline,

	/** Signed in and at the machine. */
	Online,

	/** Signed in, but idle. */
	Away,

	/** Idle long enough that the platform says so separately. */
	ExtendedAway,

	/** Signed in and asking not to be disturbed. */
	DoNotDisturb,

	/** The services did not say. */
	Unknown
};


/**
 * @enum EModularRelationship
 *
 * @brief What one account is to another.
 */
UENUM(BlueprintType)
enum class EModularRelationship : uint8
{
	/** They are friends. */
	Friend,

	/** They are not, and nothing is pending. */
	NotFriend,

	/** The local player asked, and is waiting. */
	InviteSent,

	/** The other player asked, and is waiting. */
	InviteReceived,

	/** The local player blocked them. */
	Blocked
};


/**
 * @struct FModularPresence
 *
 * @brief What one account is shown to be doing.
 */
USTRUCT(BlueprintType)
struct FModularPresence
{
	GENERATED_BODY()

	/** Whose presence this is. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FModularAccountHandle AccountId { };

	/** Whether they are around. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	EModularPresenceStatus Status { EModularPresenceStatus::Unknown };

	/** The line the platform shows about them, which the game usually sets itself. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString StatusText { };

	/** The longer line some platforms show beside it. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString RichPresence { };

	/** True when their match would take somebody who asked to join right now. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	bool bIsJoinable { false };

	/** Everything else the game published about them. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	TMap<FString, FString> Properties { };
};


/**
 * @struct FModularFriend
 *
 * @brief One entry of the friends list.
 */
USTRUCT(BlueprintType)
struct FModularFriend
{
	GENERATED_BODY()

	/** Their account. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FModularAccountHandle AccountId { };

	/** The name the platform shows for them. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString DisplayName { };

	/** The name the local player gave them, where the platform allows one. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString Nickname { };

	/** What they are to the local player. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	EModularRelationship Relationship { EModularRelationship::NotFriend };
};


/**
 * @struct FModularUserProfile
 *
 * @brief What is known about an account other than the local one.
 */
USTRUCT(BlueprintType)
struct FModularUserProfile
{
	GENERATED_BODY()

	/** Whose profile this is. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FModularAccountHandle AccountId { };

	/** The name the platform shows for them. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString DisplayName { };

	/** Where their picture can be fetched from, empty where the platform publishes none. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString AvatarUrl { };
};


/**
 * @enum EModularStatKind
 *
 * @brief What sort of number a statistic is.
 */
UENUM(BlueprintType)
enum class EModularStatKind : uint8
{
	/** A whole number: kills, matches played, seconds survived. */
	Integer,

	/** A fractional one: accuracy, distance. */
	Float,

	/** A flag. */
	Boolean,

	/** Text, which some services allow and most do not sort by. */
	Text
};


/**
 * @struct FModularStatValue
 *
 * @brief One statistic, of whichever sort it is.
 */
USTRUCT(BlueprintType)
struct FModularStatValue
{
	GENERATED_BODY()

	/** Which of the fields below means anything. */
	UPROPERTY(BlueprintReadWrite, Category = "ModularOnline")
	EModularStatKind Kind { EModularStatKind::Integer };

	/** Set when the statistic is a whole number. */
	UPROPERTY(BlueprintReadWrite, Category = "ModularOnline")
	int64 IntValue { 0 };

	/** Set when the statistic is fractional. */
	UPROPERTY(BlueprintReadWrite, Category = "ModularOnline")
	double FloatValue { 0.0 };

	/** Set when the statistic is a flag. */
	UPROPERTY(BlueprintReadWrite, Category = "ModularOnline")
	bool bBoolValue { false };

	/** Set when the statistic is text. */
	UPROPERTY(BlueprintReadWrite, Category = "ModularOnline")
	FString TextValue { };
};


/**
 * @struct FModularAchievement
 *
 * @brief One achievement, as a screen shows it.
 */
USTRUCT(BlueprintType)
struct FModularAchievement
{
	GENERATED_BODY()

	/** What the services call it. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString Id { };

	/** Its name once it is earned. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FText UnlockedName { };

	/** What it says once it is earned. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FText UnlockedDescription { };

	/** Its name while it is not. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FText LockedName { };

	/** What it says while it is not. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FText LockedDescription { };

	/** Where its picture is, where the services publish one. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString IconUrl { };

	/** True for an achievement that is not shown until it is earned. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	bool bIsHidden { false };

	/** True once this player has it. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	bool bIsUnlocked { false };

	/** How far along they are, from 0 to 1. A progress bar reads this; 1 is what unlocked means. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	float Progress { 0.0f };

	/** When they got it, meaningful only once they have. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FDateTime UnlockTime { };
};


/**
 * @struct FModularLeaderboardEntry
 *
 * @brief One line of a ranked table.
 */
USTRUCT(BlueprintType)
struct FModularLeaderboardEntry
{
	GENERATED_BODY()

	/** Whose line it is. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FModularAccountHandle AccountId { };

	/** Where they stand, counting from one. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	int32 Rank { 0 };

	/** What put them there. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	int64 Score { 0 };
};


/**
 * @struct FModularStoreOffer
 *
 * @brief One thing the platform store sells, as a screen shows it.
 */
USTRUCT(BlueprintType)
struct FModularStoreOffer
{
	GENERATED_BODY()

	/** What the store calls it. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString OfferId { };

	/** Its name, in the player's language. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FText Title { };

	/** The short line under that name. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FText Description { };

	/** The long one, for a page rather than a tile. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FText LongDescription { };

	/** What it costs now, written the way the store writes money. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FText FormattedPrice { };

	/** What it costs off sale, written the same way; equal to the above when nothing is discounted. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FText FormattedRegularPrice { };

	/** The currency those two are in. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString CurrencyCode { };

	/** What it costs now as a number, for sorting and comparing only. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	int64 Price { 0 };

	/** What it costs off sale as a number, for the same. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	int64 RegularPrice { 0 };

	/** How many of the digits of those numbers are decimals. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	int32 PriceDecimalPoint { 0 };

	/** How many times it may be bought, negative where the store sets no limit. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	int32 PurchaseLimit { -1 };

	/** True when the store said from when it sells. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	bool bHasReleaseDate { false };

	/** From when, meaningful only when it said so. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FDateTime ReleaseDate { };

	/** True when the store said until when this price holds. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	bool bHasExpirationDate { false };

	/** Until when, meaningful only when it said so. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FDateTime ExpirationDate { };

	/** Everything else this particular store publishes about the offer. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	TMap<FString, FString> AdditionalData { };
};


/**
 * @struct FModularEntitlement
 *
 * @brief Something the account owns, whether or not the game has acted on it yet.
 */
USTRUCT(BlueprintType)
struct FModularEntitlement
{
	GENERATED_BODY()

	/** What the store calls this particular grant. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString EntitlementId { };

	/** What sort of grant it is, as the store names the sort. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString EntitlementType { };

	/** What was granted. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString ProductId { };

	/** True once it has been consumed; a consumable is bought again after this. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	bool bRedeemed { false };

	/** How many of it the account holds. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	int32 Quantity { 0 };

	/** True when the store said when it was acquired. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	bool bHasAcquiredDate { false };

	/** When, meaningful only when it said so. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FDateTime AcquiredDate { };

	/** True when the grant runs out, which subscriptions do. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	bool bHasExpiryDate { false };

	/** When it runs out, meaningful only when it does. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FDateTime ExpiryDate { };
};


/**
 * @struct FModularPurchaseLine
 *
 * @brief One line of a checkout: what to buy and how many.
 */
USTRUCT(BlueprintType)
struct FModularPurchaseLine
{
	GENERATED_BODY()

	/** What to buy. */
	UPROPERTY(BlueprintReadWrite, Category = "ModularOnline")
	FString OfferId { };

	/** How many of it. */
	UPROPERTY(BlueprintReadWrite, Category = "ModularOnline")
	int32 Quantity { 1 };
};
