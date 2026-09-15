// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Core/ModularOnlineTypes.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Templates/Function.h"

#include "AsyncAction_ModularOnline.generated.h"

class UGameInstance;
class UModularOnlineSubsystem;


/** How every asynchronous node of this plugin answers: one result, on one of three pins. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularOnlineActionCompleted, const FModularOnlineResult&, Result);


/**
 * @class UAsyncAction_ModularOnlineBase
 *
 * @brief Base of every Blueprint node of this plugin.
 */
UCLASS(MinimalAPI, Abstract)
class UAsyncAction_ModularOnlineBase : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

protected:
	/** Broadcasts whatever this node answers with, once, and retires it. Safe to call more than once. */
	MODULARONLINE_API void Answer(TFunctionRef<void()> Broadcast);

	/** A node tied to the game instance it was called from, or one already retired when there is none. */
	template <typename ActionType>
	static ActionType* MakeNode(UGameInstance* GameInstance)
	{
		const auto Action = NewObject<ActionType>();

		if (GameInstance)
		{
			Action->RegisterWithGameInstance(GameInstance);
		}
		else
		{
			Action->SetReadyToDestroy();
		}

		return Action;
	}

	/** True once the node has answered, so that a late callback cannot broadcast a second time. */
	bool bHasFinished { false };
};


/**
 * @class UAsyncAction_ModularOnlineResultBase
 *
 * @brief A node whose whole answer is one result, on one of three pins.
 */
UCLASS(MinimalAPI, Abstract)
class UAsyncAction_ModularOnlineResultBase : public UAsyncAction_ModularOnlineBase
{
	GENERATED_BODY()

public:
	/** The operation did what was asked. */
	UPROPERTY(BlueprintAssignable)
	FModularOnlineActionCompleted OnSuccess { };

	/** The operation failed; Result says why, and ErrorText can be shown to the player. */
	UPROPERTY(BlueprintAssignable)
	FModularOnlineActionCompleted OnFailure { };

	/** The provider of this platform implements no such component. Retrying will not change that. */
	UPROPERTY(BlueprintAssignable)
	FModularOnlineActionCompleted OnNotSupported { };

protected:
	/** Broadcasts on the pin the result calls for and retires the node. Safe to call more than once. */
	MODULARONLINE_API void FinishWithResult(const FModularOnlineResult& Result);
};
