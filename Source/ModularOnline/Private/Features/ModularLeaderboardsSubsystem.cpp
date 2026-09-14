// Copyright PoFig Games Studio. All Rights Reserved.

#include "Features/ModularLeaderboardsSubsystem.h"

#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineTags.h"
#include "Online/Leaderboards.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineResult.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularLeaderboardsSubsystem)

namespace PoFigGames::Online::Private
{
	/** The lines of a table as a screen reads them. */
	static TArray<FModularLeaderboardEntry> DescribeEntries(const TArray<UE::Online::FLeaderboardEntry>& Entries)
	{
		TArray<FModularLeaderboardEntry> Described;
		Described.Reserve(Entries.Num());

		for (const auto& Entry : Entries)
		{
			FModularLeaderboardEntry Line;
			Line.AccountId = MakeModularAccount(Entry.AccountId);
			Line.Rank = Entry.Rank;
			Line.Score = Entry.Score;

			Described.Add(MoveTemp(Line));
		}

		return Described;
	}
}

FGameplayTag UModularLeaderboardsSubsystem::GetFeatureTag() const
{
	return ModularOnlineTags::Feature_Leaderboards;
}

bool UModularLeaderboardsSubsystem::CanRead(const TSharedPtr<UE::Online::ILeaderboards>& Leaderboards, const UE::Online::FAccountId& Account,
	const FModularLeaderboardDelegate& OnComplete) const
{
	if (!Leaderboards.IsValid())
	{
		OnComplete.ExecuteIfBound(TArray<FModularLeaderboardEntry> { }, MissingFeature());

		return false;
	}

	if (!Account.IsValid())
	{
		OnComplete.ExecuteIfBound(TArray<FModularLeaderboardEntry> { }, NotSignedIn());

		return false;
	}

	return true;
}

bool UModularLeaderboardsSubsystem::ReadAroundPlayer(const int32 LocalPlayerIndex, const FString& BoardName, const int32 Offset, const int32 Limit, FModularLeaderboardDelegate OnComplete)
{
	const auto Leaderboards = GetInterface<UE::Online::ILeaderboards>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!CanRead(Leaderboards, Account, OnComplete))
	{
		return false;
	}

	UE::Online::FReadEntriesAroundUser::Params Params;
	Params.LocalAccountId = Account;
	Params.AccountId = Account;
	Params.Offset = Offset;
	Params.Limit = static_cast<uint32>(FMath::Max(1, Limit));
	Params.BoardName = BoardName;

	Leaderboards->ReadEntriesAroundUser(MoveTemp(Params)).OnComplete(this, [OnComplete](const UE::Online::TOnlineResult<UE::Online::FReadEntriesAroundUser>& Result)
	{
		if (Result.IsError())
		{
			OnComplete.ExecuteIfBound(TArray<FModularLeaderboardEntry> { }, FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));

			return;
		}

		OnComplete.ExecuteIfBound(PoFigGames::Online::Private::DescribeEntries(Result.GetOkValue().Entries), FModularOnlineResult::Success());
	});

	return true;
}

bool UModularLeaderboardsSubsystem::ReadAroundRank(const int32 LocalPlayerIndex, const FString& BoardName, const int32 Rank, const int32 Limit, FModularLeaderboardDelegate OnComplete)
{
	const auto Leaderboards = GetInterface<UE::Online::ILeaderboards>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!CanRead(Leaderboards, Account, OnComplete))
	{
		return false;
	}

	UE::Online::FReadEntriesAroundRank::Params Params;
	Params.LocalAccountId = Account;
	Params.Rank = static_cast<uint32>(FMath::Max(0, Rank));
	Params.Limit = static_cast<uint32>(FMath::Max(1, Limit));
	Params.BoardName = BoardName;

	Leaderboards->ReadEntriesAroundRank(MoveTemp(Params)).OnComplete(this, [OnComplete](const UE::Online::TOnlineResult<UE::Online::FReadEntriesAroundRank>& Result)
	{
		if (Result.IsError())
		{
			OnComplete.ExecuteIfBound(TArray<FModularLeaderboardEntry> { }, FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));

			return;
		}

		OnComplete.ExecuteIfBound(PoFigGames::Online::Private::DescribeEntries(Result.GetOkValue().Entries), FModularOnlineResult::Success());
	});

	return true;
}

bool UModularLeaderboardsSubsystem::ReadForPlayers(const int32 LocalPlayerIndex, const FString& BoardName, const TArray<FModularAccountHandle>& AccountIds, FModularLeaderboardDelegate OnComplete)
{
	const auto Leaderboards = GetInterface<UE::Online::ILeaderboards>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!CanRead(Leaderboards, Account, OnComplete))
	{
		return false;
	}

	UE::Online::FReadEntriesForUsers::Params Params;
	Params.LocalAccountId = Account;
	Params.BoardName = BoardName;
	Params.AccountIds.Reserve(AccountIds.Num());

	for (const auto& AccountId : AccountIds)
	{
		if (AccountId.IsValid())
		{
			Params.AccountIds.Add(AccountId.AccountId);
		}
	}

	if (Params.AccountIds.IsEmpty())
	{
		// Nobody this provider knows was asked about; an empty table is the honest answer.
		OnComplete.ExecuteIfBound(TArray<FModularLeaderboardEntry> { }, FModularOnlineResult::Success());

		return true;
	}

	Leaderboards->ReadEntriesForUsers(MoveTemp(Params)).OnComplete(this, [OnComplete](const UE::Online::TOnlineResult<UE::Online::FReadEntriesForUsers>& Result)
	{
		if (Result.IsError())
		{
			OnComplete.ExecuteIfBound(TArray<FModularLeaderboardEntry> { }, FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));

			return;
		}

		OnComplete.ExecuteIfBound(PoFigGames::Online::Private::DescribeEntries(Result.GetOkValue().Entries), FModularOnlineResult::Success());
	});

	return true;
}
