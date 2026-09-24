// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "GameFramework/OnlineReplStructs.h"
#include "Match/ModularMatchSubsystem.h"
#include "User/ModularUserInfo.h"
#include "User/ModularUserSubsystem.h"

#include "ModularOnlineTestProbes.generated.h"


/**
 * @class UModularMergeMatchesProbe
 *
 * @brief Reaches the protected merge of two searches, which is what a browser on both roles shows.
 */
UCLASS()
class UModularMergeMatchesProbe : public UModularMatchSubsystem
{
	GENERATED_BODY()

public:

	/** The merge itself, so that a test can put two answers in and read the list a browser would show. */
	void Merge(TArray<FModularMatchInfo>& Matches, const TArray<FModularMatchInfo>& Companion, const int32 MaxResults) const
	{
		MergeMatches(Matches, Companion, MaxResults);
	}
};


/**
 * @class UModularLoginFlowProbe
 *
 * @brief Reaches the rules the login walks by, so that they can be asked without a provider behind them.
 */
UCLASS()
class UModularLoginFlowProbe : public UModularUserSubsystem
{
	GENERATED_BODY()

public:

	/** Which steps a login would run, given what the caller asked for. */
	EModularStepPolicy PolicyOf(const EModularLoginStep Step, const FModularLoginParams& Params) const
	{
		return GetStepPolicy(Step, Params);
	}

	/** Whether a player who failed to sign in may sit as a guest of the primary one. */
	bool WouldBecomeGuest(const int32 LocalPlayerIndex, const bool bAllowGuest, const EModularOnlineErrorCategory Category, const EModularLoginStep FailedStep)
	{
		const auto User = NewObject<UModularUserInfo>(this);
		User->LocalPlayerIndex = LocalPlayerIndex;

		const auto Request = MakeShared<FLoginRequest>();
		Request->User = User;
		Request->Params.bAllowGuest = bAllowGuest;
		Request->FailedStep = FailedStep;

		FModularOnlineResult Failure;
		Failure.bWasSuccessful = false;
		Failure.Category = Category;
		Request->Failure = Failure;

		return CanBecomeGuest(Request);
	}
};


/**
 * @struct FModularAccountIdProbe
 *
 * @brief An account id held the way APlayerState::UniqueId holds one, so a test can ask Iris how it replicates.
 */
USTRUCT()
struct FModularAccountIdProbe
{
	GENERATED_BODY()

	UPROPERTY()
	FUniqueNetIdRepl Id { };
};
