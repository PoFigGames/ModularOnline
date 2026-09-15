// Copyright PoFig Games Studio. All Rights Reserved.

#include "Features/ModularStoreSubsystem.h"

#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineSettings.h"
#include "Core/ModularOnlineTags.h"
#include "Online/Commerce.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineErrorDefinitions.h"
#include "Online/OnlineResult.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularStoreSubsystem)

void UModularStoreSubsystem::AnnounceOffers(const TArray<FModularStoreOffer>& Offers, const FModularOnlineResult& Result, const FModularStoreOffersDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(Offers, Result);
	OnOffersQueried.Broadcast(Offers, Result);
	K2_OnOffersQueried.Broadcast(Offers, Result);
}

void UModularStoreSubsystem::AnnounceEntitlements(const TArray<FModularEntitlement>& Entitlements, const FModularOnlineResult& Result, const FModularEntitlementsDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(Entitlements, Result);
	OnEntitlementsQueried.Broadcast(Entitlements, Result);
	K2_OnEntitlementsQueried.Broadcast(Entitlements, Result);
}

void UModularStoreSubsystem::AnnounceCheckout(const FString& TransactionId, const FModularOnlineResult& Result, const FModularCheckoutDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(TransactionId, Result);
	OnCheckoutAnswered.Broadcast(TransactionId, Result);
	K2_OnCheckoutAnswered.Broadcast(TransactionId, Result);
}

void UModularStoreSubsystem::AnnounceRedeemed(const FModularOnlineResult& Result, const FModularStoreOperationDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(Result);
	OnRedeemAnswered.Broadcast(Result);
	K2_OnRedeemAnswered.Broadcast(Result);
}

namespace PoFigGames::Online::Private
{
	/** One offer as a screen reads it, with the formatted prices kept as the store wrote them. */
	static FModularStoreOffer DescribeOffer(const UE::Online::FOffer& Offer)
	{
		FModularStoreOffer Described;
		Described.OfferId = Offer.OfferId;
		Described.Title = Offer.Title;
		Described.Description = Offer.Description;
		Described.LongDescription = Offer.LongDescription;
		Described.FormattedPrice = Offer.FormattedPrice;
		Described.FormattedRegularPrice = Offer.FormattedRegularPrice;
		Described.CurrencyCode = Offer.CurrencyCode;
		Described.Price = static_cast<int64>(Offer.Price);
		Described.RegularPrice = static_cast<int64>(Offer.RegularPrice);
		Described.PriceDecimalPoint = Offer.PriceDecimalPoint;
		Described.PurchaseLimit = Offer.PurchaseLimit;
		Described.bHasReleaseDate = Offer.ReleaseDate.IsSet();
		Described.ReleaseDate = Offer.ReleaseDate.Get(FDateTime { });
		Described.bHasExpirationDate = Offer.ExpirationDate.IsSet();
		Described.ExpirationDate = Offer.ExpirationDate.Get(FDateTime { });
		Described.AdditionalData = Offer.AdditionalData;

		return Described;
	}

	/** One grant as a screen reads it. */
	static FModularEntitlement DescribeEntitlement(const UE::Online::FEntitlement& Entitlement)
	{
		FModularEntitlement Described;
		Described.EntitlementId = Entitlement.EntitlementId;
		Described.EntitlementType = Entitlement.EntitlementType.ToString();
		Described.ProductId = Entitlement.ProductId;
		Described.bRedeemed = Entitlement.bRedeemed;
		Described.Quantity = Entitlement.Quantity;
		Described.bHasAcquiredDate = Entitlement.AcquiredDate.IsSet();
		Described.AcquiredDate = Entitlement.AcquiredDate.Get(FDateTime { });
		Described.bHasExpiryDate = Entitlement.ExpiryDate.IsSet();
		Described.ExpiryDate = Entitlement.ExpiryDate.Get(FDateTime { });

		return Described;
	}
}

void UModularStoreSubsystem::Deinitialize()
{
	PurchaseCompletedHandle = UE::Online::FOnlineEventDelegateHandle { };
	bListeningToPurchases = false;

	Super::Deinitialize();
}

FGameplayTag UModularStoreSubsystem::GetFeatureTag() const
{
	return ModularOnlineTags::Feature_Commerce;
}

EModularOnlineRole UModularStoreSubsystem::GetFeatureRole() const
{
	const auto Settings = GetDefault<UModularOnlineSettings>();

	return Settings ? Settings->StoreRole : EModularOnlineRole::Platform;
}

void UModularStoreSubsystem::EnsureListening()
{
	const auto Commerce = GetInterface<UE::Online::ICommerce>();

	if (!ShouldStartListening(bListeningToPurchases) || !Commerce.IsValid())
	{
		return;
	}

	PurchaseCompletedHandle = Commerce->OnPurchaseCompleted().Add(this, &ThisClass::HandlePurchaseCompleted);
	bListeningToPurchases = true;
}

void UModularStoreSubsystem::HandlePurchaseCompleted(const UE::Online::FCommerceOnPurchaseComplete& EventParameters)
{
	const auto AccountId = MakeModularAccount(EventParameters.LocalAccountId);
	const auto TransactionId = EventParameters.TransactionId.Get(FString { });

	OnPurchaseCompleted.Broadcast(AccountId, TransactionId);
	K2_OnPurchaseCompleted.Broadcast(AccountId, TransactionId);
}

bool UModularStoreSubsystem::QueryOffers(const int32 LocalPlayerIndex, const TArray<FString>& OfferIds, FModularStoreOffersDelegate OnComplete)
{
	const auto Commerce = GetInterface<UE::Online::ICommerce>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!Commerce.IsValid())
	{
		AnnounceOffers(TArray<FModularStoreOffer> { }, MissingFeature(), OnComplete);

		return false;
	}

	if (!Account.IsValid())
	{
		AnnounceOffers(TArray<FModularStoreOffer> { }, NotSignedIn(), OnComplete);

		return false;
	}

	EnsureListening();

	// Asking for named offers and asking for the catalogue are two different calls, and both answer with
	// ids rather than offers: the offers themselves are read back from the cache the query filled.
	const auto AnswerFromCache = [this, LocalPlayerIndex, OnComplete](const UE::Online::FOnlineError* Error)
	{
		if (Error)
		{
			AnnounceOffers(TArray<FModularStoreOffer> { }, FModularOnlineResult::FromOnlineError(*Error), OnComplete);

			return;
		}

		TArray<FModularStoreOffer> Offers;
		GetOffers(LocalPlayerIndex, Offers);

		AnnounceOffers(Offers, FModularOnlineResult::Success(), OnComplete);
	};

	if (OfferIds.IsEmpty())
	{
		UE::Online::FCommerceQueryOffers::Params Params;
		Params.LocalAccountId = Account;

		Commerce->QueryOffers(MoveTemp(Params)).OnComplete(this, [this, AnswerFromCache](const UE::Online::TOnlineResult<UE::Online::FCommerceQueryOffers>& Result)
		{
			AnswerFromCache(Result.IsError() ? &Result.GetErrorValue() : nullptr);
		});

		return true;
	}

	UE::Online::FCommerceQueryOffersById::Params Params;
	Params.LocalAccountId = Account;
	Params.OfferIds = OfferIds;

	Commerce->QueryOffersById(MoveTemp(Params)).OnComplete(this, [this, AnswerFromCache](const UE::Online::TOnlineResult<UE::Online::FCommerceQueryOffersById>& Result)
	{
		AnswerFromCache(Result.IsError() ? &Result.GetErrorValue() : nullptr);
	});

	return true;
}

bool UModularStoreSubsystem::GetOffers(const int32 LocalPlayerIndex, TArray<FModularStoreOffer>& OutOffers) const
{
	const auto Commerce = GetInterface<UE::Online::ICommerce>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!Commerce.IsValid() || !Account.IsValid())
	{
		return false;
	}

	UE::Online::FCommerceGetOffers::Params Params;
	Params.LocalAccountId = Account;

	const auto Cached = Commerce->GetOffers(MoveTemp(Params));
	if (Cached.IsError())
	{
		return false;
	}

	OutOffers.Reset();
	OutOffers.Reserve(Cached.GetOkValue().Offers.Num());

	for (const auto& Offer : Cached.GetOkValue().Offers)
	{
		OutOffers.Add(PoFigGames::Online::Private::DescribeOffer(Offer));
	}

	return true;
}

bool UModularStoreSubsystem::Checkout(const int32 LocalPlayerIndex, const TArray<FModularPurchaseLine>& Lines, FModularCheckoutDelegate OnComplete)
{
	const auto Commerce = GetInterface<UE::Online::ICommerce>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!Commerce.IsValid())
	{
		AnnounceCheckout(FString { }, MissingFeature(), OnComplete);

		return false;
	}

	if (!Account.IsValid())
	{
		AnnounceCheckout(FString { }, NotSignedIn(), OnComplete);

		return false;
	}

	if (Lines.IsEmpty())
	{
		UE_LOG(LogModularOnline, Warning, TEXT("A checkout was asked for with nothing in it."));

		AnnounceCheckout(FString { }, FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidParams()), OnComplete);

		return false;
	}

	EnsureListening();

	UE::Online::FCommerceCheckout::Params Params;
	Params.LocalAccountId = Account;
	Params.Offers.Reserve(Lines.Num());

	for (const auto& Line : Lines)
	{
		Params.Offers.Add(UE::Online::FPurchaseOffer { Line.OfferId, FMath::Max(1, Line.Quantity) });
	}

	Commerce->Checkout(MoveTemp(Params)).OnComplete(this, [this, OnComplete](const UE::Online::TOnlineResult<UE::Online::FCommerceCheckout>& Result)
	{
		if (Result.IsError())
		{
			AnnounceCheckout(FString { }, FModularOnlineResult::FromOnlineError(Result.GetErrorValue()), OnComplete);

			return;
		}

		// Not every store hands back a transaction; the ones that do not report the purchase on the event.
		AnnounceCheckout(Result.GetOkValue().TransactionId.Get(FString { }), FModularOnlineResult::Success(), OnComplete);
	});

	return true;
}

bool UModularStoreSubsystem::QueryEntitlements(const int32 LocalPlayerIndex, const bool bIncludeRedeemed, FModularEntitlementsDelegate OnComplete)
{
	const auto Commerce = GetInterface<UE::Online::ICommerce>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!Commerce.IsValid())
	{
		AnnounceEntitlements(TArray<FModularEntitlement> { }, MissingFeature(), OnComplete);

		return false;
	}

	if (!Account.IsValid())
	{
		AnnounceEntitlements(TArray<FModularEntitlement> { }, NotSignedIn(), OnComplete);

		return false;
	}

	EnsureListening();

	UE::Online::FCommerceQueryEntitlements::Params Params;
	Params.LocalAccountId = Account;
	Params.bIncludeRedeemed = bIncludeRedeemed;

	Commerce->QueryEntitlements(MoveTemp(Params)).OnComplete(this, [this, LocalPlayerIndex, OnComplete](const UE::Online::TOnlineResult<UE::Online::FCommerceQueryEntitlements>& Result)
	{
		if (Result.IsError())
		{
			AnnounceEntitlements(TArray<FModularEntitlement> { }, FModularOnlineResult::FromOnlineError(Result.GetErrorValue()), OnComplete);

			return;
		}

		// As with offers, the query answers with nothing and fills the cache it is read back from.
		TArray<FModularEntitlement> Entitlements;
		GetEntitlements(LocalPlayerIndex, Entitlements);

		AnnounceEntitlements(Entitlements, FModularOnlineResult::Success(), OnComplete);
	});

	return true;
}

bool UModularStoreSubsystem::GetEntitlements(const int32 LocalPlayerIndex, TArray<FModularEntitlement>& OutEntitlements) const
{
	const auto Commerce = GetInterface<UE::Online::ICommerce>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!Commerce.IsValid() || !Account.IsValid())
	{
		return false;
	}

	UE::Online::FCommerceGetEntitlements::Params Params;
	Params.LocalAccountId = Account;

	const auto Cached = Commerce->GetEntitlements(MoveTemp(Params));
	if (Cached.IsError())
	{
		return false;
	}

	OutEntitlements.Reset();
	OutEntitlements.Reserve(Cached.GetOkValue().Entitlements.Num());

	for (const auto& Entitlement : Cached.GetOkValue().Entitlements)
	{
		OutEntitlements.Add(PoFigGames::Online::Private::DescribeEntitlement(Entitlement));
	}

	return true;
}

bool UModularStoreSubsystem::RedeemEntitlement(const int32 LocalPlayerIndex, const FString& EntitlementId, const int32 Quantity, FModularStoreOperationDelegate OnComplete)
{
	const auto Commerce = GetInterface<UE::Online::ICommerce>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!Commerce.IsValid())
	{
		AnnounceRedeemed(MissingFeature(), OnComplete);

		return false;
	}

	if (!Account.IsValid())
	{
		AnnounceRedeemed(NotSignedIn(), OnComplete);

		return false;
	}

	UE::Online::FCommerceRedeemEntitlement::Params Params;
	Params.LocalAccountId = Account;
	Params.EntitlementId = EntitlementId;
	Params.Quantity = FMath::Max(1, Quantity);

	Commerce->RedeemEntitlement(MoveTemp(Params)).OnComplete(this, [this, OnComplete](const UE::Online::TOnlineResult<UE::Online::FCommerceRedeemEntitlement>& Result)
	{
		AnnounceRedeemed(Result.IsError() ? FModularOnlineResult::FromOnlineError(Result.GetErrorValue()) : FModularOnlineResult::Success(), OnComplete);
	});

	return true;
}

bool UModularStoreSubsystem::ShowStoreUI(const int32 LocalPlayerIndex)
{
	const auto Commerce = GetInterface<UE::Online::ICommerce>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!Commerce.IsValid() || !Account.IsValid())
	{
		UE_LOG(LogModularOnline, Verbose, TEXT("The store of the platform was not opened for player %d."), LocalPlayerIndex);

		return false;
	}

	UE::Online::FCommerceShowStoreUI::Params Params;
	Params.LocalAccountId = Account;

	// The answer is nobody's to wait for, but a refusal that nothing reports is a write that
	// silently did not happen.
	Commerce->ShowStoreUI(MoveTemp(Params)).OnComplete(this, [this](const UE::Online::TOnlineResult<UE::Online::FCommerceShowStoreUI>& Result)
	{
		UE_CLOG(Result.IsError(), LogModularOnline, Warning, TEXT("The services refused to open the store: %s"), *Result.GetErrorValue().GetLogString());
	});

	return true;
}
