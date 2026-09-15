// Copyright PoFig Games Studio. All Rights Reserved.

#include "Core/ModularOnlineTypes.h"

#include "Online/OnlineError.h"
#include "Online/OnlineErrorDefinitions.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularOnlineTypes)

#define LOCTEXT_NAMESPACE "ModularOnline"

namespace PoFigGames::Online::Private
{
	/** Maps an error of the online services onto the category a screen branches on. */
	static EModularOnlineErrorCategory CategoriseError(const UE::Online::FOnlineError& InError)
	{
		using namespace UE::Online;

		if (InError == Errors::NotImplemented() || InError == Errors::MissingInterface())
		{
			return EModularOnlineErrorCategory::NotSupported;
		}

		if (InError == Errors::NotLoggedIn() || InError == Errors::InvalidUser() || InError == Errors::InvalidAuth() || InError == Errors::InvalidCreds())
		{
			return EModularOnlineErrorCategory::NotLoggedIn;
		}

		if (InError == Errors::NoConnection())
		{
			return EModularOnlineErrorCategory::NoConnection;
		}

		if (InError == Errors::AccessDenied())
		{
			return EModularOnlineErrorCategory::AccessDenied;
		}

		if (InError == Errors::InvalidState() || InError == Errors::InvalidParams() || InError == Errors::AlreadyPending())
		{
			return EModularOnlineErrorCategory::InvalidState;
		}

		if (InError == Errors::Cancelled())
		{
			return EModularOnlineErrorCategory::Cancelled;
		}

		if (InError == Errors::Timeout())
		{
			return EModularOnlineErrorCategory::TimedOut;
		}

		return EModularOnlineErrorCategory::Unknown;
	}
}

FModularOnlineResult FModularOnlineResult::Success()
{
	return FModularOnlineResult { };
}

FModularOnlineResult FModularOnlineResult::NotSupported(const FGameplayTag Feature)
{
	FModularOnlineResult Result { };
	Result.bWasSuccessful = false;
	Result.Category = EModularOnlineErrorCategory::NotSupported;
	const auto NotImplemented = UE::Online::Errors::NotImplemented();
	Result.ErrorId = NotImplemented.GetErrorId();
	Result.ErrorText = FText::Format(LOCTEXT("FeatureNotSupported", "{0} is not available on this platform."), FText::FromName(Feature.GetTagName()));
	Result.MissingFeature = Feature;

	return Result;
}

FModularOnlineResult FModularOnlineResult::FromOnlineError(const UE::Online::FOnlineError& InError)
{
	if (InError == UE::Online::Errors::Success())
	{
		return Success();
	}

	FModularOnlineResult Result { };
	Result.bWasSuccessful = false;
	Result.Category = PoFigGames::Online::Private::CategoriseError(InError);
	Result.ErrorId = InError.GetErrorId();
	Result.ErrorText = InError.GetText();

	return Result;
}

FString FModularOnlineResult::ToLogString() const
{
	if (bWasSuccessful)
	{
		return TEXT("Success");
	}

	return FString::Printf(TEXT("%s (%s)"), *ErrorId, *PoFigGames::Online::LexToString(Category));
}

#undef LOCTEXT_NAMESPACE
