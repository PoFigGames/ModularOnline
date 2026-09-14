// Copyright PoFig Games Studio. All Rights Reserved.

#include "Blueprint/AsyncAction_ModularOnline.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AsyncAction_ModularOnline)

void UAsyncAction_ModularOnlineBase::Answer(TFunctionRef<void()> Broadcast)
{
	if (bHasFinished)
	{
		return;
	}

	bHasFinished = true;

	Broadcast();

	SetReadyToDestroy();
}

void UAsyncAction_ModularOnlineResultBase::FinishWithResult(const FModularOnlineResult& Result)
{
	Answer([this, &Result]
	{
		if (Result.bWasSuccessful)
		{
			OnSuccess.Broadcast(Result);
		}
		else if (Result.Category == EModularOnlineErrorCategory::NotSupported)
		{
			OnNotSupported.Broadcast(Result);
		}
		else
		{
			OnFailure.Broadcast(Result);
		}
	});
}
