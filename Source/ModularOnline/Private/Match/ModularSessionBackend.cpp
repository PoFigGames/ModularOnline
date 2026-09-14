// Copyright PoFig Games Studio. All Rights Reserved.

#include "Match/ModularSessionBackend.h"

#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineSettings.h"
#include "Core/ModularOnlineTags.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineErrorDefinitions.h"
#include "Online/OnlineResult.h"

namespace PoFigGames::Online
{
	namespace Private
	{
		/** What a session setting looks like once it is a string, whatever the host published it as. */
		static FString SessionVariantToString(const UE::Online::FSchemaVariant& Value)
		{
			switch (Value.VariantType)
			{
			case UE::Online::ESchemaAttributeType::String:
				return Value.GetString();

			case UE::Online::ESchemaAttributeType::Int64:
				return ::LexToString(Value.GetInt64());

			case UE::Online::ESchemaAttributeType::Double:
				return ::LexToString(Value.GetDouble());

			case UE::Online::ESchemaAttributeType::Bool:
				return ::LexToString(Value.GetBoolean());

			case UE::Online::ESchemaAttributeType::None:
				break;
			}

			return FString { };
		}

		/** The join policy of the services that matches the one the game asked for. */
		static UE::Online::ESessionJoinPolicy ToSessionJoinPolicy(const EModularMatchJoinPolicy JoinPolicy)
		{
			switch (JoinPolicy)
			{
			case EModularMatchJoinPolicy::PublicNotAdvertised:
				// Sessions have no "anyone with the address, but not listed": the nearest thing a provider
				// offers is friends only, which is narrower. Said out loud, because a host who asked for
				// one and got the other would otherwise never find out.
				UE_LOG(LogModularOnline, Warning, TEXT("Sessions cannot publish a match unlisted; it is limited to friends instead."));

				return UE::Online::ESessionJoinPolicy::FriendsOnly;

			case EModularMatchJoinPolicy::InvitationOnly:
				return UE::Online::ESessionJoinPolicy::InviteOnly;

			case EModularMatchJoinPolicy::PublicAdvertised:
				break;
			}

			return UE::Online::ESessionJoinPolicy::Public;
		}

		/** The settings a match publishes about itself, named as the project's schema names them. */
		static UE::Online::FSessionSettings BuildSessionSettings(const FModularMatchSettings& Settings)
		{
			UE::Online::FSessionSettings SessionSettings;
			SessionSettings.NumMaxConnections = static_cast<uint32>(FMath::Max(1, Settings.MaxPlayers));
			SessionSettings.JoinPolicy = ToSessionJoinPolicy(Settings.JoinPolicy);
			SessionSettings.bAllowNewMembers = true;

			if (const auto ConfiguredSettings = GetDefault<UModularMatchBackendSettings>())
			{
				SessionSettings.SchemaName = FName(*ConfiguredSettings->SessionSchemaId);

				if (!ConfiguredSettings->MatchNameAttribute.IsNone() && !Settings.Name.IsEmpty())
				{
					SessionSettings.CustomSettings.Emplace(ConfiguredSettings->MatchNameAttribute, UE::Online::FCustomSessionSetting { Settings.Name, UE::Online::ESchemaAttributeVisibility::Public });
				}

				if (!ConfiguredSettings->MatchMapAttribute.IsNone())
				{
					SessionSettings.CustomSettings.Emplace(ConfiguredSettings->MatchMapAttribute, UE::Online::FCustomSessionSetting { Settings.MapDisplayName, UE::Online::ESchemaAttributeVisibility::Public });
				}
			}

			for (const auto& Attribute : Settings.Attributes)
			{
				SessionSettings.CustomSettings.Emplace(Attribute.Key, UE::Online::FCustomSessionSetting { Attribute.Value, UE::Online::ESchemaAttributeVisibility::Public });
			}

			return SessionSettings;
		}

		/** The sessions of a context, or nothing when the provider has none. */
		static UE::Online::ISessionsPtr GetSessions(const FModularMatchContext& Context)
		{
			return Context.Services.IsValid() ? Context.Services->GetSessionsInterface() : nullptr;
		}

		/** Answers a caller that asked for something the sessions cannot do here. */
		static void AnswerNoSessions(FModularMatchOperationDelegate& OnComplete)
		{
			OnComplete.ExecuteIfBound(FModularOnlineResult::NotSupported(ModularOnlineTags::Feature_Sessions));
		}
	}

	FGameplayTag FModularSessionBackend::GetRequiredFeature() const
	{
		return ModularOnlineTags::Feature_Sessions;
	}

	FModularMatchInfo FModularSessionBackend::DescribeSession(const UE::Online::ISession& Session)
	{
		const auto& SessionSettings = Session.GetSessionSettings();

		FModularMatchInfo Info;
		Info.Handle.SessionId = Session.GetSessionId();
		Info.Handle.Id = ToLogString(Session.GetSessionId());
		Info.MaxPlayers = static_cast<int32>(SessionSettings.NumMaxConnections);
		Info.OpenSlots = static_cast<int32>(Session.GetNumOpenConnections());

		const auto Settings = GetDefault<UModularMatchBackendSettings>();

		// Which attribute carries which of these is configuration, and the same for every setting of the
		// session, so it is read once rather than asked about inside the loop.
		const auto NameAttribute = Settings ? Settings->MatchNameAttribute : FName { };
		const auto MapAttribute = Settings ? Settings->MatchMapAttribute : FName { };

		for (const auto& Setting : SessionSettings.CustomSettings)
		{
			const auto& Value = Info.Attributes.Emplace(Setting.Key, Private::SessionVariantToString(Setting.Value.Data));

			if (Setting.Key == NameAttribute)
			{
				Info.Name = Value;
			}
			else if (Setting.Key == MapAttribute)
			{
				Info.MapName = Value;
			}
		}

		return Info;
	}

	TSharedPtr<const UE::Online::ISession> FModularSessionBackend::FindOwnSession(const FModularMatchContext& Context)
	{
		const auto Sessions = Private::GetSessions(Context);
		if (!Sessions.IsValid() || Context.LocalName.IsNone())
		{
			return nullptr;
		}

		const auto Found = Sessions->GetSessionByName({ Context.LocalName });

		return Found.IsOk() ? Found.GetOkValue().Session.ToSharedPtr() : nullptr;
	}

	bool FModularSessionBackend::GetCurrentMatch(const FModularMatchContext& Context, FModularMatchHandle& OutMatch) const
	{
		if (const auto Session = FindOwnSession(Context))
		{
			OutMatch = DescribeSession(*Session).Handle;

			return true;
		}

		return false;
	}

	void FModularSessionBackend::CreateMatch(const FModularMatchContext& Context, const FModularMatchSettings& Settings, FModularMatchOperationDelegate OnComplete)
	{
		const auto Sessions = Private::GetSessions(Context);
		if (!Sessions.IsValid() || !Context.IsValid())
		{
			Private::AnswerNoSessions(OnComplete);

			return;
		}

		UE::Online::FCreateSession::Params Params;
		Params.LocalAccountId = Context.LocalAccount;
		Params.SessionName = Context.LocalName;
		Params.bPresenceEnabled = Settings.bUsePresence;
		Params.bIsLANSession = Settings.OnlineMode == EModularMatchOnlineMode::LAN;
		Params.SessionSettings = Private::BuildSessionSettings(Settings);

		Sessions->CreateSession(MoveTemp(Params)).OnComplete([OnComplete](const UE::Online::TOnlineResult<UE::Online::FCreateSession>& Result)
		{
			OnComplete.ExecuteIfBound(Result.IsOk() ? FModularOnlineResult::Success() : FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));
		});
	}

	void FModularSessionBackend::FindMatches(const FModularMatchContext& Context, const FModularMatchSearchParams& Params, FModularMatchSearchDelegate OnComplete)
	{
		const auto Sessions = Private::GetSessions(Context);
		if (!Sessions.IsValid() || !Context.IsValid())
		{
			OnComplete.ExecuteIfBound(FModularOnlineResult::NotSupported(ModularOnlineTags::Feature_Sessions), TArray<FModularMatchInfo> { });

			return;
		}

		UE::Online::FFindSessions::Params FindParams;
		FindParams.LocalAccountId = Context.LocalAccount;
		FindParams.MaxResults = static_cast<uint32>(FMath::Max(1, Params.MaxResults));

		for (const auto& Required : Params.RequiredAttributes)
		{
			FindParams.Filters.Emplace(UE::Online::FFindSessionsSearchFilter { Required.Key, UE::Online::ESchemaAttributeComparisonOp::Equals, Required.Value });
		}

		Sessions->FindSessions(MoveTemp(FindParams)).OnComplete([OnComplete, Sessions](const UE::Online::TOnlineResult<UE::Online::FFindSessions>& Result)
		{
			if (Result.IsError())
			{
				OnComplete.ExecuteIfBound(FModularOnlineResult::FromOnlineError(Result.GetErrorValue()), TArray<FModularMatchInfo> { });

				return;
			}

			// A search answers with ids; what they stand for is asked of the services one by one, which
			// is how the sessions interface is meant to be read.
			TArray<FModularMatchInfo> Matches;
			Matches.Reserve(Result.GetOkValue().FoundSessionIds.Num());

			for (const auto& SessionId : Result.GetOkValue().FoundSessionIds)
			{
				if (const auto Session = Sessions->GetSessionById({ SessionId }); Session.IsOk())
				{
					Matches.Add(DescribeSession(*Session.GetOkValue().Session));
				}
			}

			OnComplete.ExecuteIfBound(FModularOnlineResult::Success(), Matches);
		});
	}

	void FModularSessionBackend::JoinMatch(const FModularMatchContext& Context, const FModularMatchHandle& Match, const bool bUsePresence, FModularMatchOperationDelegate OnComplete)
	{
		const auto Sessions = Private::GetSessions(Context);
		if (!Sessions.IsValid() || !Context.IsValid())
		{
			Private::AnswerNoSessions(OnComplete);

			return;
		}

		if (!Match.SessionId.IsValid())
		{
			// A handle from a lobby cannot be joined through the sessions; the layer above picks the
			// backend, so reaching here means the two disagreed.
			OnComplete.ExecuteIfBound(FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidParams()));

			return;
		}

		UE::Online::FJoinSession::Params Params;
		Params.LocalAccountId = Context.LocalAccount;
		Params.SessionName = Context.LocalName;
		Params.SessionId = Match.SessionId;
		Params.bPresenceEnabled = bUsePresence;

		Sessions->JoinSession(MoveTemp(Params)).OnComplete([OnComplete](const UE::Online::TOnlineResult<UE::Online::FJoinSession>& Result)
		{
			OnComplete.ExecuteIfBound(Result.IsOk() ? FModularOnlineResult::Success() : FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));
		});
	}

	void FModularSessionBackend::LeaveMatch(const FModularMatchContext& Context, FModularMatchOperationDelegate OnComplete)
	{
		const auto Sessions = Private::GetSessions(Context);
		const auto Session = FindOwnSession(Context);

		if (!Sessions.IsValid() || !Session.IsValid())
		{
			// Not being in a match is the state the caller wanted.
			OnComplete.ExecuteIfBound(FModularOnlineResult::Success());

			return;
		}

		UE::Online::FLeaveSession::Params Params;
		Params.LocalAccountId = Context.LocalAccount;
		Params.SessionName = Context.LocalName;

		// The owner takes the session with them; anybody else only leaves it.
		Params.bDestroySession = Session->GetOwnerAccountId() == Context.LocalAccount;

		Sessions->LeaveSession(MoveTemp(Params)).OnComplete([OnComplete](const UE::Online::TOnlineResult<UE::Online::FLeaveSession>& Result)
		{
			OnComplete.ExecuteIfBound(Result.IsOk() ? FModularOnlineResult::Success() : FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));
		});
	}

	void FModularSessionBackend::InviteToMatch(const FModularMatchContext& Context, const UE::Online::FAccountId& TargetAccount, FModularMatchOperationDelegate OnComplete)
	{
		const auto Sessions = Private::GetSessions(Context);
		if (!Sessions.IsValid() || !FindOwnSession(Context).IsValid())
		{
			OnComplete.ExecuteIfBound(FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidState()));

			return;
		}

		UE::Online::FSendSessionInvite::Params Params;
		Params.LocalAccountId = Context.LocalAccount;
		Params.SessionName = Context.LocalName;
		Params.TargetUsers = { TargetAccount };

		Sessions->SendSessionInvite(MoveTemp(Params)).OnComplete([OnComplete](const UE::Online::TOnlineResult<UE::Online::FSendSessionInvite>& Result)
		{
			OnComplete.ExecuteIfBound(Result.IsOk() ? FModularOnlineResult::Success() : FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));
		});
	}

	void FModularSessionBackend::KickMember(const FModularMatchContext& Context, const UE::Online::FAccountId& TargetAccount, FModularMatchOperationDelegate OnComplete)
	{
		// The sessions interface has no notion of removing somebody else: a session is a record, not a
		// room, and who is in it is decided by the server that owns it. A game that needs this kicks the
		// player off the server, and the record follows.
		OnComplete.ExecuteIfBound(FModularOnlineResult::NotSupported(ModularOnlineTags::Feature_Sessions));
	}

	void FModularSessionBackend::UpdateSettings(const FModularMatchContext& Context, const FModularMatchSettings& Settings, FModularMatchOperationDelegate OnComplete)
	{
		const auto Sessions = Private::GetSessions(Context);
		const auto Session = FindOwnSession(Context);

		if (!Sessions.IsValid() || !Session.IsValid())
		{
			OnComplete.ExecuteIfBound(FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidState()));

			return;
		}

		const auto NewSettings = Private::BuildSessionSettings(Settings);

		UE::Online::FUpdateSessionSettings::Params Params;
		Params.LocalAccountId = Context.LocalAccount;
		Params.SessionName = Context.LocalName;
		Params.Mutations.NumMaxConnections = NewSettings.NumMaxConnections;
		Params.Mutations.JoinPolicy = NewSettings.JoinPolicy;
		Params.Mutations.UpdatedCustomSettings = NewSettings.CustomSettings;

		// A setting the match no longer names comes off, or the previous match's mode and map stay
		// advertised beside the current one. What the provider keeps under its own reserved names is left
		// alone, exactly as it is for a lobby.
		const auto Configured = GetDefault<UModularMatchBackendSettings>();
		const auto Reserved = Configured ? Configured->ReservedAttributePrefix : FString { };

		for (const auto& Published : Session->GetSessionSettings().CustomSettings)
		{
			const auto bStillPublished = NewSettings.CustomSettings.Contains(Published.Key);
			const auto bBelongsToProvider = !Reserved.IsEmpty() && Published.Key.ToString().StartsWith(Reserved);

			if (!bStillPublished && !bBelongsToProvider)
			{
				Params.Mutations.RemovedCustomSettings.Add(Published.Key);
			}
		}

		Sessions->UpdateSessionSettings(MoveTemp(Params)).OnComplete([OnComplete](const UE::Online::TOnlineResult<UE::Online::FUpdateSessionSettings>& Result)
		{
			OnComplete.ExecuteIfBound(Result.IsOk() ? FModularOnlineResult::Success() : FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));
		});
	}
}
