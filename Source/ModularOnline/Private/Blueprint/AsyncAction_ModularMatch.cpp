// Copyright PoFig Games Studio. All Rights Reserved.

#include "Blueprint/AsyncAction_ModularMatch.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Match/ModularMatchSubsystem.h"
#include "Core/ModularOnlineTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AsyncAction_ModularMatch)

namespace PoFigGames::Online::Private
{
	/** The match layer of the game instance a node was called from, or null outside of one. */
	static UModularMatchSubsystem* FindMatches(const UObject* WorldContextObject, UGameInstance*& OutGameInstance)
	{
		const auto World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
		OutGameInstance = World ? World->GetGameInstance() : nullptr;

		return OutGameInstance ? OutGameInstance->GetSubsystem<UModularMatchSubsystem>() : nullptr;
	}

	/** What a node answers with when it never reached the match layer. */
	static FModularOnlineResult NoMatchLayer()
	{
		return FModularOnlineResult::NotSupported(ModularOnlineTags::Feature_Lobbies);
	}
}

UAsyncAction_ModularHostMatch* UAsyncAction_ModularHostMatch::HostMatch(const UObject* WorldContextObject, FModularMatchSettings Settings, const int32 LocalPlayerIndex)
{
	UGameInstance* GameInstance { nullptr };
	const auto Subsystem = PoFigGames::Online::Private::FindMatches(WorldContextObject, GameInstance);

	const auto Action = MakeNode<UAsyncAction_ModularHostMatch>(GameInstance);

	Action->Matches = Subsystem;
	Action->MatchSettings = MoveTemp(Settings);
	Action->PlayerIndex = LocalPlayerIndex;

	return Action;
}

void UAsyncAction_ModularHostMatch::Activate()
{
	const auto Subsystem = Matches.Get();
	if (!Subsystem)
	{
		FinishWithResult(PoFigGames::Online::Private::NoMatchLayer());

		return;
	}

	const auto OnComplete = FModularMatchOperationDelegate::CreateWeakLambda(this, [this](const FModularOnlineResult& Result)
	{
		FinishWithResult(Result);
	});

	Subsystem->HostMatch(PlayerIndex, MatchSettings, OnComplete);
}

UAsyncAction_ModularJoinMatch* UAsyncAction_ModularJoinMatch::JoinMatch(const UObject* WorldContextObject, FModularMatchHandle Match, const int32 LocalPlayerIndex)
{
	UGameInstance* GameInstance { nullptr };
	const auto Subsystem = PoFigGames::Online::Private::FindMatches(WorldContextObject, GameInstance);

	const auto Action = MakeNode<UAsyncAction_ModularJoinMatch>(GameInstance);

	Action->Matches = Subsystem;
	Action->Handle = MoveTemp(Match);
	Action->PlayerIndex = LocalPlayerIndex;

	return Action;
}

void UAsyncAction_ModularJoinMatch::Activate()
{
	const auto Subsystem = Matches.Get();
	if (!Subsystem)
	{
		FinishWithResult(PoFigGames::Online::Private::NoMatchLayer());

		return;
	}

	const auto OnComplete = FModularMatchOperationDelegate::CreateWeakLambda(this, [this](const FModularOnlineResult& Result)
	{
		FinishWithResult(Result);
	});

	Subsystem->JoinMatch(PlayerIndex, Handle, OnComplete);
}

UAsyncAction_ModularFindMatches* UAsyncAction_ModularFindMatches::FindMatches(const UObject* WorldContextObject, FModularMatchSearchParams Params, const int32 LocalPlayerIndex)
{
	UGameInstance* GameInstance { nullptr };
	const auto Subsystem = PoFigGames::Online::Private::FindMatches(WorldContextObject, GameInstance);

	const auto Action = MakeNode<UAsyncAction_ModularFindMatches>(GameInstance);

	Action->Matches = Subsystem;
	Action->SearchParams = MoveTemp(Params);
	Action->PlayerIndex = LocalPlayerIndex;

	return Action;
}

void UAsyncAction_ModularFindMatches::Activate()
{
	const auto Subsystem = Matches.Get();
	if (!Subsystem)
	{
		Answer([this] { OnFailure.Broadcast(TArray<FModularMatchInfo> { }, PoFigGames::Online::Private::NoMatchLayer()); });

		return;
	}

	const auto OnComplete = FModularMatchSearchDelegate::CreateWeakLambda(this, [this](const FModularOnlineResult& Result, const TArray<FModularMatchInfo>& Found)
	{
		Answer([this, &Result, &Found]
		{
			if (Result.bWasSuccessful)
			{
				OnSuccess.Broadcast(Found, Result);
			}
			else
			{
				OnFailure.Broadcast(Found, Result);
			}
		});
	});

	Subsystem->FindMatches(PlayerIndex, SearchParams, OnComplete);
}
