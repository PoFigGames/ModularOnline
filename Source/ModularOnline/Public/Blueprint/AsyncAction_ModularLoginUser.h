// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Blueprint/AsyncAction_ModularOnline.h"
#include "User/ModularUserTypes.h"

#include "AsyncAction_ModularLoginUser.generated.h"

class UModularUserSubsystem;


/**
 * @class UAsyncAction_ModularLoginUser
 *
 * @brief Signs a local player in from a Blueprint graph.
 */
UCLASS(MinimalAPI)
class UAsyncAction_ModularLoginUser : public UAsyncAction_ModularOnlineResultBase
{
	GENERATED_BODY()

public:
	/** Signs the local player in, running as many login steps as the platform needs. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|User", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static MODULARONLINE_API UAsyncAction_ModularLoginUser* LoginLocalUser(const UObject* WorldContextObject, FModularLoginParams Params);

#pragma region UBlueprintAsyncActionBase

	MODULARONLINE_API virtual void Activate() override;

#pragma endregion UBlueprintAsyncActionBase

private:
	/** The subsystem that will run the login, found when the node was created. */
	TWeakObjectPtr<UModularUserSubsystem> UserSubsystem { };

	/** What the graph asked for. */
	FModularLoginParams LoginParams { };
};
