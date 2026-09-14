// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Core/ModularOnlineTypes.h"
#include "Match/ModularMatchBackend.h"
#include "Match/ModularMatchTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "ModularMatchSubsystem.generated.h"

class UModularOnlineSubsystem;
class UModularUserInfo;
class UModularUserSubsystem;

namespace UE::Online
{
	struct FLobbyInvitationAdded;
	struct FLobbyJoined;
	struct FLobbyLeft;
	struct FLobbyMemberJoined;
	struct FLobbyMemberLeft;
	struct FSessionInviteReceived;
	struct FSessionLeft;
	struct FUILobbyJoinRequested;
	struct FUISessionJoinRequested;
}


/** Events of this layer come in pairs: a native one for game code, and a dynamic twin for Blueprint. */
DECLARE_MULTICAST_DELEGATE_OneParam(FModularMatchResultEvent, const FModularOnlineResult& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularMatchResultDynamic, const FModularOnlineResult&, Result);

/** The local player is no longer in a match, and this is why. */
DECLARE_MULTICAST_DELEGATE_OneParam(FModularMatchLeftEvent, EModularMatchLeaveReason /*Reason*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularMatchLeftDynamic, EModularMatchLeaveReason, Reason);

/** Somebody joined or left the match the local player is in. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FModularMatchMemberEvent, const FModularAccountHandle& /*AccountId*/, EModularMatchLeaveReason /*Reason*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularMatchMemberDynamic, const FModularAccountHandle&, AccountId, EModularMatchLeaveReason, Reason);

/** The travel URL a joining client is about to use, offered for the game to add to. */
DECLARE_MULTICAST_DELEGATE_OneParam(FModularPreClientTravelEvent, FString& /*URL*/);

/** A match arrived from outside the game: an invitation, or a join asked for in the system overlay. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FModularMatchOfferedEvent, const FModularMatchInfo& /*Match*/, const FModularAccountHandle& /*SenderId*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularMatchOfferedDynamic, const FModularMatchInfo&, Match, const FModularAccountHandle&, SenderId);


/**
 * @class UModularMatchSubsystem
 *
 * @brief One way to host, find and join a match, whatever carries it underneath.
 */
UCLASS(MinimalAPI, BlueprintType)
class UModularMatchSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** A match was opened, or the attempt failed. */
	FModularMatchResultEvent OnMatchCreated { };

	/** A match was joined, or the attempt failed. Fires before the client travels. */
	FModularMatchResultEvent OnMatchJoined { };

	/** The local player left, was kicked, or the match closed under them. */
	FModularMatchLeftEvent OnMatchLeft { };

	/** Somebody else joined the match. */
	FModularMatchMemberEvent OnMemberJoined { };

	/** Somebody else left the match, and why. */
	FModularMatchMemberEvent OnMemberLeft { };

	/** An invitation arrived. */
	FModularMatchOfferedEvent OnInvitationReceived { };

	/** The player asked to join a match from the system overlay rather than from the game. */
	FModularMatchOfferedEvent OnJoinRequestedFromOverlay { };

	/** Fired with the address a joining client is about to travel to, so the game may add to it. */
	FModularPreClientTravelEvent OnPreClientTravel { };

	UModularMatchSubsystem() { }

#pragma region UGameInstanceSubsystem

	MODULARONLINE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	MODULARONLINE_API virtual void Deinitialize() override;

	MODULARONLINE_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

#pragma endregion UGameInstanceSubsystem

	/** Opens a match and travels to its map. */
	MODULARONLINE_API virtual bool HostMatch(int32 LocalPlayerIndex, const FModularMatchSettings& Settings, FModularMatchOperationDelegate OnComplete = FModularMatchOperationDelegate { });

	/** Looks for matches to join. */
	MODULARONLINE_API virtual bool FindMatches(int32 LocalPlayerIndex, const FModularMatchSearchParams& Params, FModularMatchSearchDelegate OnComplete);

	/** Joins a match and travels to it. */
	MODULARONLINE_API virtual bool JoinMatch(int32 LocalPlayerIndex, const FModularMatchHandle& Match, FModularMatchOperationDelegate OnComplete = FModularMatchOperationDelegate { });

	/** Leaves the match, which is what returning to the main menu does. */
	MODULARONLINE_API virtual bool LeaveMatch(int32 LocalPlayerIndex, FModularMatchOperationDelegate OnComplete = FModularMatchOperationDelegate { });

	/** Invites somebody to the match the local player is in. */
	MODULARONLINE_API virtual bool InviteToMatch(int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId, FModularMatchOperationDelegate OnComplete = FModularMatchOperationDelegate { });

	/** Removes somebody from the match. Only the host may. */
	MODULARONLINE_API virtual bool KickMember(int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId, FModularMatchOperationDelegate OnComplete = FModularMatchOperationDelegate { });

	/** Republishes what the match says about itself. */
	MODULARONLINE_API virtual bool UpdateMatchSettings(int32 LocalPlayerIndex, const FModularMatchSettings& Settings, FModularMatchOperationDelegate OnComplete = FModularMatchOperationDelegate { });

	/** Moves a match that is already running to another map. */
	MODULARONLINE_API virtual bool TravelMatchTo(int32 LocalPlayerIndex, const FModularMatchSettings& Settings, FModularMatchOperationDelegate OnComplete = FModularMatchOperationDelegate { });

	/** The match this local player is in, if any. */
	MODULARONLINE_API bool GetCurrentMatch(int32 LocalPlayerIndex, FModularMatchHandle& OutMatch) const;

	/** Which role carries matches on this platform. */
	MODULARONLINE_API EModularOnlineRole GetMatchRole() const;

	/** The role carrying the second publication, which is where invitations and the overlay look. */
	MODULARONLINE_API EModularOnlineRole GetCompanionMatchRole() const;

	/** True when this project publishes a match on both roles and both of them can carry one. */
	MODULARONLINE_API bool IsPublishingOnBothRoles() const;

	/** Whether this local player's account may play with people on other platforms. */
	MODULARONLINE_API bool CanPlayerCrossPlay(int32 LocalPlayerIndex) const;

	/** Whether this platform lets the player turn cross play off themselves. */
	MODULARONLINE_API bool IsCrossPlayOptional() const;

	/** Whether a match about to be opened is published on both roles. */
	static MODULARONLINE_API bool ShouldMirrorMatch(bool bPublishesOnBothRoles, bool bHostAllowsCrossPlay, bool bAccountMayCrossPlay);

	/** How wide a search actually looks, given what was asked for and what is allowed. */
	static MODULARONLINE_API EModularCrossPlayScope ResolveSearchScope(EModularCrossPlayScope Asked, bool bPublishesOnBothRoles, bool bAccountMayCrossPlay);

	/** True when some backend can carry a match at all; false in a build with no online services. */
	MODULARONLINE_API bool CanHostOnlineMatches() const;

protected:

	/** Whatever carries matches here. Empty when no provider can. */
	mutable TSharedPtr<PoFigGames::Online::IModularMatchBackend> Backend { nullptr };

	/** What carries the second publication, while the project asked for one and the role can carry it. */
	mutable TSharedPtr<PoFigGames::Online::IModularMatchBackend> CompanionBackend { nullptr };

	/** The URL the host travels to once the services accepted the match. */
	FString PendingTravelURL { };

	/** Subscriptions to the lobby events of the role that carries matches. */
	mutable TArray<UE::Online::FOnlineEventDelegateHandle> MatchEventHandles { };

	/** True once the subscriptions above are in place. */
	mutable bool bEventsBound { false };

	/** Subscriptions to the engine's own failures, which are not events of the services. */
	FDelegateHandle NetworkFailureHandle { };
	FDelegateHandle TravelFailureHandle { };

	/** True once the configured backend was looked for, whether or not it was found. */
	mutable bool bBackendChosen { false };

	/** The services instance the backends above were chosen against. */
	mutable FName ChosenForInstance { };

	/** Whether the instance name above has been read at all, as opposed to being empty because it is. */
	mutable bool bInstanceKnown { false };

	/** True once the companion backend was looked for, whether or not it was found. */
	mutable bool bCompanionChosen { false };

	/** True once a search was narrowed for want of the cross play privilege, so the log says it once. */
	bool bNarrowingLogged { false };

	/** True on a dedicated server, which hosts without a local player. */
	bool bIsDedicatedServer { false };

	/** The same events, for Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Match", meta = (DisplayName = "On Match Created"))
	FModularMatchResultDynamic K2_OnMatchCreated { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Match", meta = (DisplayName = "On Match Joined"))
	FModularMatchResultDynamic K2_OnMatchJoined { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Match", meta = (DisplayName = "On Match Left"))
	FModularMatchLeftDynamic K2_OnMatchLeft { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Match", meta = (DisplayName = "On Member Joined"))
	FModularMatchMemberDynamic K2_OnMemberJoined { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Match", meta = (DisplayName = "On Member Left"))
	FModularMatchMemberDynamic K2_OnMemberLeft { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Match", meta = (DisplayName = "On Invitation Received"))
	FModularMatchOfferedDynamic K2_OnInvitationReceived { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Match", meta = (DisplayName = "On Join Requested From Overlay"))
	FModularMatchOfferedDynamic K2_OnJoinRequestedFromOverlay { };

	/** Picks the backend for the current provider, or leaves it empty when none fits. */
	MODULARONLINE_API virtual void SelectBackend() const;

	/** The refusal every call answers with while the configured backend is not available here. */
	MODULARONLINE_API FModularOnlineResult GetMissingBackendResult() const;

	/** Builds a backend of the asked kind and reports whether the role can actually carry it. */
	MODULARONLINE_API TSharedPtr<PoFigGames::Online::IModularMatchBackend> MakeBackend(EModularMatchBackendKind Kind, EModularOnlineRole Role) const;

	/** Fills in who is asking and of which services, or returns false when nobody can ask. */
	MODULARONLINE_API bool BuildContext(int32 LocalPlayerIndex, EModularOnlineRole Role, PoFigGames::Online::FModularMatchContext& OutContext) const;

	/** The backend that owns a handle, which is the one the match was found through. */
	MODULARONLINE_API TSharedPtr<PoFigGames::Online::IModularMatchBackend> GetBackendForRole(EModularOnlineRole Role) const;

	/** The context for addressing one publication about one other player, or the refusal owed instead. */
	MODULARONLINE_API bool AddressTarget(int32 LocalPlayerIndex, EModularOnlineRole Role, const FModularAccountHandle& TargetAccountId,
		PoFigGames::Online::FModularMatchContext& OutContext, const FModularMatchOperationDelegate& OnComplete) const;

	/** Tells the game that the platform's overlay asked to join a match, whichever backend described it. */
	MODULARONLINE_API void AnnounceJoinRequestedFromOverlay(FModularMatchInfo& Match, EModularOnlineRole Role, const UE::Online::FAccountId& Asker);

	/** Finds whichever publication this local player is actually in. */
	MODULARONLINE_API bool FindActiveMatch(int32 LocalPlayerIndex, EModularOnlineRole& OutRole, PoFigGames::Online::FModularMatchContext& OutContext, FModularMatchHandle& OutMatch) const;

	/** The settings as they are published, with what this layer says about the match written in. */
	MODULARONLINE_API FModularMatchSettings DescribeForPublication(const FModularMatchSettings& Settings, bool bCrossPlay, const FString& LinkedMatchId) const;

	/** Announces a hosted match, holds places if the project asked for them, and travels. */
	MODULARONLINE_API void CompleteHostedMatch(const FModularOnlineResult& Result, FModularMatchOperationDelegate OnComplete);

	/** Folds a second role's results into the first's, dropping the ones that are the same match twice. */
	MODULARONLINE_API void MergeMatches(TArray<FModularMatchInfo>& Matches, const TArray<FModularMatchInfo>& Companion, int32 MaxResults) const;

	/** Reads back what this layer wrote into a found match's attributes. */
	MODULARONLINE_API void DescribeFoundMatch(FModularMatchInfo& Match, EModularOnlineRole Role) const;

	/** Subscribes to what the services say about matches. Done on first use, like the contexts. */
	MODULARONLINE_API void BindMatchEvents();

	/** Travels the listen server to the map of the match it just opened. */
	MODULARONLINE_API void TravelToHostedMap();

	/** Travels a joining client to the host, resolving the address through the services. */
	MODULARONLINE_API void TravelToJoinedMatch(int32 LocalPlayerIndex);

	/** Events of the lobbies. */
	MODULARONLINE_API void HandleLobbyJoined(const UE::Online::FLobbyJoined& EventParameters);

	MODULARONLINE_API void HandleLobbyLeft(const UE::Online::FLobbyLeft& EventParameters);

	MODULARONLINE_API void HandleLobbyMemberJoined(const UE::Online::FLobbyMemberJoined& EventParameters);

	MODULARONLINE_API void HandleLobbyMemberLeft(const UE::Online::FLobbyMemberLeft& EventParameters);

	MODULARONLINE_API void HandleLobbyInvitation(const UE::Online::FLobbyInvitationAdded& EventParameters, EModularOnlineRole Role);

	MODULARONLINE_API void HandleLobbyJoinRequested(const UE::Online::FUILobbyJoinRequested& EventParameters, EModularOnlineRole Role);

	/**
	 * The engine could not keep, or could not make, the connection a match is played over.
	 *
	 * The online services hear nothing about this: a server that crashes, times out or refuses a player
	 * ends the connection, not the lobby, so without this the game is left standing in a world nobody is
	 * talking to any more.
	 */
	MODULARONLINE_API void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	/** Travelling to a match failed, which leaves the player in the same place a lost connection does. */
	MODULARONLINE_API void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	/** Somebody left a session, which the services do not otherwise tell the game about. */
	MODULARONLINE_API void HandleSessionLeft(const UE::Online::FSessionLeft& EventParameters);

	/** An invitation to a session, which arrives as an id the invitation itself has to be read from. */
	MODULARONLINE_API void HandleSessionInvite(const UE::Online::FSessionInviteReceived& EventParameters, EModularOnlineRole Role);

	/** A join asked for in the platform's overlay, when matches are carried in sessions. */
	MODULARONLINE_API void HandleSessionJoinRequested(const UE::Online::FUISessionJoinRequested& EventParameters, EModularOnlineRole Role);

	/** The user layer, which knows who the local players are. */
	MODULARONLINE_API UModularUserSubsystem* GetUsers() const;

	/** The online layer, which owns the contexts. */
	MODULARONLINE_API UModularOnlineSubsystem* GetOnline() const;

	/** What Blueprint may ask of this subsystem. */
	/** Leaves the match, for a graph that cannot pass a completion delegate. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Match", meta = (DisplayName = "Leave Match"))
	MODULARONLINE_API bool K2_LeaveMatch(int32 LocalPlayerIndex = 0) { return LeaveMatch(LocalPlayerIndex); }

	/** Invites somebody to the match the local player is in. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Match", meta = (DisplayName = "Invite To Match"))
	MODULARONLINE_API bool K2_InviteToMatch(const FModularAccountHandle& TargetAccountId, int32 LocalPlayerIndex = 0) { return InviteToMatch(LocalPlayerIndex, TargetAccountId); }

	/** Removes somebody from the match. Only the host may. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Match", meta = (DisplayName = "Kick From Match"))
	MODULARONLINE_API bool K2_KickMember(const FModularAccountHandle& TargetAccountId, int32 LocalPlayerIndex = 0) { return KickMember(LocalPlayerIndex, TargetAccountId); }

	/** Republishes what the match says about itself. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Match", meta = (DisplayName = "Update Match Settings"))
	MODULARONLINE_API bool K2_UpdateMatchSettings(const FModularMatchSettings& Settings, int32 LocalPlayerIndex = 0) { return UpdateMatchSettings(LocalPlayerIndex, Settings); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Match", meta = (DisplayName = "Get Current Match"))
	bool K2_GetCurrentMatch(int32 LocalPlayerIndex, FModularMatchHandle& OutMatch) const { return GetCurrentMatch(LocalPlayerIndex, OutMatch); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Match", meta = (DisplayName = "Can Host Online Matches"))
	bool K2_CanHostOnlineMatches() const { return CanHostOnlineMatches(); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Match", meta = (DisplayName = "Is Publishing On Both Roles"))
	bool K2_IsPublishingOnBothRoles() const { return IsPublishingOnBothRoles(); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Match", meta = (DisplayName = "Can Player Cross Play"))
	bool K2_CanPlayerCrossPlay(int32 LocalPlayerIndex = 0) const { return CanPlayerCrossPlay(LocalPlayerIndex); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Match", meta = (DisplayName = "Is Cross Play Optional"))
	bool K2_IsCrossPlayOptional() const { return IsCrossPlayOptional(); }
};
