// Copyright PoFig Games Studio. All Rights Reserved.

#include "Match/ModularMatchSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/PendingNetGame.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Match/ModularLobbyBackend.h"
#include "Match/ModularSessionBackend.h"
#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineSettings.h"
#include "Core/ModularOnlineStringTable.h"
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

	/**
	 * Whether an address a provider answered with is one this client may travel to.
	 *
	 * The string is remote data: on every provider it is assembled from what the host published, so it
	 * reaches here the way any other field of a stranger's match does. Its shape differs per provider -
	 * an IP and port, a relay identity - so what is checked is that it is an address and nothing more.
	 * Travel options are the game's to add in OnPreClientTravel; one arriving from a match is a host
	 * appending to somebody else's URL.
	 */
	static bool IsTravellableAddress(const FString& Address)
	{
		if (Address.IsEmpty() || Address.Len() > 256)
		{
			return false;
		}

		for (const auto Character : Address)
		{
			const auto bCarriesOptions = Character == TEXT('?') || Character == TEXT('#');

			if (FChar::IsWhitespace(Character) || FChar::IsControl(Character) || bCarriesOptions)
			{
				return false;
			}
		}

		return true;
	}

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

	// What carries a match is the project's decision, taken in the settings. A provider without that
	// component refuses every call by name rather than quietly becoming the other kind.
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

	// And not before there are services to choose from: the frontend asks to leave a match before anybody
	// signed in, and latching "no backend" then would cost this game instance hosting for good.
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

	// Anything other than a plain yes is a no: a platform or a parent can forbid cross play outright.
	// Asked of the platform, which is where the login asked it and where such a gate lives.
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
		const auto bPublishesCrossPlay = Configured->CrossPlayPolicy == EModularCrossPlayPolicy::BothRoles;

		// Published, because the only reader that matters is somebody else's search. A match with a single
		// publication says nothing about cross play: a project on one role need not carry the attribute.
		if (bPublishesCrossPlay && !Configured->MatchCrossPlayAttribute.IsNone())
		{
			Described.Attributes.Emplace(Configured->MatchCrossPlayAttribute, bCrossPlay ? TEXT("1") : TEXT("0"));
		}

		UE_CLOG(bPublishesCrossPlay && Configured->MatchCrossPlayAttribute.IsNone(), LogModularOnline, Error,
			TEXT("Cross play is on and MatchCrossPlayAttribute names nothing; every search will narrow itself on an answer this match never published."));

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
	bDepartureAnnounced = false;

	if (const auto Waiting = RefuseWhileTravelling(); !Waiting.bWasSuccessful)
	{
		OnComplete.ExecuteIfBound(Waiting);
		OnMatchCreated.Broadcast(Waiting);
		K2_OnMatchCreated.Broadcast(Waiting);

		return false;
	}

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

	// Cross play has to be wanted by the project, by the host and by the account; any no means the match
	// lives on the platform's own role and is published nowhere else.
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

	if (const auto Refusal = BuildContextOrRefusal(LocalPlayerIndex, PrimaryRole, PrimaryBackend, Context); !Refusal.bWasSuccessful)
	{
		// Nothing was opened, so nothing is waiting to be travelled to.
		PendingTravelURL.Reset();

		OnComplete.ExecuteIfBound(Refusal);
		OnMatchCreated.Broadcast(Refusal);
		K2_OnMatchCreated.Broadcast(Refusal);

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
		// Nothing was opened, so the address it would have travelled to is not waiting for anybody.
		PendingTravelURL.Reset();

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

	if (const auto Refusal = BuildContextOrRefusal(LocalPlayerIndex, FirstRole, FirstBackend, Context); !Refusal.bWasSuccessful)
	{
		OnComplete.ExecuteIfBound(Refusal, TArray<FModularMatchInfo> { });

		return false;
	}

	const auto bSearchBothRoles = !bOwnPlatformOnly && IsPublishingOnBothRoles();

	FirstBackend->FindMatches(Context, Params, FModularMatchSearchDelegate::CreateWeakLambda(this, [this, LocalPlayerIndex, Params, FirstRole, bSearchBothRoles, OnComplete](const FModularOnlineResult& Result, const TArray<FModularMatchInfo>& Found)
	{
		// A match on the cross platform role that refuses cross play is reached through the other role
		// instead, so it is never copied out of the answer. Asked of a project that publishes once, this
		// would empty every search, which is why it only applies when both roles were searched.
		const auto bDropsWithoutCrossPlay = FirstRole == GetMatchRole() && bSearchBothRoles;

		TArray<FModularMatchInfo> Matches;
		Matches.Reserve(Found.Num());

		for (const auto& Match : Found)
		{
			auto& Described = Matches.Add_GetRef(Match);
			DescribeFoundMatch(Described, FirstRole);

			if (bDropsWithoutCrossPlay && !Described.bAllowsCrossPlay)
			{
				Matches.Pop(EAllowShrinking::No);
			}
		}

		FModularMatchContext CompanionContext;

		if (!Result.bWasSuccessful || !bSearchBothRoles || !CompanionBackend.IsValid() || !BuildContext(LocalPlayerIndex, GetCompanionMatchRole(), CompanionContext))
		{
			OnComplete.ExecuteIfBound(Result, Matches);

			return;
		}

		const auto MaxResults = Params.MaxResults;

		CompanionBackend->FindMatches(CompanionContext, Params, FModularMatchSearchDelegate::CreateWeakLambda(this, [this, MaxResults, Matches = MoveTemp(Matches), OnComplete](const FModularOnlineResult& CompanionResult, const TArray<FModularMatchInfo>& CompanionFound) mutable
		{
			// The second role failing is not the search failing: what the first one found is still real,
			// and a browser that showed nothing because one of two backends was down would be lying.
			if (!CompanionResult.bWasSuccessful)
			{
				UE_LOG(LogModularOnline, Warning, TEXT("The companion role could not be searched, so only %s answered: %s"), *LexToString(GetMatchRole()), *CompanionResult.ToLogString());

				OnComplete.ExecuteIfBound(FModularOnlineResult::Success(), Matches);

				return;
			}

			// Merged first and described after: the merge reads ids and attributes, which describing does
			// not touch, so only what is actually listed is walked and no second copy is made.
			const auto Listed = Matches.Num();
			MergeMatches(Matches, CompanionFound, MaxResults);

			for (auto Index = Listed; Index < Matches.Num(); ++Index)
			{
				DescribeFoundMatch(Matches[Index], GetCompanionMatchRole());
			}

			OnComplete.ExecuteIfBound(FModularOnlineResult::Success(), Matches);
		}));
	}));

	return true;
}

bool UModularMatchSubsystem::JoinMatch(const int32 LocalPlayerIndex, const FModularMatchHandle& Match, FModularMatchOperationDelegate OnComplete)
{
	SelectBackend();
	BindMatchEvents();

	// Being in a match again is what makes the next departure worth announcing. A host is never told it
	// joined its own lobby, and a session backend announces no join at all, so it is said here.
	bDepartureAnnounced = false;

	const auto JoinBackend = GetBackendForRole(Match.Role);

	FModularMatchContext Context;

	if (const auto Refusal = BuildContextOrRefusal(LocalPlayerIndex, Match.Role, JoinBackend, Context); !Refusal.bWasSuccessful)
	{
		AnswerJoin(Refusal, OnComplete);

		return false;
	}

	if (!Match.IsValid())
	{
		AnswerJoin(FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidParams()), OnComplete);

		return false;
	}

	JoinBackend->JoinMatch(Context, Match, true, FModularMatchOperationDelegate::CreateWeakLambda(this, [this, LocalPlayerIndex, OnComplete](const FModularOnlineResult& Result)
	{
		if (!Result.bWasSuccessful)
		{
			UE_LOG(LogModularOnline, Error, TEXT("The match could not be joined: %s"), *Result.ToLogString());

			AnswerJoin(Result, OnComplete);

			return;
		}

		// A lobby is findable before the host's server is bound to it. Answering success then would leave
		// the player in a lobby they cannot reach, waiting for an address that arrives on no event.
		FString TravelURL;

		if (const auto Reached = ResolveJoinedMatch(LocalPlayerIndex, TravelURL); !Reached.bWasSuccessful)
		{
			UE_LOG(LogModularOnline, Error, TEXT("The match was joined but cannot be reached: %s"), *Reached.ToLogString());

			LeaveMatch(LocalPlayerIndex);
			AnswerJoin(Reached, OnComplete);

			return;
		}

		AnswerJoin(Result, OnComplete);
		TravelToJoinedMatch(LocalPlayerIndex, TravelURL);
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

	if (const auto Waiting = RefuseWhileTravelling(); !Waiting.bWasSuccessful)
	{
		OnComplete.ExecuteIfBound(Waiting);

		return false;
	}

	if (FText Error; !Settings.Validate(Error))
	{
		auto Refusal = FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidParams());
		Refusal.ErrorText = Error;

		UE_LOG(LogModularOnline, Warning, TEXT("TravelMatchTo refused: %s"), *Error.ToString());

		OnComplete.ExecuteIfBound(Refusal);

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

	/**
	 * @struct FLeaving
	 *
	 * @brief One publication to leave: which backend carries it, for whom, and on which role.
	 */
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

	// Answered by the publication that carried the game where there is one, otherwise by whichever is
	// left: a real answer either way, never a success invented while the services are still working.
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

	if (const auto Refusal = BuildContextOrRefusal(LocalPlayerIndex, Role, RoleBackend, OutContext); !Refusal.bWasSuccessful)
	{
		OnComplete.ExecuteIfBound(Refusal);

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

	// Invitations belong to the companion role wherever there is one: an invitation the platform did not
	// send is one its overlay, notifications and friends list know nothing about.
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
		CompanionBackend->KickMember(CompanionContext, Target, FModularMatchOperationDelegate::CreateWeakLambda(this, [](const FModularOnlineResult& Result)
		{
			UE_CLOG(!Result.bWasSuccessful, LogModularOnline, Warning,
				TEXT("The companion publication did not remove the player: %s. It still believes they are in the match."), *Result.ToLogString());
		}));
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
		// Not a platform that cannot publish: there is simply no match of ours to change.
		OnComplete.ExecuteIfBound(FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidState()));

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
	// Cleared on every path out of here, including the ones that cannot travel: an address left behind
	// would refuse every later request as one already in flight.
	const FString Address { MoveTemp(PendingTravelURL) };
	PendingTravelURL.Reset();

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

	if (Address.IsEmpty())
	{
		UE_LOG(LogModularOnline, Error, TEXT("The match was opened without a map to travel to."));

		return;
	}

	UE_LOG(LogModularOnline, Log, TEXT("Travelling to the hosted map: %s"), *Address);

	World->ServerTravel(Address);
}

void UModularMatchSubsystem::AnswerJoin(const FModularOnlineResult& Result, const FModularMatchOperationDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(Result);
	OnMatchJoined.Broadcast(Result);
	K2_OnMatchJoined.Broadcast(Result);
}

FModularOnlineResult UModularMatchSubsystem::ResolveJoinedMatch(const int32 LocalPlayerIndex, FString& OutTravelURL) const
{
	auto Role = EModularOnlineRole::Default;
	FModularMatchContext Context;
	FModularMatchHandle Match;

	if (!FindActiveMatch(LocalPlayerIndex, Role, Context, Match))
	{
		return FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidState());
	}

	UE::Online::FGetResolvedConnectString::Params Params;
	Params.LocalAccountId = Context.LocalAccount;
	Params.LobbyId = Match.LobbyId;
	Params.SessionId = Match.SessionId;

	const auto Resolved = Context.Services->GetResolvedConnectString(MoveTemp(Params));

	if (!Resolved.IsOk())
	{
		return FModularOnlineResult::FromOnlineError(Resolved.GetErrorValue());
	}

	const auto& Address = Resolved.GetOkValue().ResolvedConnectString;

	if (!PoFigGames::Online::Private::IsTravellableAddress(Address))
	{
		UE_LOG(LogModularOnline, Error, TEXT("The match answered with an address this client will not travel to: '%s'."), *Address);

		return FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidResults());
	}

	OutTravelURL = Address;

	return FModularOnlineResult::Success();
}

void UModularMatchSubsystem::TravelToJoinedMatch(const int32 LocalPlayerIndex, const FString& TravelURL)
{
	const auto GameInstance = GetGameInstance();
	const auto PlayerController = GameInstance ? GameInstance->GetLocalPlayerByIndex(LocalPlayerIndex) : nullptr;
	const auto Controller = PlayerController ? PlayerController->GetPlayerController(GetWorld()) : nullptr;

	if (!Controller)
	{
		UE_LOG(LogModularOnline, Error, TEXT("The match was joined, but there is no local player to travel."));

		return;
	}

	// The game gets the last word on the address: an encryption token and anything else only it knows
	// has to be on the URL before the travel, and there is no second chance afterwards.
	auto Address = TravelURL;
	OnPreClientTravel.Broadcast(Address);

	UE_LOG(LogModularOnline, Log, TEXT("Travelling to the joined match."));

	Controller->ClientTravel(Address, TRAVEL_Absolute);
}

void UModularMatchSubsystem::BindMatchEvents()
{
	const auto Online = GetOnline();

	if (bEventsBound || !Online)
	{
		return;
	}

	// Both publications are listened to: an invitation or a join asked for in the platform's own overlay
	// arrives on the platform role, and heard on the match role alone it would never reach the game.
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

			// The role travels with all of them: whether a member is the local player is a question about
			// the role whose lobby this is, and an account of one provider means nothing to another.
			MatchEventHandles.Add(Lobbies->OnLobbyJoined().Add(this, &ThisClass::HandleLobbyJoined, Named));
			MatchEventHandles.Add(Lobbies->OnLobbyLeft().Add(this, &ThisClass::HandleLobbyLeft, Named));
			MatchEventHandles.Add(Lobbies->OnLobbyMemberJoined().Add(this, &ThisClass::HandleLobbyMemberJoined, Named));
			MatchEventHandles.Add(Lobbies->OnLobbyMemberLeft().Add(this, &ThisClass::HandleLobbyMemberLeft, Named));

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

bool UModularMatchSubsystem::IsOurLobby(const UE::Online::FLobby& Lobby)
{
	// These events carry every lobby of the services, including one a project joined for something else.
	return Lobby.LocalName == PoFigGames::Online::Private::MatchLocalName;
}

bool UModularMatchSubsystem::IsLocalAccount(const EModularOnlineRole Role, const UE::Online::FAccountId& AccountId) const
{
	const auto Users = GetUsers();

	if (!Users || !AccountId.IsValid())
	{
		return false;
	}

	for (const auto User : Users->GetAllUsers())
	{
		if (User && User->GetAccountId(Role) == AccountId)
		{
			return true;
		}
	}

	return false;
}

void UModularMatchSubsystem::AnnounceDeparture(const EModularMatchLeaveReason Reason)
{
	if (bDepartureAnnounced)
	{
		return;
	}

	bDepartureAnnounced = true;

	OnMatchLeft.Broadcast(Reason);
	K2_OnMatchLeft.Broadcast(Reason);
}

void UModularMatchSubsystem::HandleLobbyJoined(const UE::Online::FLobbyJoined& EventParameters, const EModularOnlineRole /*Role*/)
{
	if (IsOurLobby(*EventParameters.Lobby))
	{
		bDepartureAnnounced = false;

		UE_LOG(LogModularOnline, Log, TEXT("Joined match %s."), *ToLogString(EventParameters.Lobby->LobbyId));
	}
}

void UModularMatchSubsystem::HandleLobbyLeft(const UE::Online::FLobbyLeft& EventParameters, const EModularOnlineRole /*Role*/)
{
	if (IsOurLobby(*EventParameters.Lobby))
	{
		UE_LOG(LogModularOnline, Log, TEXT("Left match %s."), *ToLogString(EventParameters.Lobby->LobbyId));

		// Says nothing about why, so it only answers when nothing else already did: a member event carries
		// the reason and arrives first.
		AnnounceDeparture(EModularMatchLeaveReason::Left);
	}
}

void UModularMatchSubsystem::HandleLobbyMemberJoined(const UE::Online::FLobbyMemberJoined& EventParameters, const EModularOnlineRole Role)
{
	if (IsOurLobby(*EventParameters.Lobby) && !IsLocalAccount(Role, EventParameters.Member->AccountId))
	{
		// Somebody arriving is not somebody leaving, and the reason a member event carries only means
		// anything when they went.
		const auto Member = MakeModularAccount(EventParameters.Member->AccountId);

		OnMemberJoined.Broadcast(Member, EModularMatchLeaveReason::None);
		K2_OnMemberJoined.Broadcast(Member, EModularMatchLeaveReason::None);
	}
}

void UModularMatchSubsystem::HandleLobbyMemberLeft(const UE::Online::FLobbyMemberLeft& EventParameters, const EModularOnlineRole Role)
{
	if (!IsOurLobby(*EventParameters.Lobby))
	{
		return;
	}

	const auto Reason = PoFigGames::Online::Private::FromLobbyLeaveReason(EventParameters.Reason);

	// The member who left being us is the one place that knows why the match ended, which the plain left
	// event cannot tell apart.
	if (IsLocalAccount(Role, EventParameters.Member->AccountId))
	{
		AnnounceDeparture(Reason);

		return;
	}

	// Once we are out, the services report everybody still in the lobby as having left it. Passing that
	// on would tell the game the match emptied when it was the game that walked away.
	if (!bDepartureAnnounced)
	{
		const auto MemberId = MakeModularAccount(EventParameters.Member->AccountId);

		OnMemberLeft.Broadcast(MemberId, Reason);
		K2_OnMemberLeft.Broadcast(MemberId, Reason);
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

bool UModularMatchSubsystem::OwnsFailedConnection(const UWorld* World, const UNetDriver* NetDriver) const
{
	if (World)
	{
		return World->GetGameInstance() == GetGameInstance();
	}

	// A connection that failed before a world existed is reported with none, and its owner is found
	// through the pending game of each context - which is how the engine finds it too
	// (UnrealEngine.cpp:15318-15331, checked on 2026-09-15).
	if (GEngine)
	{
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (Context.PendingNetGame && Context.PendingNetGame->NetDriver == NetDriver)
			{
				return Context.OwningGameInstance == GetGameInstance();
			}
		}
	}

	return false;
}

void UModularMatchSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, const ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// Only the driver carrying the game is a match. A beacon has its own failures, and a host losing one
	// client is that client's problem rather than the host's.
	if (NetDriver == nullptr
		|| (NetDriver->NetDriverName != NAME_GameNetDriver && NetDriver->NetDriverName != NAME_PendingNetDriver)
		|| NetDriver->GetNetMode() != NM_Client
		|| !OwnsFailedConnection(World, NetDriver))
	{
		return;
	}

	// FailureReceived is the server sending NMT_Failure - a refused login, a wrong password, a full server
	// - and not a kick: the engine kicks by destroying the controller, which reads here as a lost one.
	const auto Reason = FailureType == ENetworkFailure::FailureReceived
		? EModularMatchLeaveReason::Refused
		: EModularMatchLeaveReason::Disconnected;

	UE_LOG(LogModularOnline, Log, TEXT("The connection to the match failed (%s): %s"), ENetworkFailure::ToString(FailureType), *ErrorString);

	AnnounceDeparture(Reason);
}

void UModularMatchSubsystem::HandleTravelFailure(UWorld* World, const ETravelFailure::Type FailureType, const FString& ErrorString)
{
	// A server failing its own travel did not lose a match, it failed to open one, and it has nobody to
	// tell either way.
	if (bIsDedicatedServer || FailureType == ETravelFailure::ServerTravelFailure)
	{
		return;
	}

	if (!World || World->GetGameInstance() != GetGameInstance() || World->GetNetMode() == NM_ListenServer)
	{
		return;
	}

	// Travel fails for a mistyped address as readily as for a match, and only one of those is something
	// the player was in.
	auto Role = EModularOnlineRole::Default;
	FModularMatchContext Context;
	FModularMatchHandle Match;

	if (!FindActiveMatch(0, Role, Context, Match))
	{
		return;
	}

	UE_LOG(LogModularOnline, Log, TEXT("Travelling to the match failed (%s): %s"), ETravelFailure::ToString(FailureType), *ErrorString);

	AnnounceDeparture(EModularMatchLeaveReason::Disconnected);
}

void UModularMatchSubsystem::HandleSessionLeft(const UE::Online::FSessionLeft& EventParameters)
{
	UE_LOG(LogModularOnline, Log, TEXT("Left a session."));

	AnnounceDeparture(EModularMatchLeaveReason::Left);
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

FModularOnlineResult UModularMatchSubsystem::RefuseWhileTravelling() const
{
	if (PendingTravelURL.IsEmpty())
	{
		return FModularOnlineResult::Success();
	}

	// One address is kept at a time, and a second request would take the first one's place: the first
	// caller would then be answered with a success for a travel to somebody else's map.
	UE_LOG(LogModularOnline, Warning, TEXT("A match is already waiting to travel to '%s'; the new request was refused."), *PendingTravelURL);

	auto Refusal = FModularOnlineResult::FromOnlineError(UE::Online::Errors::AlreadyPending());
	Refusal.ErrorText = FText::FromStringTable(PoFigGames::Online::StringTableId, TEXT("MatchAlreadyTravelling"));

	return Refusal;
}

FModularOnlineResult UModularMatchSubsystem::BuildContextOrRefusal(const int32 LocalPlayerIndex, const EModularOnlineRole Role,
	const TSharedPtr<PoFigGames::Online::IModularMatchBackend>& InBackend, FModularMatchContext& OutContext) const
{
	if (!InBackend.IsValid())
	{
		return GetMissingBackendResult();
	}

	if (!BuildContext(LocalPlayerIndex, Role, OutContext))
	{
		auto Refusal = FModularOnlineResult::FromOnlineError(UE::Online::Errors::NotLoggedIn());

		Refusal.ErrorText = bIsDedicatedServer
			? FText::FromStringTable(PoFigGames::Online::StringTableId, TEXT("ServerNotSignedIn"))
			: FText::FromStringTable(PoFigGames::Online::StringTableId, TEXT("PlayerNotSignedIn"));

		return Refusal;
	}

	return FModularOnlineResult::Success();
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

