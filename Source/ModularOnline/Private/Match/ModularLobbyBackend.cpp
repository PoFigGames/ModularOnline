// Copyright PoFig Games Studio. All Rights Reserved.

#include "Match/ModularLobbyBackend.h"

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
		/** What a lobby attribute looks like once it is a string, whatever the host published it as. */
		static FString VariantToString(const UE::Online::FSchemaVariant& Value)
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
		static UE::Online::ELobbyJoinPolicy ToLobbyJoinPolicy(const EModularMatchJoinPolicy JoinPolicy)
		{
			switch (JoinPolicy)
			{
			case EModularMatchJoinPolicy::PublicNotAdvertised:
				return UE::Online::ELobbyJoinPolicy::PublicNotAdvertised;

			case EModularMatchJoinPolicy::InvitationOnly:
				return UE::Online::ELobbyJoinPolicy::InvitationOnly;

			case EModularMatchJoinPolicy::PublicAdvertised:
				break;
			}

			return UE::Online::ELobbyJoinPolicy::PublicAdvertised;
		}

		/** The attributes a match publishes about itself, named as the project's schema names them. */
		static TMap<UE::Online::FSchemaAttributeId, UE::Online::FSchemaVariant> BuildAttributes(const FModularMatchSettings& Settings)
		{
			TMap<UE::Online::FSchemaAttributeId, UE::Online::FSchemaVariant> Attributes;

			if (const auto ConfiguredSettings = GetDefault<UModularMatchBackendSettings>())
			{
				if (!ConfiguredSettings->MatchMapAttribute.IsNone())
				{
					Attributes.Emplace(ConfiguredSettings->MatchMapAttribute, Settings.MapDisplayName);
				}

				// The one attribute a provider is asked to search on, and the only way a browser can tell
				// how full a match is before joining it.
				if (!ConfiguredSettings->MatchNameAttribute.IsNone() && !Settings.Name.IsEmpty())
				{
					Attributes.Emplace(ConfiguredSettings->MatchNameAttribute, Settings.Name);
				}
			}

			for (const auto& Attribute : Settings.Attributes)
			{
				Attributes.Emplace(Attribute.Key, Attribute.Value);
			}

			return Attributes;
		}

		/** Answers a caller that asked for something the lobbies cannot do here. */
		static void AnswerNotSupported(FModularMatchOperationDelegate& OnComplete)
		{
			OnComplete.ExecuteIfBound(FModularOnlineResult::NotSupported(ModularOnlineTags::Feature_Lobbies));
		}

		/** The lobbies of a context, or nothing when the provider has none. */
		static UE::Online::ILobbiesPtr GetLobbies(const FModularMatchContext& Context)
		{
			return Context.Services.IsValid() ? Context.Services->GetLobbiesInterface() : nullptr;
		}
	}

	FGameplayTag FModularLobbyBackend::GetRequiredFeature() const
	{
		return ModularOnlineTags::Feature_Lobbies;
	}

	FModularMatchInfo FModularLobbyBackend::DescribeLobby(const UE::Online::FLobby& Lobby)
	{
		FModularMatchInfo Info;
		Info.Handle.LobbyId = Lobby.LobbyId;
		Info.Handle.Id = ToLogString(Lobby.LobbyId);
		Info.MaxPlayers = Lobby.MaxMembers;

		// Which attribute carries which of these is configuration, and the same for every attribute of the
		// lobby, so it is read once rather than asked about inside the loop.
		const auto Settings = GetDefault<UModularMatchBackendSettings>();
		const auto NameAttribute = Settings ? Settings->MatchNameAttribute : FName { };
		const auto MapAttribute = Settings ? Settings->MatchMapAttribute : FName { };
		const auto MemberCountAttribute = Settings ? Settings->MatchMemberCountAttribute : FName { };

		// Members are only listed for a lobby this player has joined, so counting them would show every
		// match in a browser as empty. What the owner published is read below and wins where it is there.
		auto Taken = Lobby.Members.Num();

		for (const auto& Attribute : Lobby.Attributes)
		{
			const auto& Value = Info.Attributes.Emplace(Attribute.Key, Private::VariantToString(Attribute.Value));

			if (Attribute.Key == NameAttribute)
			{
				Info.Name = Value;
			}
			else if (Attribute.Key == MapAttribute)
			{
				Info.MapName = Value;
			}
			else if (Attribute.Key == MemberCountAttribute)
			{
				Taken = FMath::Max(Taken, FCString::Atoi(*Value));
			}
		}

		Info.OpenSlots = FMath::Max(0, Lobby.MaxMembers - Taken);

		return Info;
	}

	TSharedPtr<const UE::Online::FLobby> FModularLobbyBackend::FindJoinedLobby(const FModularMatchContext& Context)
	{
		const auto Lobbies = Private::GetLobbies(Context);
		if (!Lobbies.IsValid() || !Context.IsValid())
		{
			return nullptr;
		}

		const auto Joined = Lobbies->GetJoinedLobbies({ Context.LocalAccount });
		if (!Joined.IsOk())
		{
			return nullptr;
		}

		for (const auto& Lobby : Joined.GetOkValue().Lobbies)
		{
			if (Lobby->LocalName == Context.LocalName)
			{
				return Lobby;
			}
		}

		return nullptr;
	}

	bool FModularLobbyBackend::GetCurrentMatch(const FModularMatchContext& Context, FModularMatchHandle& OutMatch) const
	{
		if (const auto Lobby = FindJoinedLobby(Context))
		{
			OutMatch = DescribeLobby(*Lobby).Handle;

			return true;
		}

		return false;
	}

	void FModularLobbyBackend::CreateMatch(const FModularMatchContext& Context, const FModularMatchSettings& Settings, FModularMatchOperationDelegate OnComplete)
	{
		const auto Lobbies = Private::GetLobbies(Context);
		if (!Lobbies.IsValid() || !Context.IsValid())
		{
			Private::AnswerNotSupported(OnComplete);

			return;
		}

		const auto ConfiguredSettings = GetDefault<UModularMatchBackendSettings>();

		UE::Online::FCreateLobby::Params Params;
		Params.LocalAccountId = Context.LocalAccount;
		Params.LocalName = Context.LocalName;
		Params.SchemaId = UE::Online::FSchemaId(ConfiguredSettings ? FName(*ConfiguredSettings->LobbySchemaId) : FName(TEXT("GameLobby")));
		Params.bPresenceEnabled = Settings.bUsePresence;
		Params.MaxMembers = Settings.MaxPlayers;
		Params.JoinPolicy = Private::ToLobbyJoinPolicy(Settings.JoinPolicy);
		Params.Attributes = Private::BuildAttributes(Settings);

		// Kept for the failure below: a service refuses an attribute its schema does not name, and answers
		// with InvalidParams and nothing else. Saying which names were offered turns an afternoon into a
		// minute, because the answer is always "that one is not in the schema".
		TStringBuilder<256> Offered;

		for (const auto& Attribute : Params.Attributes)
		{
			if (Offered.Len())
			{
				Offered.Append(TEXT(", "));
			}

			Offered.Append(Attribute.Key.ToString());
		}

		const FString OfferedNames { Offered.ToString() };

		Lobbies->CreateLobby(MoveTemp(Params)).OnComplete([OnComplete, OfferedNames](const UE::Online::TOnlineResult<UE::Online::FCreateLobby>& Result)
		{
			UE_CLOG(!Result.IsOk(), LogModularOnline, Error, TEXT("The match was refused (%s). Attributes offered: %s. An attribute the schema of this provider does not name is refused rather than ignored."),
				*ToLogString(Result.GetErrorValue()), *OfferedNames);

			OnComplete.ExecuteIfBound(Result.IsOk() ? FModularOnlineResult::Success() : FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));
		});
	}

	void FModularLobbyBackend::FindMatches(const FModularMatchContext& Context, const FModularMatchSearchParams& Params, FModularMatchSearchDelegate OnComplete)
	{
		const auto Lobbies = Private::GetLobbies(Context);
		if (!Lobbies.IsValid() || !Context.IsValid())
		{
			OnComplete.ExecuteIfBound(FModularOnlineResult::NotSupported(ModularOnlineTags::Feature_Lobbies), TArray<FModularMatchInfo> { });

			return;
		}

		UE::Online::FFindLobbies::Params FindParams;
		FindParams.LocalAccountId = Context.LocalAccount;
		FindParams.MaxResults = static_cast<uint32>(FMath::Max(1, Params.MaxResults));

		for (const auto& Required : Params.RequiredAttributes)
		{
			FindParams.Filters.Emplace(UE::Online::FFindLobbySearchFilter { Required.Key, UE::Online::ESchemaAttributeComparisonOp::Equals, Required.Value });
		}

		Lobbies->FindLobbies(MoveTemp(FindParams)).OnComplete([OnComplete](const UE::Online::TOnlineResult<UE::Online::FFindLobbies>& Result)
		{
			if (Result.IsError())
			{
				OnComplete.ExecuteIfBound(FModularOnlineResult::FromOnlineError(Result.GetErrorValue()), TArray<FModularMatchInfo> { });

				return;
			}

			TArray<FModularMatchInfo> Matches;
			Matches.Reserve(Result.GetOkValue().Lobbies.Num());

			for (const auto& Lobby : Result.GetOkValue().Lobbies)
			{
				Matches.Add(DescribeLobby(*Lobby));
			}

			OnComplete.ExecuteIfBound(FModularOnlineResult::Success(), Matches);
		});
	}

	void FModularLobbyBackend::JoinMatch(const FModularMatchContext& Context, const FModularMatchHandle& Match, const bool bUsePresence, FModularMatchOperationDelegate OnComplete)
	{
		const auto Lobbies = Private::GetLobbies(Context);
		if (!Lobbies.IsValid() || !Context.IsValid())
		{
			Private::AnswerNotSupported(OnComplete);

			return;
		}

		if (!Match.LobbyId.IsValid())
		{
			// A handle from a session cannot be joined through the lobbies; the layer above picks the
			// backend, so reaching here means the two disagreed.
			OnComplete.ExecuteIfBound(FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidParams()));

			return;
		}

		UE::Online::FJoinLobby::Params Params;
		Params.LocalAccountId = Context.LocalAccount;
		Params.LocalName = Context.LocalName;
		Params.LobbyId = Match.LobbyId;
		Params.bPresenceEnabled = bUsePresence;

		Lobbies->JoinLobby(MoveTemp(Params)).OnComplete([OnComplete](const UE::Online::TOnlineResult<UE::Online::FJoinLobby>& Result)
		{
			OnComplete.ExecuteIfBound(Result.IsOk() ? FModularOnlineResult::Success() : FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));
		});
	}

	void FModularLobbyBackend::LeaveMatch(const FModularMatchContext& Context, FModularMatchOperationDelegate OnComplete)
	{
		const auto Lobbies = Private::GetLobbies(Context);
		const auto Lobby = FindJoinedLobby(Context);

		if (!Lobbies.IsValid() || !Lobby.IsValid())
		{
			// Not being in a match is the state the caller wanted; saying so as a failure would only make
			// every caller special case it.
			OnComplete.ExecuteIfBound(FModularOnlineResult::Success());

			return;
		}

		Lobbies->LeaveLobby({ Context.LocalAccount, Lobby->LobbyId }).OnComplete([OnComplete](const UE::Online::TOnlineResult<UE::Online::FLeaveLobby>& Result)
		{
			OnComplete.ExecuteIfBound(Result.IsOk() ? FModularOnlineResult::Success() : FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));
		});
	}

	void FModularLobbyBackend::InviteToMatch(const FModularMatchContext& Context, const UE::Online::FAccountId& TargetAccount, FModularMatchOperationDelegate OnComplete)
	{
		const auto Lobbies = Private::GetLobbies(Context);
		const auto Lobby = FindJoinedLobby(Context);

		if (!Lobbies.IsValid() || !Lobby.IsValid())
		{
			OnComplete.ExecuteIfBound(FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidState()));

			return;
		}

		Lobbies->InviteLobbyMember({ Context.LocalAccount, Lobby->LobbyId, TargetAccount }).OnComplete([OnComplete](const UE::Online::TOnlineResult<UE::Online::FInviteLobbyMember>& Result)
		{
			OnComplete.ExecuteIfBound(Result.IsOk() ? FModularOnlineResult::Success() : FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));
		});
	}

	void FModularLobbyBackend::KickMember(const FModularMatchContext& Context, const UE::Online::FAccountId& TargetAccount, FModularMatchOperationDelegate OnComplete)
	{
		const auto Lobbies = Private::GetLobbies(Context);
		const auto Lobby = FindJoinedLobby(Context);

		if (!Lobbies.IsValid() || !Lobby.IsValid())
		{
			OnComplete.ExecuteIfBound(FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidState()));

			return;
		}

		Lobbies->KickLobbyMember({ Context.LocalAccount, Lobby->LobbyId, TargetAccount }).OnComplete([OnComplete](const UE::Online::TOnlineResult<UE::Online::FKickLobbyMember>& Result)
		{
			OnComplete.ExecuteIfBound(Result.IsOk() ? FModularOnlineResult::Success() : FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));
		});
	}

	void FModularLobbyBackend::UpdateSettings(const FModularMatchContext& Context, const FModularMatchSettings& Settings, FModularMatchOperationDelegate OnComplete)
	{
		const auto Lobbies = Private::GetLobbies(Context);
		const auto Lobby = FindJoinedLobby(Context);

		if (!Lobbies.IsValid() || !Lobby.IsValid())
		{
			OnComplete.ExecuteIfBound(FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidState()));

			return;
		}

		UE::Online::FModifyLobbyAttributes::Params AttributeParams;
		AttributeParams.LocalAccountId = Context.LocalAccount;
		AttributeParams.LobbyId = Lobby->LobbyId;
		AttributeParams.UpdatedAttributes = Private::BuildAttributes(Settings);

		// An attribute the new settings no longer name is taken off, or the previous match stays advertised
		// beside the current one. What the provider publishes about the lobby itself is not ours to remove:
		// taking it off either has the update refused or blanks what a search filters on.
		const auto Configured = GetDefault<UModularMatchBackendSettings>();
		const auto Reserved = Configured ? Configured->ReservedAttributePrefix : FString { };

		for (const auto& Published : Lobby->Attributes)
		{
			const auto bStillPublished = AttributeParams.UpdatedAttributes.Contains(Published.Key);
			const auto bBelongsToProvider = !Reserved.IsEmpty() && Published.Key.ToString().StartsWith(Reserved);

			if (!bStillPublished && !bBelongsToProvider)
			{
				AttributeParams.RemovedAttributes.Add(Published.Key);
			}
		}

		const auto JoinPolicy = Private::ToLobbyJoinPolicy(Settings.JoinPolicy);
		const auto LobbyId = Lobby->LobbyId;
		const auto LocalAccount = Context.LocalAccount;
		const auto LobbiesPtr = Lobbies;

		LobbiesPtr->ModifyLobbyAttributes(MoveTemp(AttributeParams)).OnComplete([OnComplete, LobbiesPtr, LobbyId, LocalAccount, JoinPolicy](const UE::Online::TOnlineResult<UE::Online::FModifyLobbyAttributes>& Result)
		{
			if (Result.IsError())
			{
				OnComplete.ExecuteIfBound(FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));

				return;
			}

			// Who may join is not an attribute but a setting of its own, so it takes a second call.
			LobbiesPtr->ModifyLobbyJoinPolicy({ LocalAccount, LobbyId, JoinPolicy }).OnComplete([OnComplete](const UE::Online::TOnlineResult<UE::Online::FModifyLobbyJoinPolicy>& PolicyResult)
			{
				OnComplete.ExecuteIfBound(PolicyResult.IsOk() ? FModularOnlineResult::Success() : FModularOnlineResult::FromOnlineError(PolicyResult.GetErrorValue()));
			});
		});
	}
}
