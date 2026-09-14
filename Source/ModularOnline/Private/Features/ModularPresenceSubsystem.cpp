// Copyright PoFig Games Studio. All Rights Reserved.

#include "Features/ModularPresenceSubsystem.h"

#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineSettings.h"
#include "Core/ModularOnlineTags.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineResult.h"
#include "Online/Presence.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularPresenceSubsystem)

namespace PoFigGames::Online::Private
{
	/** The status of the services that matches the one the game speaks. */
	static UE::Online::EUserPresenceStatus ToOnlineStatus(const EModularPresenceStatus Status)
	{
		switch (Status)
		{
		case EModularPresenceStatus::Offline:
			return UE::Online::EUserPresenceStatus::Offline;

		case EModularPresenceStatus::Online:
			return UE::Online::EUserPresenceStatus::Online;

		case EModularPresenceStatus::Away:
			return UE::Online::EUserPresenceStatus::Away;

		case EModularPresenceStatus::ExtendedAway:
			return UE::Online::EUserPresenceStatus::ExtendedAway;

		case EModularPresenceStatus::DoNotDisturb:
			return UE::Online::EUserPresenceStatus::DoNotDisturb;

		case EModularPresenceStatus::Unknown:
			break;
		}

		return UE::Online::EUserPresenceStatus::Unknown;
	}

	/** What a presence property looks like as text. */
	static FString PropertyToString(const UE::Online::FPresenceProperty& Value)
	{
		if (Value.IsType<FString>())
		{
			return Value.Get<FString>();
		}

		if (Value.IsType<int64>())
		{
			return ::LexToString(Value.Get<int64>());
		}

		if (Value.IsType<double>())
		{
			return ::LexToString(Value.Get<double>());
		}

		if (Value.IsType<bool>())
		{
			return ::LexToString(Value.Get<bool>());
		}

		return FString { };
	}

	/** And back again. */
	static EModularPresenceStatus FromOnlineStatus(const UE::Online::EUserPresenceStatus Status)
	{
		switch (Status)
		{
		case UE::Online::EUserPresenceStatus::Offline:
			return EModularPresenceStatus::Offline;

		case UE::Online::EUserPresenceStatus::Online:
			return EModularPresenceStatus::Online;

		case UE::Online::EUserPresenceStatus::Away:
			return EModularPresenceStatus::Away;

		case UE::Online::EUserPresenceStatus::ExtendedAway:
			return EModularPresenceStatus::ExtendedAway;

		case UE::Online::EUserPresenceStatus::DoNotDisturb:
			return EModularPresenceStatus::DoNotDisturb;

		case UE::Online::EUserPresenceStatus::Unknown:
			break;
		}

		return EModularPresenceStatus::Unknown;
	}
}

void UModularPresenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Subscribed on the first call rather than here, for the same reason the contexts are built then:
	// there is no world, and so no services instance of this game instance, while subsystems start.
}

void UModularPresenceSubsystem::Deinitialize()
{
	PresenceUpdatedHandle = UE::Online::FOnlineEventDelegateHandle { };
	bListeningToPresence = false;

	Super::Deinitialize();
}

FGameplayTag UModularPresenceSubsystem::GetFeatureTag() const
{
	return ModularOnlineTags::Feature_Presence;
}

FModularPresence UModularPresenceSubsystem::Describe(const UE::Online::FUserPresence& Presence)
{
	FModularPresence Described;
	Described.AccountId = MakeModularAccount(Presence.AccountId);
	Described.Status = PoFigGames::Online::Private::FromOnlineStatus(Presence.Status);
	Described.StatusText = Presence.StatusString;
	Described.RichPresence = Presence.RichPresenceString;

	// Anything but a private or unknown match is one somebody could ask to join; whether they are allowed
	// is the match's own answer, not this one.
	Described.bIsJoinable = Presence.Joinability == UE::Online::EUserPresenceJoinability::Public
		|| Presence.Joinability == UE::Online::EUserPresenceJoinability::FriendsOnly
		|| Presence.Joinability == UE::Online::EUserPresenceJoinability::InviteOnly;

	for (const auto& Property : Presence.Properties)
	{
		Described.Properties.Emplace(Property.Key, PoFigGames::Online::Private::PropertyToString(Property.Value));
	}

	return Described;
}

void UModularPresenceSubsystem::HandlePresenceUpdated(const UE::Online::FPresenceUpdated& EventParameters)
{
	const auto Updated = Describe(*EventParameters.UpdatedPresence);

	OnPresenceUpdated.Broadcast(Updated);
	K2_OnPresenceUpdated.Broadcast(Updated);
}

namespace PoFigGames::Online::Private
{
	/** The joinability a provider understands, from the one a project configures. */
	static UE::Online::EUserPresenceJoinability ToOnlineJoinability(const EModularPresenceJoinability Joinability)
	{
		switch (Joinability)
		{
			case EModularPresenceJoinability::Public:
				return UE::Online::EUserPresenceJoinability::Public;

			case EModularPresenceJoinability::FriendsOnly:
				return UE::Online::EUserPresenceJoinability::FriendsOnly;

			case EModularPresenceJoinability::InviteOnly:
				return UE::Online::EUserPresenceJoinability::InviteOnly;

			case EModularPresenceJoinability::Private:
				return UE::Online::EUserPresenceJoinability::Private;

			case EModularPresenceJoinability::Unknown:
				break;
		}

		return UE::Online::EUserPresenceJoinability::Unknown;
	}
}

bool UModularPresenceSubsystem::PublishState(const int32 LocalPlayerIndex, const FGameplayTag State, const TMap<FString, FString>& Values)
{
	const auto Settings = GetDefault<UModularPresenceSettings>();
	const auto Described = Settings ? Settings->FindState(State) : nullptr;

	if (!Described)
	{
		UE_LOG(LogModularOnline, Warning, TEXT("Presence state '%s' is not described in the presence settings, so nothing was published for it."), *State.ToString());

		return false;
	}

	// A configured value may name a supplied one in braces; the braces go round each key once here rather
	// than once per property.
	TArray<TPair<FString, FString>> Replacements;
	Replacements.Reserve(Values.Num());

	for (const auto& Value : Values)
	{
		if (!Value.Value.IsEmpty())
		{
			Replacements.Emplace(FString::Printf(TEXT("{%s}"), *Value.Key), Value.Value);
		}
	}

	TMap<FString, FString> Properties;
	Properties.Reserve(Described->Properties.Num());

	for (const auto& Property : Described->Properties)
	{
		auto Resolved = Property.Value;

		for (const auto& Replacement : Replacements)
		{
			Resolved.ReplaceInline(*Replacement.Key, *Replacement.Value, ESearchCase::CaseSensitive);
		}

		// A placeholder nobody answered leaves its key out rather than publishing braces at the player;
		// a value that merely contains a brace is published as written.
		int32 Opening;

		const auto bLeftUnanswered = Resolved.FindChar(TEXT('{'), Opening)
			&& Resolved.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Opening) != INDEX_NONE;

		if (!bLeftUnanswered)
		{
			Properties.Emplace(Property.Key.ToString(), MoveTemp(Resolved));
		}
	}

	return SetPresence(LocalPlayerIndex, EModularPresenceStatus::Online, Described->Status, Properties, Described->Joinability);
}

bool UModularPresenceSubsystem::SetPresence(const int32 LocalPlayerIndex, const EModularPresenceStatus Status, const FString& StatusText, const TMap<FString, FString>& Properties, const EModularPresenceJoinability Joinability)
{
	const auto Presence = GetInterface<UE::Online::IPresence>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!Presence.IsValid() || !Account.IsValid())
	{
		UE_LOG(LogModularOnline, Verbose, TEXT("Presence was not published: %s."),
			Presence.IsValid() ? TEXT("the player is not signed in") : TEXT("this provider has no presence"));

		return false;
	}

	if (ShouldStartListening(bListeningToPresence))
	{
		PresenceUpdatedHandle = Presence->OnPresenceUpdated().Add(this, &ThisClass::HandlePresenceUpdated);
		bListeningToPresence = true;
	}

	UE::Online::FPartialUpdatePresence::Params Params;
	Params.LocalAccountId = Account;
	Params.Mutations.StatusString.Emplace(StatusText);
	Params.Mutations.Status.Emplace(PoFigGames::Online::Private::ToOnlineStatus(Status));

	// What decides whether a friend's client offers a "join game" beside this player. Left out entirely
	// when nothing was said, so that a state which does not speak about joining does not answer for it.
	if (Joinability != EModularPresenceJoinability::Unknown)
	{
		Params.Mutations.Joinability.Emplace(PoFigGames::Online::Private::ToOnlineJoinability(Joinability));
	}

	for (const auto& Property : Properties)
	{
		Params.Mutations.UpdatedProperties.AddVariant(Property.Key, Property.Value);
	}

	UE_LOG(LogModularOnline, Verbose, TEXT("Publishing presence for player %d: '%s' with %d propert%s."),
		LocalPlayerIndex, *StatusText, Properties.Num(), Properties.Num() == 1 ? TEXT("y") : TEXT("ies"));

	Presence->PartialUpdatePresence(MoveTemp(Params)).OnComplete(this, [](const UE::Online::TOnlineResult<UE::Online::FPartialUpdatePresence>& Result)
	{
		// Nobody waits on presence and nothing downstream changes when it fails, which is exactly why a
		// refusal has to reach the log: otherwise a friends list quietly showing the wrong thing has no
		// trace anywhere at all.
		if (Result.IsError())
		{
			UE_LOG(LogModularOnline, Warning, TEXT("Presence was refused by the services: %s"), *ToLogString(Result.GetErrorValue()));
		}
	});

	return true;
}

bool UModularPresenceSubsystem::QueryPresence(const int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId, FModularPresenceDelegate OnComplete)
{
	const auto Presence = GetInterface<UE::Online::IPresence>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);
	const auto Target = TargetAccountId.AccountId;

	if (!Presence.IsValid())
	{
		OnComplete.ExecuteIfBound(FModularPresence { }, MissingFeature());

		return false;
	}

	if (!Account.IsValid() || !Target.IsValid())
	{
		OnComplete.ExecuteIfBound(FModularPresence { }, NotSignedIn());

		return false;
	}

	if (ShouldStartListening(bListeningToPresence))
	{
		PresenceUpdatedHandle = Presence->OnPresenceUpdated().Add(this, &ThisClass::HandlePresenceUpdated);
		bListeningToPresence = true;
	}

	UE::Online::FQueryPresence::Params Params;
	Params.LocalAccountId = Account;
	Params.TargetAccountId = Target;

	// Asked to keep listening: a friends list that only ever sees one answer goes stale the moment
	// somebody starts a match.
	Params.bListenToChanges = true;

	Presence->QueryPresence(MoveTemp(Params)).OnComplete(this, [OnComplete](const UE::Online::TOnlineResult<UE::Online::FQueryPresence>& Result)
	{
		if (Result.IsError())
		{
			OnComplete.ExecuteIfBound(FModularPresence { }, FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));

			return;
		}

		OnComplete.ExecuteIfBound(Describe(*Result.GetOkValue().Presence), FModularOnlineResult::Success());
	});

	return true;
}

bool UModularPresenceSubsystem::GetCachedPresence(const int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId, FModularPresence& OutPresence) const
{
	const auto Presence = GetInterface<UE::Online::IPresence>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);
	const auto Target = TargetAccountId.AccountId;

	if (!Presence.IsValid() || !Account.IsValid() || !Target.IsValid())
	{
		return false;
	}

	const auto Cached = Presence->GetCachedPresence({ Account, Target });
	if (!Cached.IsOk())
	{
		return false;
	}

	OutPresence = Describe(*Cached.GetOkValue().Presence);

	return true;
}
