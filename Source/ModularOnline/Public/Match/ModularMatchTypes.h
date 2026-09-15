// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Core/ModularOnlineTypes.h"
#include "Online/CoreOnline.h"
#include "UObject/ObjectMacros.h"
#include "UObject/PrimaryAssetId.h"

#include "ModularMatchTypes.generated.h"


/**
 * @enum EModularMatchOnlineMode
 *
 * @brief How much of the online stack a match uses.
 */
UENUM(BlueprintType)
enum class EModularMatchOnlineMode : uint8
{
	/** Nobody else can join: the game travels to the map and that is all. */
	Offline,

	/** Advertised on the local network only. */
	LAN,

	/** Published to the online services so that anyone allowed may find and join it. */
	Online
};


/**
 * @enum EModularMatchBackendKind
 *
 * @brief What carries a match: a lobby, or a registered session.
 */
UENUM(BlueprintType)
enum class EModularMatchBackendKind : uint8
{
	/** A player hosted group. The host has to be in it, and it goes away with them. */
	Lobbies,

	/** A record the services keep for the game. Nobody has to be in it, so a dedicated server can own one. */
	Sessions
};


/**
 * @enum EModularCrossPlayPolicy
 *
 * @brief Whether a match is published on one role or on both at once.
 */
UENUM(BlueprintType)
enum class EModularCrossPlayPolicy : uint8
{
	/** One publication, on the role that carries matches. A project shipping on a single backend. */
	SingleRole,

	/** Two publications of the same match, on the match role and on the companion role beside it. */
	BothRoles
};


/**
 * @enum EModularCrossPlayScope
 *
 * @brief How wide a search is allowed to look.
 */
UENUM(BlueprintType)
enum class EModularCrossPlayScope : uint8
{
	/** Every role the project publishes on. */
	Everywhere,

	/** Only the companion role, which is the one belonging to the machine the game runs on. */
	OwnPlatformOnly
};


/**
 * @enum EModularMatchJoinPolicy
 *
 * @brief Who may join a match once it exists.
 */
UENUM(BlueprintType)
enum class EModularMatchJoinPolicy : uint8
{
	/** Anyone may find it in a search, join it by its id, or be invited. */
	PublicAdvertised,

	/** It stays out of searches, and is joined by its id or by an invitation. */
	PublicNotAdvertised,

	/** Only an invited player may join. */
	InvitationOnly
};


/**
 * @enum EModularMatchLeaveReason
 *
 * @brief Why a player is no longer in a match.
 */
UENUM(BlueprintType)
enum class EModularMatchLeaveReason : uint8
{
	/** Nobody left: what a member event carries when somebody arrived instead. */
	None,

	/** They chose to leave. */
	Left,

	/** They lost their connection. */
	Disconnected,

	/** The host removed them from the match. */
	Kicked,

	/** The server refused the connection: a login it turned down, a wrong password, a full server. */
	Refused,

	/** The match itself went away. */
	Closed
};


/**
 * @struct FModularMatchHandle
 *
 * @brief What a found match is joined by.
 */
USTRUCT(BlueprintType)
struct FModularMatchHandle
{
	GENERATED_BODY()

	/** Human unreadable identifier, for logs and for telling two entries in a list apart. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString Id { };

	/** Set when this match lives in a lobby. */
	UE::Online::FLobbyId LobbyId { };

	/** Set when this match lives in a registered session. */
	UE::Online::FOnlineSessionId SessionId { };

	/** Which role's publication this handle names. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	EModularOnlineRole Role { EModularOnlineRole::Default };

	/** True when either identifier names something. */
	MODULARONLINE_API bool IsValid() const;
};


/**
 * @struct FModularMatchSettings
 *
 * @brief What a host asks for when it opens a match.
 */
USTRUCT(BlueprintType)
struct FModularMatchSettings
{
	GENERATED_BODY()

	/** Offline, on the local network, or published to the services. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	EModularMatchOnlineMode OnlineMode { EModularMatchOnlineMode::Online };

	/** Who may join once it exists. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	EModularMatchJoinPolicy JoinPolicy { EModularMatchJoinPolicy::PublicAdvertised };

	/** The map the match plays on. Has to be a primary asset the asset manager knows, or a package path. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline", meta = (AllowedTypes = "World"))
	FPrimaryAssetId MapId { };

	/** Friendly map name for presence and for a server browser, which need not be the package name. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	FString MapDisplayName { };

	/** What this match is called in somebody else's browser. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	FString Name { };

	/** How many players fit, the host included. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	int32 MaxPlayers { 4 };

	/** Whether players from other platforms may join. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	bool bAllowCrossPlay { true };

	/** Whether this match becomes what the player's friends see them playing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	bool bUsePresence { true };

	/** Extra attributes published with the match, for a browser to filter and display. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	TMap<FName, FString> Attributes { };

	/** Extra options appended to the travel URL. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	TMap<FString, FString> ExtraArgs { };

	/** Package name of the map, resolved through the asset manager. Empty when the map is unknown. */
	MODULARONLINE_API FString GetMapName() const;

	/** The URL the host travels to, options included. */
	MODULARONLINE_API FString ConstructTravelURL() const;

	/** Whether these settings can be hosted at all, with a reason when they cannot. */
	MODULARONLINE_API bool Validate(FText& OutError) const;
};


/**
 * @struct FModularMatchInfo
 *
 * @brief One match as a search answered it, in a shape a list widget can show.
 */
USTRUCT(BlueprintType)
struct FModularMatchInfo
{
	GENERATED_BODY()

	/** What to hand back to the join node. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FModularMatchHandle Handle { };

	/** Name the host published, usually built from their own. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString Name { };

	/** Friendly map name, as the host published it. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	FString MapName { };

	/** How many players fit. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	int32 MaxPlayers { 0 };

	/** How many of those places are still free. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	int32 OpenSlots { 0 };

	/** Whether the host said players from other platforms may join. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	bool bAllowsCrossPlay { false };

	/** Every attribute the host published, for a browser that wants more than the fields above. */
	UPROPERTY(BlueprintReadOnly, Category = "ModularOnline")
	TMap<FName, FString> Attributes { };
};


/**
 * @struct FModularMatchSearchParams
 *
 * @brief What a search asks for.
 */
USTRUCT(BlueprintType)
struct FModularMatchSearchParams
{
	GENERATED_BODY()

	/** Upper bound on results. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	int32 MaxResults { 20 };

	/** Only matches whose attributes equal these are returned. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	TMap<FName, FString> RequiredAttributes { };

	/** How wide to look when the project publishes on both roles. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ModularOnline")
	EModularCrossPlayScope CrossPlay { EModularCrossPlayScope::Everywhere };

};
