// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Core/ModularOnlineTypes.h"
#include "Engine/DeveloperSettings.h"
#include "Features/ModularFeatureTypes.h"
#include "GameplayTagContainer.h"
#include "Match/ModularMatchTypes.h"

#include "ModularOnlineSettings.generated.h"


/**
 * @class UModularOnlineSettings
 *
 * @brief Which services answer which question.
 *
 * The engine resolves the default and platform roles from its own OnlineServices keys; the service role
 * has no engine key, so it is named here.
 */
UCLASS(MinimalAPI, Config = Engine, DefaultConfig, meta = (DisplayName = "Modular Online"))
class UModularOnlineSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Provider taking the service role, spelled as EOnlineServices does. Empty resolves to the default. */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	FString ServiceProvider { };

	/** Which of the online roles carries matches. */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	EModularOnlineRole MatchRole { EModularOnlineRole::Service };

	/**
	 * The role carrying the second publication while the policy asks for both.
	 *
	 * Set equal to the match role, hosting refuses rather than publishing twice in the same services.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	EModularOnlineRole CompanionMatchRole { EModularOnlineRole::Platform };

	/** Which of the online roles answers about people: presence, friends, profiles. */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	EModularOnlineRole FeatureRole { EModularOnlineRole::Platform };

	/** Which of the online roles carries the store. */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	EModularOnlineRole StoreRole { EModularOnlineRole::Platform };

	/** Writes the provider and the component set of every role to the log once they are resolved. */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	bool bLogCapabilitiesOnStartup { true };

#pragma region UDeveloperSettings

	MODULARONLINE_API virtual FName GetCategoryName() const override;

	MODULARONLINE_API virtual void OverrideConfigSection(FString& OutSectionName) override;

#pragma endregion UDeveloperSettings
};


/**
 * @class UModularMatchBackendSettings
 *
 * @brief What carries a match, and what it publishes about itself.
 *
 * The schema ids and attribute names have to agree with the project's OnlineServices section: a service
 * refuses an attribute its schema does not name.
 */
UCLASS(MinimalAPI, Config = Engine, DefaultConfig, meta = (DisplayName = "Modular Online - Matches"))
class UModularMatchBackendSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** What carries a match here: a lobby, or a registered session. A provider lacking it says so. */
	UPROPERTY(Config, EditAnywhere, Category = "Matches")
	EModularMatchBackendKind MatchBackend { EModularMatchBackendKind::Lobbies };

	/** Schema a match is published under, as declared in the project's OnlineServices.Lobbies section. Unset hosts nothing. */
	UPROPERTY(Config, EditAnywhere, Category = "Matches")
	FString LobbySchemaId { };

	/** Schema a match published as a session is created with, as the project declared it. Unset hosts nothing. */
	UPROPERTY(Config, EditAnywhere, Category = "Matches")
	FString SessionSchemaId { };

	/** Attribute a match publishes its name under. Unset publishes no name. */
	UPROPERTY(Config, EditAnywhere, Category = "Matches")
	FName MatchNameAttribute { };

	/** Attribute a match publishes its map under, for a browser and for presence. Unset publishes no map. */
	UPROPERTY(Config, EditAnywhere, Category = "Matches")
	FName MatchMapAttribute { };

	/**
	 * How a provider marks an attribute as its own; an attribute named this way is never removed.
	 *
	 * A provider publishes its own bookkeeping beside a match, and taking it off has the whole update
	 * refused or blanks the fields a search filters on. Empty means nothing is reserved.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Matches")
	FString ReservedAttributePrefix { TEXT("__") };

	/**
	 * Which attribute says how many are already in a match.
	 *
	 * Steam listed no members of a lobby nobody has joined when this was checked on 2026-09-15. Empty shows
	 * every match as having room.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Matches")
	FName MatchMemberCountAttribute { };

#pragma region UDeveloperSettings

	MODULARONLINE_API virtual FName GetCategoryName() const override;

	MODULARONLINE_API virtual void OverrideConfigSection(FString& OutSectionName) override;

#pragma endregion UDeveloperSettings
};


/**
 * @class UModularCrossPlaySettings
 *
 * @brief Whether one match is published on both roles at once.
 *
 * Left off, nothing here is read and a match has a single publication.
 */
UCLASS(MinimalAPI, Config = Engine, DefaultConfig, meta = (DisplayName = "Modular Online - Cross Play"))
class UModularCrossPlaySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * Whether a match is published on one role or on both at once.
	 *
	 * BothRoles is the console shape: the companion role carries the lobby invitations and the overlay
	 * understand, the match role the session search and dedicated servers understand.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Cross Play")
	EModularCrossPlayPolicy CrossPlayPolicy { EModularCrossPlayPolicy::SingleRole };

	/** What carries the second publication. Lobbies, because that is what an overlay and an invite know. */
	UPROPERTY(Config, EditAnywhere, Category = "Cross Play")
	EModularMatchBackendKind CompanionMatchBackend { EModularMatchBackendKind::Lobbies };

	/** Attribute a match publishes the host's cross play answer under, so a browser can show it. */
	UPROPERTY(Config, EditAnywhere, Category = "Cross Play")
	FName MatchCrossPlayAttribute { };

	/** Attribute the second publication names the first by, so a browser does not show one match twice. */
	UPROPERTY(Config, EditAnywhere, Category = "Cross Play")
	FName MatchLinkAttribute { };

#pragma region UDeveloperSettings

	MODULARONLINE_API virtual FName GetCategoryName() const override;

	MODULARONLINE_API virtual void OverrideConfigSection(FString& OutSectionName) override;

#pragma endregion UDeveloperSettings
};


/**
 * @class UModularAccountSettings
 *
 * @brief Names under which a provider publishes things about an account.
 *
 * Only the display name is standard across the services; the rest is whatever key the provider chose.
 */
UCLASS(MinimalAPI, Config = Engine, DefaultConfig, meta = (DisplayName = "Modular Online - Accounts"))
class UModularAccountSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Attribute an avatar is published under, for providers not named below. Unset reads no avatars. */
	UPROPERTY(Config, EditAnywhere, Category = "Accounts")
	FName DefaultAvatarAttribute { };

	/** Avatar attribute per provider, keyed by its name as EOnlineServices spells it. */
	UPROPERTY(Config, EditAnywhere, Category = "Accounts")
	TMap<FString, FName> AvatarAttributeByProvider { };

	/** The attribute an avatar is published under on a provider, falling back to the default. */
	MODULARONLINE_API FName GetAvatarAttribute(const FString& ProviderName) const;

#pragma region UDeveloperSettings

	MODULARONLINE_API virtual FName GetCategoryName() const override;

	MODULARONLINE_API virtual void OverrideConfigSection(FString& OutSectionName) override;

#pragma endregion UDeveloperSettings
};


/**
 * @struct FModularPresenceState
 *
 * @brief One thing a player can be shown to be doing, described entirely in configuration.
 *
 * A new state is an entry here plus a tag, never a rebuild.
 */
USTRUCT(BlueprintType)
struct FModularPresenceState
{
	GENERATED_BODY()

	/** What game code asks for when it wants this state published. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ModularOnline")
	FGameplayTag State { };

	/**
	 * Whether a friend can get into what this state describes.
	 *
	 * Said here because it belongs to the state rather than to the moment: being in the front end is
	 * never joinable and being in a run may be, and which of the two a game is in is exactly what a state
	 * names. A state that says nothing leaves the answer unknown, which is how it was before anything
	 * said anything.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ModularOnline")
	EModularPresenceJoinability Joinability { EModularPresenceJoinability::Unknown };

	/**
	 * The status the provider renders, in that provider's own terms.
	 *
	 * On Steam this is a rich presence localisation token the application has declared; a token it has
	 * not declared showed as nothing at all when this was checked on 2026-09-15, which is why this is a
	 * name and not a sentence.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ModularOnline")
	FString Status { };

	/**
	 * Keys published beside the status, and what each of them says.
	 *
	 * A value may name one the publisher supplied by wrapping it in braces; one nobody supplied leaves
	 * its key unpublished.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ModularOnline")
	TMap<FName, FString> Properties { };
};


/**
 * @class UModularPresenceSettings
 *
 * @brief What the player's friends are told they are doing.
 *
 * The plugin ships no states and publishes none by itself: the game names one by tag, and this says
 * what that turns into.
 */
UCLASS(MinimalAPI, Config = Engine, DefaultConfig, meta = (DisplayName = "Modular Online - Presence"))
class UModularPresenceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Every state this project can publish. Asking for one not described here publishes nothing. */
	UPROPERTY(Config, EditAnywhere, Category = "Presence")
	TArray<FModularPresenceState> States { };

	/** The state with this tag, or nothing when the project never described it. */
	MODULARONLINE_API const FModularPresenceState* FindState(FGameplayTag State) const;

#pragma region UDeveloperSettings

	MODULARONLINE_API virtual FName GetCategoryName() const override;

	MODULARONLINE_API virtual void OverrideConfigSection(FString& OutSectionName) override;

#pragma endregion UDeveloperSettings
};


/**
 * @class UModularServerSettings
 *
 * @brief Signing in a machine that has no player at it.
 *
 * A dedicated server signs in as itself and hosts with the account the services give it. There is no
 * standard name for that credential, so the project names it per provider.
 */
UCLASS(MinimalAPI, Config = Engine, DefaultConfig, meta = (DisplayName = "Modular Online - Dedicated Server"))
class UModularServerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * Credentials a dedicated server signs in with, for providers not named below.
	 *
	 * Signing in a machine rather than a person is a provider specific credential with no standard name.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Dedicated Server")
	FName DefaultServerCredentialsType { TEXT("Auto") };

	/** Server credentials per provider, keyed by its name as EOnlineServices spells it. */
	UPROPERTY(Config, EditAnywhere, Category = "Dedicated Server")
	TMap<FString, FName> ServerCredentialsTypeByProvider { };

	/** The credentials a dedicated server signs in with on a provider, falling back to the default. */
	MODULARONLINE_API FName GetServerCredentialsType(const FString& ProviderName) const;

#pragma region UDeveloperSettings

	MODULARONLINE_API virtual FName GetCategoryName() const override;

	MODULARONLINE_API virtual void OverrideConfigSection(FString& OutSectionName) override;

#pragma endregion UDeveloperSettings
};
