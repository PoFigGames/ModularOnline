// Copyright PoFig Games Studio. All Rights Reserved.

#include "Blueprint/AsyncAction_ModularLoginUser.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Core/ModularOnlineStringTable.h"
#include "Core/ModularOnlineTags.h"
#include "User/ModularUserSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AsyncAction_ModularLoginUser)

UAsyncAction_ModularLoginUser* UAsyncAction_ModularLoginUser::LoginLocalUser(const UObject* WorldContextObject, FModularLoginParams Params)
{
	const auto World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const auto GameInstance = World ? World->GetGameInstance() : nullptr;

	// With no game instance MakeNode hands back a node already retired, not one waiting for an Activate
	// that will never come.
	const auto Action = MakeNode<UAsyncAction_ModularLoginUser>(GameInstance);

	Action->UserSubsystem = GameInstance ? GameInstance->GetSubsystem<UModularUserSubsystem>() : nullptr;
	Action->LoginParams = MoveTemp(Params);

	return Action;
}

void UAsyncAction_ModularLoginUser::Activate()
{
	const auto Subsystem = UserSubsystem.Get();
	if (!Subsystem)
	{
		FinishWithResult(FModularOnlineResult::NotSupported(ModularOnlineTags::Feature_Auth));

		return;
	}

	const auto OnComplete = FModularUserLoginCompleteDelegate::CreateWeakLambda(this, [this](const UModularUserInfo* /*User*/, const FModularOnlineResult& Result)
	{
		FinishWithResult(Result);
	});

	if (!Subsystem->LoginLocalUser(LoginParams, OnComplete))
	{
		// The request never started: a bad index, or a login already running for this player. The reason
		// is in the log, and the graph gets the failure pin rather than a node that never answers.
		auto Result = FModularOnlineResult::Success();
		Result.bWasSuccessful = false;
		Result.Category = EModularOnlineErrorCategory::InvalidState;
		Result.ErrorId = TEXT("login_request_refused");
		Result.ErrorText = FText::FromStringTable(PoFigGames::Online::StringTableId, TEXT("LoginRequestRefused"));

		FinishWithResult(Result);
	}
}
