// Copyright PoFig Games Studio. All Rights Reserved.

#include "Match/ModularMatchSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Match/ModularLobbyBackend.h"
#include "Match/ModularSessionBackend.h"
#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineSettings.h"
#include "Core/ModularOnlineSubsystem.h"
#include "Core/ModularOnlineTags.h"
#include "Server/ModularServerSubsystem.h"
#include "User/ModularUserInfo.h"
#include "User/ModularUserSubsystem.h"
#include "User/ModularUserTags.h"
#include "Online/Lobbies.h"
#include "Online/OnlineErrorDefinitions.h"
#include "Online/OnlineResult.h"
#include "Online/OnlineServices.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularMatchSubsystem)

using PoFigGames::Online::FModularLobbyBackend;
using PoFigGames::Online::IModularMatchBackend;
using PoFigGames::Online::LexToString;
using PoFigGames::Online::FModularSessionBackend;
using PoFigGames::Online::FModularMatchContext;

namespace PoFigGames::Online::Private
{
	/** Local name every match of this plugin is created under. */
	static const FName MatchLocalName { TEXT("GameSession") };

	/** What the services call a leave reason, in the words of this plugin. */
	static EModularMatchLeaveReason FromLobbyLeaveReason(const UE::Online::ELobbyMemberLeaveReason Reason)
	{
		switch (Reason)
		{
		case UE::Online::ELobbyMemberLeaveReason::Left:
			return EModularMatchLeaveReason::Left;

		case UE::Online::ELobbyMemberLeaveReason::Disconnected:
			return EModularMatchLeaveReason::Disconnected;

		case UE::Online::ELobbyMemberLeaveReason::Kicked:
			return EModularMatchLeaveReason::Kicked;

		case UE::Online::ELobbyMemberLeaveReason::Closed:
			return EModularMatchLeaveReason::Closed;
		}

		return EModularMatchLeaveReason::Left;
	}
}

void UModularMatchSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UModularOnlineSubsystem>();
	Collection.InitializeDependency<UModularUserSubsystem>();

	const auto GameInstance = GetGameInstance();
	bIsDedicatedServer = GameInstance && GameInstance->IsDedicatedServerInstance();

	// The engine is the only one who knows that the connection a match is played over has gone.
	if (GEngine != nullptr)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UModularMatchSubsystem::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UModularMatchSubsystem::HandleTravelFailure);
	}
}

void UModularMatchSubsystem::Deinitialize()
{
	if (GEngine != nullptr)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}

	NetworkFailureHandle.Reset();
	TravelFailureHandle.Reset();

	MatchEventHandles.Reset();
	Backend.Reset();
	CompanionBackend.Reset();
	bEventsBound = false;

	Super::Deinitialize();
}

bool UModularMatchSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TArray<UClass*> ChildClasses;
	GetDerivedClasses(GetClass(), ChildClasses, false);

	// Only create an instance if there is not a game specific subclass
	return ChildClasses.Num() == 0;
}

UModularOnlineSubsystem* UModularMatchSubsystem::GetOnline() const
{
	const auto GameInstance = GetGameInstance();

	return GameInstance ? GameInstance->GetSubsystem<UModularOnlineSubsystem>() : nullptr;
}

UModularUserSubsystem* UModularMatchSubsystem::GetUsers() const
{
	const auto GameInstance = GetGameInstance();

	return GameInstance ? GameInstance->GetSubsystem<UModularUserSubsystem>() : nullptr;
}

EModularOnlineRole UModularMatchSubsystem::GetMatchRole() const
{
	const auto Settings = GetDefault<UModularOnlineSettings>();

	// Named in the settings. A role without a provider of its own resolves to the default one, which is
	// the documented behaviour of the roles themselves rather than a guess made here.
	return Settings ? Settings->MatchRole : EModularOnlineRole::Service;
}

EModularOnlineRole UModularMatchSubsystem::GetCompanionMatchRole() const
{
	const auto Settings = GetDefault<UModularOnlineSettings>();

	return Settings ? Settings->CompanionMatchRole : EModularOnlineRole::Platform;
}

TSharedPtr<IModularMatchBackend> UModularMatchSubsystem::MakeBackend(const EModularMatchBackendKind Kind, const EModularOnlineRole Role) const
{
	const auto Online = GetOnline();
	if (!Online)
	{
		return nullptr;
	}

	// What carries a match is the project's decision, taken in the settings. If the provider of this
	// platform has no such component, matches do not work here and every call says so by name - which is
	// an answer somebody can act on, unlike a match that quietly became the other kind.
	TSharedPtr<IModularMatchBackend> Chosen;

	switch (Kind)
	{
	case EModularMatchBackendKind::Lobbies:
		Chosen = MakeShared<FModularLobbyBackend>();
		break;

	case EModularMatchBackendKind::Sessions:
		Chosen = MakeShared<FModularSessionBackend>();
		break;
	}

	if (!ensure(Chosen.IsValid()))
	{
		return nullptr;
	}

	if (const auto Required = Chosen->GetRequiredFeature(); !Online->HasFeature(Required, Role))
	{
		// A role with no provider at all is a build running offline, already said once elsewhere. A
		// provider that is there and lacks the component is the case to shout about.
		if (const auto Provider = Online->GetProviderName(Role); Provider.IsEmpty())
		{
			UE_LOG(LogModularOnline, Log, TEXT("The %s role has no online services here, so matches stay offline."), *LexToString(Role));
		}
		else
		{
			UE_LOG(LogModularOnline, Error, TEXT("Matches are configured to use %s on the %s role, which %s does not implement (%s). Calls to that role will answer NotSupported until the configuration or the provider changes."),
				*LexToString(Kind),
				*LexToString(Role),
				*Provider,
				*Required.ToString());
		}

		return nullptr;
	}

	return Chosen;
}

void UModularMatchSubsystem::SelectBackend() const
{
	const auto Settings = GetDefault<UModularMatchBackendSettings>();
	const auto CrossPlay = GetDefault<UModularCrossPlaySettings>();
	const auto Online = GetOnline();

	if (!Settings || !CrossPlay || !Online)
	{
		return;
	}

	// A backend chosen against the previous services names services that are gone. The instance name is
	// how a new world is recognised, and outside the editor it is always empty.
	if (const auto Instance = Online->GetBoundInstanceName(); Instance != ChosenForInstance || !bInstanceKnown)
	{
		ChosenForInstance = Instance;
		bInstanceKnown = true;

		Backend.Reset();
		CompanionBackend.Reset();

		bBackendChosen = false;
		bCompanionChosen = false;

		// The subscriptions belong to the services they were taken from, exactly as the backends do: kept
		// across a rebuild, the handles hold services that are gone and nothing is listening any more.
		MatchEventHandles.Reset();
		bEventsBound = false;
	}

	// And it cannot be answered before there are any services to choose from. The frontend asks to leave
	// a match before anybody has signed in, and latching "no backend" at that moment would leave this
	// whole game instance unable to host or search for the rest of its life.
	if (!Online->HasAnyProvider())
	{
		bBackendChosen = false;
		bCompanionChosen = false;

		return;
	}

	if (!bBackendChosen)
	{
		bBackendChosen = true;

		if (Backend = MakeBackend(Settings->MatchBackend, GetMatchRole()); Backend.IsValid())
		{
			UE_LOG(LogModularOnline, Log, TEXT("Matches are carried in %s on %s."), *LexToString(Settings->MatchBackend), *Online->GetProviderName(GetMatchRole()));
		}
	}

	if (bCompanionChosen || CrossPlay->CrossPlayPolicy != EModularCrossPlayPolicy::BothRoles)
	{
		return;
	}

	bCompanionChosen = true;

	// A machine with no player of its own has no platform to publish to, so the second publication is
	// not merely skipped here - it would have nobody to be about.
	if (bIsDedicatedServer)
	{
		UE_LOG(LogModularOnline, Log, TEXT("A dedicated server publishes on the match role only; the companion role belongs to a player's machine."));

		return;
	}

	// Two publications in the same services would be the same match advertised twice to the same people.
	// Saying so is better than silently doing it, because the setting was meant to name a second place.
	if (GetCompanionMatchRole() == GetMatchRole())
	{
		UE_LOG(LogModularOnline, Error, TEXT("Cross play asks for two publications, but the companion role is the same role that carries matches. The second publication is skipped."));

		return;
	}

	if (CompanionBackend = MakeBackend(CrossPlay->CompanionMatchBackend, GetCompanionMatchRole()); CompanionBackend.IsValid())
	{
		UE_LOG(LogModularOnline, Log, TEXT("Matches are also published in %s on %s, which is where invitations and the system overlay look."),
			*LexToString(CrossPlay->CompanionMatchBackend),
			*Online->GetProviderName(GetCompanionMatchRole()));
	}
}

bool UModularMatchSubsystem::IsPublishingOnBothRoles() const
{
	SelectBackend();

	return Backend.IsValid() && CompanionBackend.IsValid();
}

bool UModularMatchSubsystem::CanPlayerCrossPlay(const int32 LocalPlayerIndex) const
{
	const auto Users = GetUsers();
	const auto User = Users ? Users->GetUserForLocalPlayerIndex(LocalPlayerIndex) : nullptr;

	if (!User)
	{
		return false;
	}

	// Anything other than a plain yes is a no. The privilege exists because a platform or a parent can
	// forbid cross play outright, and a maybe there is not something a match can be built on.
	// Asked of the platform, which is where the login asked it and where a gate on cross play lives.
	return User->GetPrivilege(EModularOnlinePrivilege::CanUseCrossPlay, EModularOnlineRole::Platform) == EModularOnlinePrivilegeResult::Available;
}

bool UModularMatchSubsystem::IsCrossPlayOptional() const
{
	const auto Users = GetUsers();

	return Users && Users->HasTrait(ModularUserTags::Trait_CrossPlayOptional);
}

bool UModularMatchSubsystem::ShouldMirrorMatch(const bool bPublishesOnBothRoles, const bool bHostAllowsCrossPlay, const bool bAccountMayCrossPlay)
{
	return bPublishesOnBothRoles && bHostAllowsCrossPlay && bAccountMayCrossPlay;
}

EModularCrossPlayScope UModularMatchSubsystem::ResolveSearchScope(const EModularCrossPlayScope Asked, const bool bPublishesOnBothRoles, const bool bAccountMayCrossPlay)
{
	if (bPublishesOnBothRoles && !bAccountMayCrossPlay)
	{
		return EModularCrossPlayScope::OwnPlatformOnly;
	}

	return Asked;
}

TSharedPtr<IModularMatchBackend> UModularMatchSubsystem::GetBackendForRole(const EModularOnlineRole Role) const
{
	// Default names no particular role, which is what a handle built before the roles mattered carries;
	// it means the role that carries matches, exactly as the roles themselves are documented.
	return Role == GetCompanionMatchRole() && Role != GetMatchRole() ? CompanionBackend : Backend;
}

bool UModularMatchSubsystem::CanHostOnlineMatches() const
{
	SelectBackend();

	return Backend.IsValid();
}

bool UModularMatchSubsystem::BuildContext(const int32 LocalPlayerIndex, const EModularOnlineRole Role, FModularMatchContext& OutContext) const
{
	const auto Online = GetOnline();
	const auto Users = GetUsers();
	const auto Context = Online ? Online->GetContext(Role) : nullptr;

	if (!Context || !Users)
	{
		return false;
	}

	OutContext.Services = Context->GetServices();
	OutContext.LocalName = PoFigGames::Online::Private::MatchLocalName;

	// A machine hosting without anybody at it publishes as itself. There is no local player to ask, and
	// the account the services gave this machine when it signed in is the only identity it has.
	if (bIsDedicatedServer)
	{
		const auto GameInstance = GetGameInstance();
		const auto Servers = GameInstance ? GameInstance->GetSubsystem<UModularServerSubsystem>() : nullptr;

		OutContext.LocalAccount = Servers ? Servers->GetServerAccountId() : UE::Online::FAccountId { };

		return OutContext.IsValid();
	}

	const auto User = Users->GetUserForLocalPlayerIndex(LocalPlayerIndex);
	if (!User)
	{
		return false;
	}

	OutContext.LocalAccount = User->GetAccountId(Role);

	return OutContext.IsValid();
}

bool UModularMatchSubsystem::FindActiveMatch(const int32 LocalPlayerIndex, EModularOnlineRole& OutRole, FModularMatchContext& OutContext, FModularMatchHandle& OutMatch) const
{
	for (const auto Role : { GetMatchRole(), GetCompanionMatchRole() })
	{
		const auto RoleBackend = GetBackendForRole(Role);

		FModularMatchContext Context;
		FModularMatchHandle Match;

		if (RoleBackend.IsValid() && BuildContext(LocalPlayerIndex, Role, Context) && RoleBackend->GetCurrentMatch(Context, Match))
		{
			Match.Role = Role;

			OutRole = Role;
			OutContext = Context;
			OutMatch = Match;

			return true;
		}
	}

	return false;
}

FModularMatchSettings UModularMatchSubsystem::DescribeForPublication(const FModularMatchSettings& Settings, const bool bCrossPlay, const FString& LinkedMatchId) const
{
	auto Described = Settings;
	Described.bAllowCrossPlay = bCrossPlay;

	if (const auto Configured = GetDefault<UModularCrossPlaySettings>())
	{
		// Published, because the only reader that matters is somebody else's search. A match with a single
		// publication says nothing about cross play: a project on one role need not carry the attribute.
		if (!Configured->MatchCrossPlayAttribute.IsNone() && Configured->CrossPlayPolicy == EModularCrossPlayPolicy::BothRoles)
		{
			Described.Attributes.Emplace(Configured->MatchCrossPlayAttribute, bCrossPlay ? TEXT("1") : TEXT("0"));
		}

		if (!Configured->MatchLinkAttribute.IsNone() && !LinkedMatchId.IsEmpty())
		{
			Described.Attributes.Emplace(Configured->MatchLinkAttribute, LinkedMatchId);
		}
	}

	return Described;
}

void UModularMatchSubsystem::DescribeFoundMatch(FModularMatchInfo& Match, const EModularOnlineRole Role) const
{
	Match.Handle.Role = Role;

	const auto Settings = GetDefault<UModularCrossPlaySettings>();
	if (!Settings || Settings->MatchCrossPlayAttribute.IsNone())
	{
		return;
	}

	// A host that published nothing about cross play predates the attribute or never had the choice, and
	// is read as allowing it: that is what a match with one publication has always been.
	const auto Published = Match.Attributes.Find(Settings->MatchCrossPlayAttribute);
	Match.bAllowsCrossPlay = !Published || *Published != TEXT("0");
}

void UModularMatchSubsystem::MergeMatches(TArray<FModularMatchInfo>& Matches, const TArray<FModularMatchInfo>& Companion, const int32 MaxResults) const
{
	const auto Settings = GetDefault<UModularCrossPlaySettings>();
	const auto LinkAttribute = Settings ? Settings->MatchLinkAttribute : FName { };

	TSet<FString> Known;
	Known.Reserve(Matches.Num());

	for (const auto& Match : Matches)
	{
		Known.Add(Match.Handle.Id);
	}

	for (const auto& Match : Companion)
	{
		if (MaxResults > 0 && Matches.Num() >= MaxResults)
		{
			break;
		}

		// The second publication names the first, which is the only way to know that two search answers
		// are one game. Without it a browser lists the same match once per role.
		const auto Linked = LinkAttribute.IsNone() ? nullptr : Match.Attributes.Find(LinkAttribute);

		if (const auto bAlreadyListed = Known.Contains(Match.Handle.Id) || (Linked && Known.Contains(*Linked)); !bAlreadyListed)
		{
			Known.Add(Match.Handle.Id);
			Matches.Add(Match);
		}
	}
}

bool UModularMatchSubsystem::HostMatch(const int32 LocalPlayerIndex, const FModularMatchSettings& Settings, FModularMatchOperationDelegate OnComplete)
{
	if (FText Error; !Settings.Validate(Error))
	{
		auto Result = FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidParams());
		Result.ErrorText = Error;

		UE_LOG(LogModularOnline, Warning, TEXT("HostMatch refused: %s"), *Error.ToString());

		OnComplete.ExecuteIfBound(Result);
		OnMatchCreated.Broadcast(Result);
		K2_OnMatchCreated.Broadcast(Result);

		return false;
	}

	const auto World = GetWorld();
	if (!World)
	{
		// Answered on the event as well as to the caller: a listener that only watches OnMatchCreated
		// would otherwise wait forever for a match that was refused before it began.
		const auto Refused = FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidState());

		OnComplete.ExecuteIfBound(Refused);
		OnMatchCreated.Broadcast(Refused);
		K2_OnMatchCreated.Broadcast(Refused);

		return false;
	}

	PendingTravelURL = Settings.ConstructTravelURL();

	if (Settings.OnlineMode == EModularMatchOnlineMode::Offline)
	{
		// Nothing to publish and nobody to wait for.
		const auto Result = FModularOnlineResult::Success();

		OnComplete.ExecuteIfBound(Result);
		OnMatchCreated.Broadcast(Result);
		K2_OnMatchCreated.Broadcast(Result);

		TravelToHostedMap();

		return true;
	}

	SelectBackend();
	BindMatchEvents();

	// Cross play has to be wanted by the project, by the host and by the account. Any of the three
	// saying no means the same thing: this match lives on the platform's own role and is never published
	// where somebody on another platform could find it.
	const auto bMirror = ShouldMirrorMatch(IsPublishingOnBothRoles(), Settings.bAllowCrossPlay, CanPlayerCrossPlay(LocalPlayerIndex));
	const auto bCompanionOnly = !bMirror && IsPublishingOnBothRoles();
	const auto PrimaryRole = bCompanionOnly ? GetCompanionMatchRole() : GetMatchRole();
	const auto PrimaryBackend = GetBackendForRole(PrimaryRole);

	if (bCompanionOnly)
	{
		UE_LOG(LogModularOnline, Log, TEXT("This match is restricted to %s: %s."),
			*LexToString(PrimaryRole),
			Settings.bAllowCrossPlay ? TEXT("the account may not cross play") : TEXT("the host asked for no cross play"));
	}

	FModularMatchContext Context;
	if (!PrimaryBackend.IsValid() || !BuildContext(LocalPlayerIndex, PrimaryRole, Context))
	{
		const auto Result = GetMissingBackendResult();

		OnComplete.ExecuteIfBound(Result);
		OnMatchCreated.Broadcast(Result);
		K2_OnMatchCreated.Broadcast(Result);

		return false;
	}

	const auto Published = DescribeForPublication(Settings, bMirror, FString { });

	PrimaryBackend->CreateMatch(Context, Published, FModularMatchOperationDelegate::CreateWeakLambda(this, [this, LocalPlayerIndex, Settings, bMirror, OnComplete](const FModularOnlineResult& Result)
	{
		if (!Result.bWasSuccessful || !bMirror)
		{
			CompleteHostedMatch(Result, OnComplete);

			return;
		}

		auto HostedRole = EModularOnlineRole::Default;
		FModularMatchContext HostedContext;
		FModularMatchHandle Hosted;

		FModularMatchContext CompanionContext;

		if (!FindActiveMatch(LocalPlayerIndex, HostedRole, HostedContext, Hosted) || !BuildContext(LocalPlayerIndex, GetCompanionMatchRole(), CompanionContext))
		{
			CompleteHostedMatch(GetMissingBackendResult(), OnComplete);

			return;
		}

		// The second publication names the first, so that a search answering from both roles can tell
		// that the two entries are one game rather than two.
		const auto Mirrored = DescribeForPublication(Settings, true, Hosted.Id);

		CompanionBackend->CreateMatch(CompanionContext, Mirrored, FModularMatchOperationDelegate::CreateWeakLambda(this, [this, LocalPlayerIndex, HostedContext, OnComplete](const FModularOnlineResult& CompanionResult)
		{
			if (CompanionResult.bWasSuccessful)
			{
				CompleteHostedMatch(CompanionResult, OnComplete);

				return;
			}

			// Half a cross play match is worse than none: the game would be up, invitations and the
			// overlay would not work, and nothing would say why. The first publication goes away with it.
			UE_LOG(LogModularOnline, Error, TEXT("The match was opened but could not be published for cross play, and has been withdrawn: %s"), *CompanionResult.ToLogString());

			if (Backend.IsValid())
			{
				Backend->LeaveMatch(HostedContext, FModularMatchOperationDelegate { });
			}

			CompleteHostedMatch(CompanionResult, OnComplete);
		}));
	}));

	return true;
}

void UModularMatchSubsystem::CompleteHostedMatch(const FModularOnlineResult& Result, FModularMatchOperationDelegate OnComplete)
{
	OnComplete.ExecuteIfBound(Result);
	OnMatchCreated.Broadcast(Result);
	K2_OnMatchCreated.Broadcast(Result);

	if (!Result.bWasSuccessful)
	{
		UE_LOG(LogModularOnline, Error, TEXT("The match could not be opened: %s"), *Result.ToLogString());

		return;
	}

	// The map is only opened once the services accepted the match, so that nobody travels to a game that
	// was never published.
	TravelToHostedMap();
}

bool UModularMatchSubsystem::FindMatches(const int32 LocalPlayerIndex, const FModularMatchSearchParams& Params, FModularMatchSearchDelegate OnComplete)
{
	SelectBackend();

	// A player who may not cross play is narrowed whatever they asked for. Showing them matches on the
	// cross platform role would be showing them games they cannot enter.
	const auto Scope = ResolveSearchScope(Params.CrossPlay, IsPublishingOnBothRoles(), CanPlayerCrossPlay(LocalPlayerIndex));

	if (Scope != Params.CrossPlay && !bNarrowingLogged)
	{
		bNarrowingLogged = true;

		UE_LOG(LogModularOnline, Log, TEXT("Searches are restricted to %s: this account may not cross play."), *LexToString(GetCompanionMatchRole()));
	}

	// With one publication there is one place to look, and the scope has nothing to choose between.
	const auto bOwnPlatformOnly = Scope == EModularCrossPlayScope::OwnPlatformOnly && IsPublishingOnBothRoles();
	const auto FirstRole = bOwnPlatformOnly ? GetCompanionMatchRole() : GetMatchRole();
	const auto FirstBackend = GetBackendForRole(FirstRole);

	FModularMatchContext Context;
	if (!FirstBackend.IsValid() || !BuildContext(LocalPlayerIndex, FirstRole, Context))
	{
		OnComplete.ExecuteIfBound(GetMissingBackendResult(), TArray<FModularMatchInfo> { });

		return false;
	}

	const auto bSearchBothRoles = !bOwnPlatformOnly && IsPublishingOnBothRoles();

	FirstBackend->FindMatches(Context, Params, FModularMatchSearchDelegate::CreateWeakLambda(this, [this, LocalPlayerIndex, Params, FirstRole, bSearchBothRoles, OnComplete](const FModularOnlineResult& Result, const TArray<FModularMatchInfo>& Found)
	{
		auto Matches = Found;

		for (auto& Match : Matches)
		{
			DescribeFoundMatch(Match, FirstRole);
		}

		if (FirstRole == GetMatchRole() && bSearchBothRoles)
		{
			// A match on the cross platform role that refuses cross play is reached through the other role
			// instead. Asked of a project that publishes once, this would empty every search.
			Matches.RemoveAll([](const FModularMatchInfo& Match) { return !Match.bAllowsCrossPlay; });
		}

		FModularMatchContext CompanionContext;

		if (!Result.bWasSuccessful || !bSearchBothRoles || !CompanionBackend.IsValid() || !BuildContext(LocalPlayerIndex, GetCompanionMatchRole(), CompanionContext))
		{
			OnComplete.ExecuteIfBound(Result, Matches);

			return;
		}

		CompanionBackend->FindMatches(CompanionContext, Params, FModularMatchSearchDelegate::CreateWeakLambda(this, [this, Params, Matches = MoveTemp(Matches), OnComplete](const FModularOnlineResult& CompanionResult, const TArray<FModularMatchInfo>& CompanionFound) mutable
		{
			// The second role failing is not the search failing: what the first one found is still real,
			// and a browser that showed nothing because one of two backends was down would be lying.
			if (!CompanionResult.bWasSuccessful)
			{
				UE_LOG(LogModularOnline, Warning, TEXT("The companion role could not be searched, so only %s answered: %s"), *LexToString(GetMatchRole()), *CompanionResult.ToLogString());

				OnComplete.ExecuteIfBound(FModularOnlineResult::Success(), Matches);

				return;
			}

			auto Companion = CompanionFound;

			for (auto& Match : Companion)
			{
				DescribeFoundMatch(Match, GetCompanionMatchRole());
			}

			MergeMatches(Matches, Companion, Params.MaxResults);

			OnComplete.ExecuteIfBound(FModularOnlineResult::Success(), Matches);
		}));
	}));

	return true;
}

bool UModularMatchSubsystem::JoinMatch(const int32 LocalPlayerIndex, const FModularMatchHandle& Match, FModularMatchOperationDelegate OnComplete)
{
	SelectBackend();
	BindMatchEvents();

	const auto JoinBackend = GetBackendForRole(Match.Role);

	FModularMatchContext Context;
	if (!JoinBackend.IsValid() || !BuildContext(LocalPlayerIndex, Match.Role, Context))
	{
		const auto Result = GetMissingBackendResult();

		OnComplete.ExecuteIfBound(Result);
		OnMatchJoined.Broadcast(Result);
		K2_OnMatchJoined.Broadcast(Result);

		return false;
	}

	if (!Match.IsValid())
	{
		const auto Result = FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidParams());

		OnComplete.ExecuteIfBound(Result);
		OnMatchJoined.Broadcast(Result);
		K2_OnMatchJoined.Broadcast(Result);

		return false;
	}

	JoinBackend->JoinMatch(Context, Match, true, FModularMatchOperationDelegate::CreateWeakLambda(this, [this, LocalPlayerIndex, OnComplete](const FModularOnlineResult& Result)
	{
		OnComplete.ExecuteIfBound(Result);
		OnMatchJoined.Broadcast(Result);
		K2_OnMatchJoined.Broadcast(Result);

		if (Result.bWasSuccessful)
		{
			TravelToJoinedMatch(LocalPlayerIndex);
		}
		else
		{
			UE_LOG(LogModularOnline, Error, TEXT("The match could not be joined: %s"), *Result.ToLogString());
		}
	}));

	return true;
}

bool UModularMatchSubsystem::TravelMatchTo(const int32 LocalPlayerIndex, const FModularMatchSettings& Settings, FModularMatchOperationDelegate OnComplete)
{
	SelectBackend();

	const auto World = GetWorld();

	if (!World || World->GetNetMode() == NM_Client)
	{
		// A client does not decide where anybody plays; it is taken there.
		OnComplete.ExecuteIfBound(FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidState()));

		return false;
	}

	PendingTravelURL = Settings.ConstructTravelURL();

	if (PendingTravelURL.IsEmpty())
	{
		UE_LOG(LogModularOnline, Error, TEXT("The match cannot move: these settings name no map."));

		OnComplete.ExecuteIfBound(FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidParams()));

		return false;
	}

	FModularMatchHandle Current;

	if (!GetCurrentMatch(LocalPlayerIndex, Current))
	{
		// Nothing is published, so there is nothing to keep in step: a single player game, or a build
		// with no services.
		TravelToHostedMap();

		OnComplete.ExecuteIfBound(FModularOnlineResult::Success());

		return true;
	}

	// Published first. Arriving somewhere the services still describe as the previous map is what makes a
	// browser lie, and a player who joins on that description arrives expecting something else.
	return UpdateMatchSettings(LocalPlayerIndex, Settings, FModularMatchOperationDelegate::CreateWeakLambda(this,
		[this, OnComplete](const FModularOnlineResult& Result)
		{
			UE_CLOG(!Result.bWasSuccessful, LogModularOnline, Warning,
				TEXT("The match could not be told where it is going (%s); travelling anyway, because the players are already on their way."),
				*Result.ToLogString());

			TravelToHostedMap();

			OnComplete.ExecuteIfBound(Result);
		}));
}

bool UModularMatchSubsystem::LeaveMatch(const int32 LocalPlayerIndex, FModularMatchOperationDelegate OnComplete)
{
	SelectBackend();

	// Both publications go, and the caller hears about one of them. Leaving only the role we happen to
	// look at first would leave a match advertised that nobody is in any more.
	struct FLeaving
	{
		TSharedPtr<PoFigGames::Online::IModularMatchBackend> Backend { nullptr };
		FModularMatchContext Context { };
		EModularOnlineRole Role { EModularOnlineRole::Default };
	};

	TArray<FLeaving> Leaving;

	for (const auto Role : { GetMatchRole(), GetCompanionMatchRole() })
	{
		const auto RoleBackend = GetBackendForRole(Role);

		FModularMatchContext Context;
		FModularMatchHandle Match;

		if (RoleBackend.IsValid() && BuildContext(LocalPlayerIndex, Role, Context) && RoleBackend->GetCurrentMatch(Context, Match))
		{
			Leaving.Add(FLeaving { RoleBackend, Context, Role });
		}
	}

	if (Leaving.IsEmpty())
	{
		// Nobody to leave: an offline match, or a build with no services. The caller wanted to be out of a
		// match and is.
		OnComplete.ExecuteIfBound(FModularOnlineResult::Success());

		return true;
	}

	// Answered by the publication that carried the game where there is one, and otherwise by whichever is
	// left - the caller asked to be out and is owed an answer either way, and it has to be a real one
	// rather than a success invented while the services are still working.
	const auto Answering = Leaving.IndexOfByPredicate([this](const FLeaving& Entry) { return Entry.Role == GetMatchRole(); });
	const auto AnsweringIndex = Answering == INDEX_NONE ? 0 : Answering;

	for (auto Index = 0; Index < Leaving.Num(); ++Index)
	{
		const auto& Entry = Leaving[Index];

		Entry.Backend->LeaveMatch(Entry.Context, Index == AnsweringIndex ? OnComplete : FModularMatchOperationDelegate { });
	}

	return true;
}

bool UModularMatchSubsystem::AddressTarget(const int32 LocalPlayerIndex, const EModularOnlineRole Role, const FModularAccountHandle& TargetAccountId,
	FModularMatchContext& OutContext, const FModularMatchOperationDelegate& OnComplete) const
{
	const auto RoleBackend = GetBackendForRole(Role);

	if (!RoleBackend.IsValid() || !BuildContext(LocalPlayerIndex, Role, OutContext))
	{
		OnComplete.ExecuteIfBound(GetMissingBackendResult());

		return false;
	}

	if (!TargetAccountId.IsValid())
	{
		OnComplete.ExecuteIfBound(FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidParams()));

		return false;
	}

	return true;
}

void UModularMatchSubsystem::AnnounceJoinRequestedFromOverlay(FModularMatchInfo& Match, const EModularOnlineRole Role,
	const UE::Online::FAccountId& Asker)
{
	Match.Handle.Role = Role;

	UE_LOG(LogModularOnline, Log, TEXT("A join to match %s was asked for in the system overlay."), *Match.Handle.Id);

	const auto Account = MakeModularAccount(Asker);

	// Deliberately not joined here: the game decides whether it is ready to leave whatever it is doing.
	OnJoinRequestedFromOverlay.Broadcast(Match, Account);
	K2_OnJoinRequestedFromOverlay.Broadcast(Match, Account);
}

bool UModularMatchSubsystem::InviteToMatch(const int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId, FModularMatchOperationDelegate OnComplete)
{
	SelectBackend();

	// Invitations belong to the companion role wherever there is one: it is the platform's own, and an
	// invitation the platform did not send is one its overlay, its notifications and its friends list
	// know nothing about. With one publication there is nothing to choose.
	const auto Role = IsPublishingOnBothRoles() ? GetCompanionMatchRole() : GetMatchRole();
	const auto RoleBackend = GetBackendForRole(Role);

	FModularMatchContext Context;
	if (!AddressTarget(LocalPlayerIndex, Role, TargetAccountId, Context, OnComplete))
	{
		return false;
	}

	const auto Target = TargetAccountId.AccountId;

	RoleBackend->InviteToMatch(Context, Target, OnComplete);

	return true;
}

bool UModularMatchSubsystem::KickMember(const int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId, FModularMatchOperationDelegate OnComplete)
{
	SelectBackend();

	FModularMatchContext Context;
	if (!AddressTarget(LocalPlayerIndex, GetMatchRole(), TargetAccountId, Context, OnComplete))
	{
		return false;
	}

	const auto Target = TargetAccountId.AccountId;

	Backend->KickMember(Context, Target, OnComplete);

	// Somebody removed from the game has to be removed from the other publication as well, or the
	// platform still believes they are in the match and lets them back in through it.
	FModularMatchContext CompanionContext;

	if (IsPublishingOnBothRoles() && BuildContext(LocalPlayerIndex, GetCompanionMatchRole(), CompanionContext))
	{
		// The same account, asked of the other publication: an id belongs to the provider that issued it,
		// so the companion role only knows this player when both publications are that provider's.
		CompanionBackend->KickMember(CompanionContext, Target, FModularMatchOperationDelegate { });
	}

	return true;
}

bool UModularMatchSubsystem::UpdateMatchSettings(const int32 LocalPlayerIndex, const FModularMatchSettings& Settings, FModularMatchOperationDelegate OnComplete)
{
	SelectBackend();

	auto ActiveRole = EModularOnlineRole::Default;
	FModularMatchContext Context;
	FModularMatchHandle Match;

	if (!FindActiveMatch(LocalPlayerIndex, ActiveRole, Context, Match))
	{
		OnComplete.ExecuteIfBound(GetMissingBackendResult());

		return false;
	}

	// Whether this match crosses platforms was settled when it was opened and is not re-decided here: a
	// republication that quietly changed it would move the match out from under the people already in it.
	const auto bMirrored = ActiveRole == GetMatchRole() && IsPublishingOnBothRoles();
	const auto Published = DescribeForPublication(Settings, bMirrored, FString { });

	GetBackendForRole(ActiveRole)->UpdateSettings(Context, Published, OnComplete);

	FModularMatchContext CompanionContext;

	if (bMirrored && BuildContext(LocalPlayerIndex, GetCompanionMatchRole(), CompanionContext))
	{
		CompanionBackend->UpdateSettings(CompanionContext, DescribeForPublication(Settings, true, Match.Id), FModularMatchOperationDelegate { });
	}

	return true;
}

bool UModularMatchSubsystem::GetCurrentMatch(const int32 LocalPlayerIndex, FModularMatchHandle& OutMatch) const
{
	auto Role = EModularOnlineRole::Default;
	FModularMatchContext Context;

	return FindActiveMatch(LocalPlayerIndex, Role, Context, OutMatch);
}

void UModularMatchSubsystem::TravelToHostedMap()
{
	const auto World = GetWorld();
	if (!World)
	{
		UE_LOG(LogModularOnline, Error, TEXT("The match was opened, but there is no world to travel."));

		return;
	}

	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogModularOnline, Error, TEXT("A client cannot open a match: it has no map to travel anybody to."));

		return;
	}

	if (PendingTravelURL.IsEmpty())
	{
		UE_LOG(LogModularOnline, Error, TEXT("The match was opened without a map to travel to."));

		return;
	}

	UE_LOG(LogModularOnline, Log, TEXT("Travelling to the hosted map: %s"), *PendingTravelURL);

	World->ServerTravel(PendingTravelURL);
	PendingTravelURL.Reset();
}

void UModularMatchSubsystem::TravelToJoinedMatch(const int32 LocalPlayerIndex)
{
	const auto GameInstance = GetGameInstance();
	const auto PlayerController = GameInstance ? GameInstance->GetLocalPlayerByIndex(LocalPlayerIndex) : nullptr;
	const auto Controller = PlayerController ? PlayerController->GetPlayerController(GetWorld()) : nullptr;

	if (!Controller)
	{
		UE_LOG(LogModularOnline, Error, TEXT("The match was joined, but there is no local player to travel."));

		return;
	}

	auto Role = EModularOnlineRole::Default;
	FModularMatchContext Context;
	FModularMatchHandle Match;

	if (!FindActiveMatch(LocalPlayerIndex, Role, Context, Match))
	{
		UE_LOG(LogModularOnline, Error, TEXT("The match was joined, but the services no longer report it."));

		return;
	}

	UE::Online::FGetResolvedConnectString::Params Params;
	Params.LocalAccountId = Context.LocalAccount;
	Params.LobbyId = Match.LobbyId;
	Params.SessionId = Match.SessionId;

	const auto Resolved = Context.Services->GetResolvedConnectString(MoveTemp(Params));
	if (!Resolved.IsOk())
	{
		UE_LOG(LogModularOnline, Error, TEXT("The match has no address to travel to: %s"), *ToLogString(Resolved.GetErrorValue()));

		return;
	}

	// The game gets the last word on the address: an encryption token and anything else only it knows
	// has to be on the URL before the travel, and there is no second chance afterwards.
	auto TravelURL = Resolved.GetOkValue().ResolvedConnectString;
	OnPreClientTravel.Broadcast(TravelURL);

	UE_LOG(LogModularOnline, Log, TEXT("Travelling to the joined match."));

	Controller->ClientTravel(TravelURL, TRAVEL_Absolute);
}

void UModularMatchSubsystem::BindMatchEvents()
{
	const auto Online = GetOnline();

	if (bEventsBound || !Online)
	{
		return;
	}

	// Both publications are listened to, not only the one that carries the game. An invitation and a join
	// asked for in the platform's own overlay arrive on the platform role, which is where a companion
	// publication lives - heard on the match role only, neither ever reaches the game.
	TSet<EModularOnlineRole> Bound;

	for (const auto Named : { GetMatchRole(), GetCompanionMatchRole() })
	{
		const auto Role = Online->ResolveRole(Named);
		const auto Lobbies = Online->GetInterface<UE::Online::ILobbies>(Role);

		if (Lobbies.IsValid() && !Bound.Contains(Role))
		{
			Bound.Add(Role);

			// Bound on first use for the same reason the contexts are built then: there is no world, and
			// so no services instance of this game instance, while subsystems are being initialised.
			bEventsBound = true;

			MatchEventHandles.Add(Lobbies->OnLobbyJoined().Add(this, &ThisClass::HandleLobbyJoined));
			MatchEventHandles.Add(Lobbies->OnLobbyLeft().Add(this, &ThisClass::HandleLobbyLeft));
			MatchEventHandles.Add(Lobbies->OnLobbyMemberJoined().Add(this, &ThisClass::HandleLobbyMemberJoined));
			MatchEventHandles.Add(Lobbies->OnLobbyMemberLeft().Add(this, &ThisClass::HandleLobbyMemberLeft));

			// The role travels with these two, because joining has to go back to the publication the match
			// was offered through: a handle taken to the other role names nothing there.
			MatchEventHandles.Add(Lobbies->OnLobbyInvitationAdded().Add(this, &ThisClass::HandleLobbyInvitation, Named));
			MatchEventHandles.Add(Lobbies->OnUILobbyJoinRequested().Add(this, &ThisClass::HandleLobbyJoinRequested, Named));
		}
	}

	// A project whose matches live in sessions hears the same things through a different component, and
	// heard none of them before: no invitation, no join from the overlay, no notice that a session ended.
	Bound.Reset();

	for (const auto Named : { GetMatchRole(), GetCompanionMatchRole() })
	{
		const auto Role = Online->ResolveRole(Named);
		const auto Sessions = Online->GetInterface<UE::Online::ISessions>(Role);

		if (Sessions.IsValid() && !Bound.Contains(Role))
		{
			Bound.Add(Role);
			bEventsBound = true;

			MatchEventHandles.Add(Sessions->OnSessionLeft().Add(this, &ThisClass::HandleSessionLeft));
			MatchEventHandles.Add(Sessions->OnSessionInviteReceived().Add(this, &ThisClass::HandleSessionInvite, Named));
			MatchEventHandles.Add(Sessions->OnUISessionJoinRequested().Add(this, &ThisClass::HandleSessionJoinRequested, Named));
		}
	}
}

void UModularMatchSubsystem::HandleLobbyJoined(const UE::Online::FLobbyJoined& EventParameters)
{
	UE_LOG(LogModularOnline, Log, TEXT("Joined match %s."), *ToLogString(EventParameters.Lobby->LobbyId));
}

void UModularMatchSubsystem::HandleLobbyLeft(const UE::Online::FLobbyLeft& EventParameters)
{
	UE_LOG(LogModularOnline, Log, TEXT("Left match %s."), *ToLogString(EventParameters.Lobby->LobbyId));

	// The services do not say why the local player is out here; a member leave event does, and arrives
	// first when there was a reason other than leaving.
	OnMatchLeft.Broadcast(EModularMatchLeaveReason::Left);
	K2_OnMatchLeft.Broadcast(EModularMatchLeaveReason::Left);
}

void UModularMatchSubsystem::HandleLobbyMemberJoined(const UE::Online::FLobbyMemberJoined& EventParameters)
{
	// Somebody arriving is not somebody leaving, and the reason a member event carries only means anything
	// when they went.
	const auto Member = MakeModularAccount(EventParameters.Member->AccountId);

	OnMemberJoined.Broadcast(Member, EModularMatchLeaveReason::None);
	K2_OnMemberJoined.Broadcast(Member, EModularMatchLeaveReason::None);
}

void UModularMatchSubsystem::HandleLobbyMemberLeft(const UE::Online::FLobbyMemberLeft& EventParameters)
{
	const auto Reason = PoFigGames::Online::Private::FromLobbyLeaveReason(EventParameters.Reason);
	const auto MemberId = MakeModularAccount(EventParameters.Member->AccountId);

	OnMemberLeft.Broadcast(MemberId, Reason);
	K2_OnMemberLeft.Broadcast(MemberId, Reason);

	// When the member who left is us, this is also how the game learns it was kicked or that the host
	// closed the match, which the plain left event cannot tell apart.
	const auto Users = GetUsers();
	const auto LocalUser = Users ? Users->GetUserForLocalPlayerIndex(0) : nullptr;

	if (LocalUser && LocalUser->GetAccountId(GetMatchRole()) == EventParameters.Member->AccountId && Reason != EModularMatchLeaveReason::Left)
	{
		OnMatchLeft.Broadcast(Reason);
		K2_OnMatchLeft.Broadcast(Reason);
	}
}

void UModularMatchSubsystem::HandleLobbyInvitation(const UE::Online::FLobbyInvitationAdded& EventParameters, const EModularOnlineRole Role)
{
	auto Match = FModularLobbyBackend::DescribeLobby(*EventParameters.Lobby);
	Match.Handle.Role = Role;

	UE_LOG(LogModularOnline, Log, TEXT("An invitation to match %s arrived from %s."), *Match.Handle.Id, *UE::Online::ToLogString(EventParameters.SenderId));

	const auto Sender = MakeModularAccount(EventParameters.SenderId);

	OnInvitationReceived.Broadcast(Match, Sender);
	K2_OnInvitationReceived.Broadcast(Match, Sender);
}

void UModularMatchSubsystem::HandleLobbyJoinRequested(const UE::Online::FUILobbyJoinRequested& EventParameters, const EModularOnlineRole Role)
{
	if (EventParameters.Result.IsError())
	{
		UE_LOG(LogModularOnline, Error, TEXT("A join was asked for in the overlay, but the match could not be read: %s"), *ToLogString(EventParameters.Result.GetErrorValue()));

		return;
	}

	auto Match = FModularLobbyBackend::DescribeLobby(*EventParameters.Result.GetOkValue());
	AnnounceJoinRequestedFromOverlay(Match, Role, EventParameters.LocalAccountId);
}

void UModularMatchSubsystem::HandleNetworkFailure(UWorld* /*World*/, UNetDriver* NetDriver, const ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// Only the driver carrying the game is a match. A beacon has its own failures, and a host losing one
	// client is that client's problem rather than the host's.
	if (NetDriver == nullptr
		|| (NetDriver->NetDriverName != NAME_GameNetDriver && NetDriver->NetDriverName != NAME_PendingNetDriver)
		|| NetDriver->GetNetMode() != NM_Client)
	{
		return;
	}

	// A failure the server sent is a decision it made about this player; anything else is the connection
	// itself giving out.
	const auto Reason = FailureType == ENetworkFailure::FailureReceived
		? EModularMatchLeaveReason::Kicked
		: EModularMatchLeaveReason::Disconnected;

	UE_LOG(LogModularOnline, Log, TEXT("The connection to the match failed (%s): %s"), ENetworkFailure::ToString(FailureType), *ErrorString);

	OnMatchLeft.Broadcast(Reason);
	K2_OnMatchLeft.Broadcast(Reason);
}

void UModularMatchSubsystem::HandleTravelFailure(UWorld* /*World*/, const ETravelFailure::Type FailureType, const FString& ErrorString)
{
	// A dedicated server has no match of its own to be thrown out of, and nobody to tell.
	if (bIsDedicatedServer)
	{
		return;
	}

	UE_LOG(LogModularOnline, Log, TEXT("Travelling to the match failed (%s): %s"), ETravelFailure::ToString(FailureType), *ErrorString);

	OnMatchLeft.Broadcast(EModularMatchLeaveReason::Disconnected);
	K2_OnMatchLeft.Broadcast(EModularMatchLeaveReason::Disconnected);
}

void UModularMatchSubsystem::HandleSessionLeft(const UE::Online::FSessionLeft& EventParameters)
{
	UE_LOG(LogModularOnline, Log, TEXT("Left a session."));

	OnMatchLeft.Broadcast(EModularMatchLeaveReason::Left);
	K2_OnMatchLeft.Broadcast(EModularMatchLeaveReason::Left);
}

void UModularMatchSubsystem::HandleSessionInvite(const UE::Online::FSessionInviteReceived& EventParameters, const EModularOnlineRole Role)
{
	const auto Online = GetOnline();
	const auto Sessions = Online ? Online->GetInterface<UE::Online::ISessions>(Online->ResolveRole(Role)) : nullptr;

	if (!Sessions.IsValid())
	{
		return;
	}

	// The event names the invitation, not what it invites to: both the match and who sent it have to be
	// read from the invitation itself.
	const auto Invite = Sessions->GetSessionInviteById({ EventParameters.LocalAccountId, EventParameters.SessionInviteId });
	if (!Invite.IsOk())
	{
		UE_LOG(LogModularOnline, Error, TEXT("An invitation arrived that could not be read: %s"), *ToLogString(Invite.GetErrorValue()));

		return;
	}

	const auto Session = Sessions->GetSessionById({ Invite.GetOkValue().SessionInvite->GetSessionId() });
	if (!Session.IsOk())
	{
		UE_LOG(LogModularOnline, Error, TEXT("An invitation arrived to a session that could not be read: %s"), *ToLogString(Session.GetErrorValue()));

		return;
	}

	auto Match = FModularSessionBackend::DescribeSession(*Session.GetOkValue().Session);
	Match.Handle.Role = Role;

	const auto Sender = MakeModularAccount(Invite.GetOkValue().SessionInvite->GetSenderId());

	UE_LOG(LogModularOnline, Log, TEXT("An invitation to match %s arrived from %s."), *Match.Handle.Id, *Sender.Id);

	OnInvitationReceived.Broadcast(Match, Sender);
	K2_OnInvitationReceived.Broadcast(Match, Sender);
}

void UModularMatchSubsystem::HandleSessionJoinRequested(const UE::Online::FUISessionJoinRequested& EventParameters, const EModularOnlineRole Role)
{
	const auto Online = GetOnline();
	const auto Sessions = Online ? Online->GetInterface<UE::Online::ISessions>(Online->ResolveRole(Role)) : nullptr;

	if (!Sessions.IsValid())
	{
		return;
	}

	if (EventParameters.Result.IsError())
	{
		UE_LOG(LogModularOnline, Error, TEXT("A join was asked for in the overlay, but the match could not be read: %s"), *ToLogString(EventParameters.Result.GetErrorValue()));

		return;
	}

	const auto Session = Sessions->GetSessionById({ EventParameters.Result.GetOkValue() });
	if (!Session.IsOk())
	{
		UE_LOG(LogModularOnline, Error, TEXT("A join was asked for in the overlay to a session that could not be read: %s"), *ToLogString(Session.GetErrorValue()));

		return;
	}

	auto Match = FModularSessionBackend::DescribeSession(*Session.GetOkValue().Session);
	AnnounceJoinRequestedFromOverlay(Match, Role, EventParameters.LocalAccountId);
}

FModularOnlineResult UModularMatchSubsystem::GetMissingBackendResult() const
{
	const auto Settings = GetDefault<UModularMatchBackendSettings>();
	const auto Kind = Settings ? Settings->MatchBackend : EModularMatchBackendKind::Lobbies;

	// Named by what the project asked for, so the answer points at the setting rather than at whichever
	// component happened to be looked up last.
	return FModularOnlineResult::NotSupported(Kind == EModularMatchBackendKind::Sessions
		? ModularOnlineTags::Feature_Sessions.GetTag()
		: ModularOnlineTags::Feature_Lobbies.GetTag());
}

